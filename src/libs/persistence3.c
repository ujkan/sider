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
#include "persistence.h"
#include "skiplist_str.h"
#include "u_array.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
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

struct KeyOffsetPair {
  LString *key;
  u32 offset;
};

SSTable *dump_memtable_to_sst(SkipList *mt, LString *filepath) {
  SSTable *sst = malloc(sizeof(SSTable *));
  sst->filepath = filepath;

  char *fp;
  fp = lstring_to_cstr(filepath); // 0-terminated means can copy 127 chars max
  FILE *fptr = fopen(fp, "a");
  free(fp);

  if (fptr == NULL) {
    return NULL;
  }
  sst->fd = fileno(fptr);

  LString **keys;
  LString **values;
  u32 out_count;
  sl_s_get_data(mt, &keys, &values, &out_count);

  if (out_count == 0) {
    return 0;
  }

  // Record the starting position to calculate total bytes written at the end
  long start_pos = ftell(fptr);

  int idx_struct_size = sizeof(struct KeyOffsetPair);
  Array *offset_sparse_index = array_sized_new(out_count, idx_struct_size);

  u32 batch_size = 1 << 3;

  u32 index_size = 0;
  u32 index_offset = 0;
  u32 kFooterSize = 4 + 4; // 4 bytes for size; 4 for offset

  // --- Write the data and build the index ---
  for (u32 i = 0; i < out_count; i++) {
    if (i % batch_size == 0) {
      u32 current_offset = (u32)(ftell(fptr) - start_pos);
      array_push(
          offset_sparse_index,
          &(struct KeyOffsetPair){.key = keys[i], .offset = current_offset});
    }

    fwrite(&kPairTag, kTagSize, 1, fptr);

    fwrite(&keys[i]->len, kLStringLenSize, 1, fptr);
    fwrite(keys[i]->data, 1, keys[i]->len, fptr);

    fwrite(&values[i]->len, kLStringLenSize, 1, fptr);
    fwrite(values[i]->data, 1, values[i]->len, fptr);
  }

  index_offset = ftell(fptr);

  // --- Write the index ---
  u32 index_count = (out_count - 1) / batch_size + 1;
  for (u32 i = 0; i < index_count; i++) {
    struct KeyOffsetPair *pair =
        &array_index(offset_sparse_index, struct KeyOffsetPair, i);

    fwrite(&kKeyTag, kTagSize, 1, fptr);
    fwrite(&keys[i]->len, kLStringLenSize, 1, fptr);
    fwrite(pair->key->data, 1, pair->key->len, fptr);
    fwrite(&pair->offset, sizeof(u32), 1, fptr);
    index_size += kTagSize + kLStringLenSize + pair->key->len + sizeof(u32);
  }

  // write footer
  printf("index_offset: %d\n", index_offset);
  printf("index_size: %d\n", index_size);
  fwrite(&index_offset, sizeof(index_offset), 1, fptr);
  fwrite(&index_size, sizeof(index_size), 1, fptr);

  // Return total bytes written
  sst->size = (u32)(ftell(fptr) - start_pos);
  fclose(fptr); // This is mandatory to see the data on disk
  return sst;
}

