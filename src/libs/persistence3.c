/*
 *
 * Plan:
 * - leveled compaction a la rocksdb
 *   - each segment fixed max size in all levels
 *   - each segment contains SSTable
 *   - SSTable format:
 *      - stores index (sparse)
 *      - stores data, but compressed blocks; for each key, the value is a
 * compressed block of values of all other keys until the next key
 *  - each segment contains a RANGE, non-overlapping (per level)
 *  - fixed number of max levels
 *  - [opt] bloom filter
 *
 *
 * - functions:
 *   - memtable_dump: dumps memtable to SSTable format
 *   - merge_and_compact: takes list of segments (files) and merges and compacts
 * them
 *      - needs to also have "output" level for segment; also way to
 * delete/expire segments that were merged
 *   - search_in_segment:
 *      - loads index in memory
 *
 * - serialization formats: OPEN DISCUSSION
 *   - e.g. how to identify where each block starts and ends, e.g. the data
 * block, the index block, etc
 *      - idea: store [block_len][tag][block]
 *          - tag can be: pair (for actual k-v pairs), index, data, key (for key
 * in k-v pair), value, tombstone, etc.
 *          - let's allow for 16 tags, so a short (4 bytes)
 *
 *
 *
 */

#include "bytering.h"
#include "lstr.h"
#include "lz4.h"
#include "skiplist_str.h"
#include "u_array.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
const u32 kTagSize = 2; // bytes
uint16_t kKeyTag = 0;
uint16_t kValueTag = 1;
uint16_t kPairTag = 2;
uint16_t kIndexTag = 3;
uint16_t kDataTag = 4;
uint16_t kCompressedBlockTag = 5;

struct Segment {
  int fd;
  char *filename;
};
int ceil_div(int a, int b) {
  if (a == 0)
    return 0;
  return 1 + ((a - 1) / b);
}

struct Store {
    // serves requests, tier 1
    SkipList *active_memtable;
    // these also serve requests, tier 2
    // TODO: make it a queue
    // TODO: async thread dumps these to SSTable
    // once dump is done, pop from here
    Array *inactive_memtables;
    // these also serve requests, tier 3
    Array *sstables;
};

struct KeyOffsetPair {
  LString *key;
  u32 offset;
};

size_t serialize_skip_list(SkipList *sl, char *buf) {
  LString **keys;
  LString **values;
  u32 out_count;
  sl_s_get_data(sl, &keys, &values, &out_count);

  int idx_struct_size = sizeof(struct KeyOffsetPair);
  Array *offset_sparse_index = array_sized_new(out_count, idx_struct_size);

  char *start = buf;
  u32 batch_size = 1 << 6; // sparse index
  // write the data and build the index
  for (u32 i = 0; i < out_count; i++) {
    if (i % batch_size == 0) {
      array_push(offset_sparse_index, &(struct KeyOffsetPair){.key = keys[i], .offset = (u32)(buf - start)});
    }
    memcpy(buf, &kKeyTag, kTagSize);
    buf += kTagSize;

    memcpy(buf, &keys[i]->len, kLStringLenSize);
    buf += kLStringLenSize;

    memcpy(buf, keys[i]->data, keys[i]->len);
    printf("DATA[i]->data: %s\n", keys[i]->data);
    buf += keys[i]->len;

    memcpy(buf, &kValueTag, kTagSize);
    buf += kTagSize;

    memcpy(buf, &values[i]->len, kLStringLenSize);
    buf += kLStringLenSize;

    memcpy(buf, values[i]->data, values[i]->len);
    printf("values[i]->data: %s\n", values[i]->data);
    buf += values[i]->len;
  }

  // write the index
  for (u32 i = 0; i < ceil_div(out_count, batch_size); i++) {
    printf("idx-write-loop iter %d\n", i);

    LString **key = &(array_index(offset_sparse_index, struct KeyOffsetPair, i).key);
    printf("key=%p\n", key);
    memcpy(buf, (*key)->data, (*key)->len);
    buf += (*key)->len;

    u32 *offset = &(array_index(offset_sparse_index, struct KeyOffsetPair, i).offset);
    memcpy(buf, offset, sizeof(u32));
    buf += sizeof(u32);
  }

  return buf - start;
}