// tombstone will be
// [key] [len] [value]
// "key" 0     """
size_t serialize_skip_list(SkipList *mt, char *buf) {
  LString **keys;
  LString **values;
  u32 out_count;
  sl_s_get_data(mt, &keys, &values, &out_count);

  if (out_count == 0)
    return 0;

  int idx_struct_size = sizeof(struct KeyOffsetPair);
  Array *offset_sparse_index = array_sized_new(out_count, idx_struct_size);

  char *start = buf;
  u32 batch_size = 1 << 6; // sparse index
  // write the data and build the index
  for (u32 i = 0; i < out_count; i++) {
    if (i % batch_size == 0) {
      array_push(offset_sparse_index,
                 &(struct KeyOffsetPair){.key = keys[i],
                                         .offset = (u32)(buf - start)});
    }
    memcpy(buf, &kPairTag, kTagSize);
    buf += kTagSize;

    memcpy(buf, &keys[i]->len, kLStringLenSize);
    buf += kLStringLenSize;

    memcpy(buf, keys[i]->data, keys[i]->len);
    printf("DATA[i]->data: %s\n", keys[i]->data);
    buf += keys[i]->len;

    memcpy(buf, &values[i]->len, kLStringLenSize);
    buf += kLStringLenSize;

    memcpy(buf, values[i]->data, values[i]->len);
    printf("values[i]->data: %s\n", values[i]->data);
    buf += values[i]->len;
  }

  // write the index
  for (u32 i = 0; i < (out_count - 1) / batch_size + 1; i++) {
    printf("idx-write-loop iter %d\n", i);

    LString **key =
        &(array_index(offset_sparse_index, struct KeyOffsetPair, i).key);
    printf("key=%p\n", key);
    memcpy(buf, (*key)->data, (*key)->len);
    buf += (*key)->len;

    u32 *offset =
        &(array_index(offset_sparse_index, struct KeyOffsetPair, i).offset);
    memcpy(buf, offset, sizeof(u32));
    buf += sizeof(u32);
  }

  return buf - start;
}
void hex_dump(const char *desc, const void *addr, int len) {
  int i;
  unsigned char buff[17];
  const unsigned char *pc = (const unsigned char *)addr;

  // Output description if provided
  if (desc != NULL)
    printf("%s:\n", desc);

  for (i = 0; i < len; i++) {
    // Print offset at the start of every line
    if ((i % 16) == 0) {
      if (i != 0)
        printf("  %s\n", buff);
      printf("  %04x ", i);
    }

    // Print hex value
    printf(" %02x", pc[i]);

    // Store printable character for the right side
    if ((pc[i] < 0x20) || (pc[i] > 0x7e)) {
      buff[i % 16] = '.';
    } else {
      buff[i % 16] = pc[i];
    }
    buff[(i % 16) + 1] = '\0';
  }

  // Pad out last line if not exactly 16 characters
  while ((i % 16) != 0) {
    printf("   ");
    i++;
  }

  // Final print of the ASCII buffer
  printf("  %s\n", buff);
}

void deserialize_index(char *index, int len, Array *keys_out,
                       Array *offsets_out) {
  char buf[8];
  char *curr = index;
  while ((curr - index) < len) {
    curr += kTagSize;
    LString key = {0};

    memcpy(&(key.len), curr, kLStringLenSize);
    curr += kLStringLenSize;
    key.data = malloc(key.len);
    memcpy(key.data, curr, key.len);
    curr += key.len;
    array_push(keys_out, &key);

    u32 offset;
    memcpy(&offset, curr, sizeof(offset));
    curr += sizeof(offset);
    array_push(offsets_out, &offset);
  }
}