#include <ctype.h>

// Helper to inspect the raw bytes of the serialized output
void debug_dump_buffer(const char *label, const void *data, size_t size) {
  const unsigned char *p = (const unsigned char *)data;
  printf("\n--- %s (%zu bytes) ---\n", label, size);
  for (size_t i = 0; i < size; i += 16) {
    printf("%04zx: ", i);
    // Print hex
    for (int j = 0; j < 16; j++) {
      if (i + j < size)
        printf("%02x ", p[i + j]);
      else
        printf("   ");
    }
    printf(" | ");
    // Print printable chars
    for (int j = 0; j < 16; j++) {
      if (i + j < size) {
        printf("%c", isprint(p[i + j]) ? p[i + j] : '.');
      }
    }
    printf("\n");
  }
  printf("--------------------------------\n");
}

int main() {
  srand(time(0));

  // 1. Setup SkipList
  SkipList *sl = malloc(sizeof(SkipList));
  Node *nmin = malloc(sizeof(Node) + 3 * sizeof(Node *));
  nmin->key = lstring_create(0);
  nmin->level = 2;
  for (int i = 0; i < 3; i++)
    nmin->next[i] = NULL;
  sl->head = nmin;
  sl->num_levels = 3;

  // 2. Insert Test Data (using distinct values to make them easy to find in
  // hex)
  printf("Populating Memtable...\n");
  LString *key_a = lstring_create_from_buf(5, "KEYA1");
  LString *value_a = lstring_create_from_buf(7, "value_1");
  LString *value_b = lstring_create_from_buf(7, "value_Z");
  sl_s_insert(sl, key_a, value_a);
  sl_s_insert(sl, lstring_create_from_buf(5, "KEYZx"), value_b);

  // 3. Serialize
  char buffer[2048];
  memset(buffer, 0, 2048);

  // --- After inserting data into 'sl' ---

  printf("\n=== Debugging sl_s_get_data Output ===\n");

  LString **debug_keys;
  LString **debug_values;
  u32 count;

  // Call the function we want to inspect
  sl_s_get_data(sl, &debug_keys, &debug_values, &count);
  printf("debug_keys[0]=%p\n", debug_keys[0]->data);
  printf("debug_keys[0].len=%d\n", debug_keys[0]->len);
  printf("debug_keys[1]=%p\n", debug_keys[1]->data);
  printf("key_a=%p\n", key_a);

  printf("Total elements returned: %u\n", count);
  printf("%-5s | %-10s | %-20s | %s\n", "Idx", "Len", "Hex Content", "String");
  printf("---------------------------------------------------------------\n");

  for (u32 i = 0; i < count; i++) {
    printf("[%u] ", i);

    // Check for NULL data to prevent segfaults
    if (debug_keys[i]->data == NULL) {
      printf("DATA IS NULL\n");
      continue;
    }

    // %.*s takes the length (debug_keys[i].len) as the first arg
    // and the char pointer as the second.
    printf("Len: %-4u | Data: \"%.*s\" | Value: %.*s\n", debug_keys[i]->len,
           (int)debug_keys[i]->len, debug_keys[i]->data, debug_values[i]->len,
           debug_values[i]->data);
  }

  // Clean up the arrays allocated by sl_s_get_data
  free(debug_keys);
  free(debug_values);

  printf("======================================\n\n");
  size_t written = serialize_skip_list(sl, buffer);

  // 4. Debug Output
  if (written > 0) {
    debug_dump_buffer("SSTable Sparse Index Serialized Data", buffer,
                      (size_t)written);
  } else {
    printf("No data written (check if batch_size is skipping your small "
           "dataset)\n");
  }

  return 0;
}