LString *search_in_sst(SSTable sst, LString *key) {
  char *fp;
  fp = lstring_to_cstr(
      sst.filepath); // 0-terminated means can copy 127 chars max
  FILE *fptr = fopen(fp, "r");

  struct stat st;
  fstat(fileno(fptr), &st);

  fseek(fptr, st.st_size - 8, SEEK_SET);
  u32 index_offset;
  u32 index_size;
  fread(&index_offset, 1, sizeof(index_offset), fptr);
  fread(&index_size, 1, sizeof(index_size), fptr);
  fseek(fptr, index_offset, SEEK_SET);

  char *index = malloc(index_size);
  fread(index, index_size, 1, fptr);

  hex_dump("index", index, index_size);
  Array *keys = array_sized_new(32, sizeof(LString));
  Array *offsets = array_sized_new(32, sizeof(u32));

  deserialize_index(index, index_size, keys, offsets);

  int low = 0;
  int high = keys->len - 1;
  int mid;
  int offset = -1;
  while (low <= high) {
    mid = (high + low) / 2;
    int cmp_val = lstring_compare(key, &array_index(keys, LString, mid));
    if (cmp_val == 0) {
      offset = array_index(offsets, u32, mid);
      break;
    } else if (cmp_val > 0) {
      low = mid + 1;
    } else {
      high = mid - 1;
    }
  }
  if (offset != -1) {
    fseek(fptr, offset, SEEK_SET);
    char buf[8];
    fread(buf, 1, kTagSize + kLStringLenSize, fptr);
    if (memcmp(buf, &kPairTag, kTagSize) == 0) {
      u16 key_len;
      memcpy(&(key_len), buf + kTagSize, kLStringLenSize);
      fseek(fptr, key_len, SEEK_CUR);
      LString *value = malloc(sizeof(LString *));
      fread(&value->len, 1, kLStringLenSize, fptr);
      value->data = malloc(value->len);
      fread(value->data, 1, value->len, fptr);
      return value;
    }
  }
  return NULL;
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

// int main() {
//   srand(time(0));
//
//   // 1. Setup SkipList
//   SkipList *sl = malloc(sizeof(SkipList));
//   Node *nmin = malloc(sizeof(Node) + 3 * sizeof(Node *));
//   nmin->key = lstring_create(0);
//   nmin->level = 2;
//   for (int i = 0; i < 3; i++)
//     nmin->next[i] = NULL;
//   sl->head = nmin;
//   sl->num_levels = 3;
//
//   // 2. Insert Test Data (using distinct values to make them easy to find in
//   // hex)
//   printf("Populating Memtable...\n");
//   LString *key_a = lstring_create_from_buf(5, "KEYA1");
//   LString *value_a = lstring_create_from_buf(7, "value_1");
//   LString *value_b = lstring_create_from_buf(7, "value_Z");
//   sl_s_insert(sl, key_a, value_a);
//   sl_s_insert(sl, lstring_create_from_buf(5, "KEYZx"), value_b);
//
//   // 3. Serialize
//   char buffer[2048];
//   memset(buffer, 0, 2048);
//
//   // --- After inserting data into 'sl' ---
//
//   printf("\n=== Debugging sl_s_get_data Output ===\n");
//
//   LString **debug_keys;
//   LString **debug_values;
//   u32 count;
//
//   // Call the function we want to inspect
//   sl_s_get_data(sl, &debug_keys, &debug_values, &count);
//   printf("debug_keys[0]=%p\n", debug_keys[0]->data);
//   printf("debug_keys[0].len=%d\n", debug_keys[0]->len);
//   printf("debug_keys[1]=%p\n", debug_keys[1]->data);
//   printf("key_a=%p\n", key_a);
//
//   printf("Total elements returned: %u\n", count);
//   printf("%-5s | %-10s | %-20s | %s\n", "Idx", "Len", "Hex Content",
//   "String");
//   printf("---------------------------------------------------------------\n");
//
//   for (u32 i = 0; i < count; i++) {
//     printf("[%u] ", i);
//
//     // Check for NULL data to prevent segfaults
//     if (debug_keys[i]->data == NULL) {
//       printf("DATA IS NULL\n");
//       continue;
//     }
//
//     // %.*s takes the length (debug_keys[i].len) as the first arg
//     // and the char pointer as the second.
//     printf("Len: %-4u | Data: \"%.*s\" | Value: %.*s\n", debug_keys[i]->len,
//            (int)debug_keys[i]->len, debug_keys[i]->data,
//            debug_values[i]->len, debug_values[i]->data);
//   }
//
//   // Clean up the arrays allocated by sl_s_get_data
//   free(debug_keys);
//   free(debug_values);
//
//   printf("======================================\n\n");
//   size_t written = serialize_skip_list(sl, buffer);
//
//   // 4. Debug Output
//   if (written > 0) {
//     debug_dump_buffer("SSTable Sparse Index Serialized Data", buffer,
//                       (size_t)written);
//   } else {
//     printf("No data written (check if batch_size is skipping your small "
//            "dataset)\n");
//   }
//
//   return 0;
// }
