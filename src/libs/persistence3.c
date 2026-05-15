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

#include "block.h"
#include "block_item.h"
#include "bytering.h"
#include "hex_dump.h"
#include "index_block.h"
#include "lstr.h"
#include "persistence.h"
#include "scribe.h"
#include "skiplist_str.h"
#include "stb_ds.h"
#include "u_array.h"
#include <errno.h>
#include <fcntl.h>
#include <lz4.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
const u32 kTagSize = 2; // bytes
u16 kKeyTag = 0;
u16 kValueTag = 1;
u16 kPairTag = 2;
u16 kIndexTag = 3;
u16 kDataTag = 4;
u16 kCompressedBlockTag = 5;
u32 kBatchSize = 1 << 3;
__thread char *t_block_buf = NULL;
__thread char *t_comp_buf = NULL;

static void cleanup_char_buf(char **ptr) {
  //   printf("FREEINGBUFFER\n");
  free(*ptr);
}

static void cleanup_file(FILE **ptr) {
  //   printf("FREEINGBUFFER\n");
  fclose(*ptr);
}

static void cleanup_LString(LString *lstr) {
  //   printf("FREEINGBUFFER\n");
  lstring_free(lstr);
}
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
  LString key;
  u32 offset;
};

void KeyOffsetPair_free(void *ptr) {
  lstring_free(&((struct KeyOffsetPair *)ptr)->key);
}

struct SSTPair {
  u16 tag;
  LString key;
  u8 tombstone;
  LString value;
};

u32 SSTPair_get_size(struct SSTPair *pair) {
  return kTagSize + kLStringLenSize + pair->key.len + kLStringLenSize +
         pair->value.len;
}

void SSTPair_free(void *ptr) {
  free(((struct SSTPair *)ptr)->key.data);
  free(((struct SSTPair *)ptr)->value.data);
}

void SSTPair_deserialize(struct SSTPair *pair, char **cursor) {
  u8 *dst = (u8 *)*cursor;
  scribe_put_u16(&dst, pair->tag);
  scribe_put_u16(&dst, pair->key.len);
  scribe_put_bytes(&dst, pair->key.data, pair->key.len);
  scribe_put_u8(&dst, pair->tombstone);
  scribe_put_u16(&dst, pair->value.len);
  scribe_put_bytes(&dst, pair->value.data, pair->value.len);
  *cursor = (char *)dst;
}

void KeyOffsetPair_deserialize(struct KeyOffsetPair *pair, char **cursor) {
  const u8 *src = (const u8 *)*cursor;
  src += 2; // TODO: kTagSize
  pair->key.len = scribe_get_u16(&src);
  pair->key.data = malloc(pair->key.len);
  scribe_get_bytes(&src, pair->key.data, pair->key.len);
  pair->offset = scribe_get_u32(&src);
  *cursor = (char *)src;
}

u32 KeyOffsetPair_serialize(struct KeyOffsetPair *pair, FILE *cursor) {
  u8 buf[2 + sizeof(pair->key.len) + pair->key.len + sizeof(pair->offset)];
  u8 *dst = buf;
  scribe_put_u16(&dst, kKeyTag);
  scribe_put_u16(&dst, pair->key.len);
  scribe_put_bytes(&dst, pair->key.data, pair->key.len);
  scribe_put_u32(&dst, pair->offset);
  fwrite(buf, 1, (size_t)(dst - buf), cursor);
  return kTagSize + kLStringLenSize + pair->key.len + sizeof(u32);
}
#define MAX_BLOCK_SIZE (320 * 1024)
#define MAX_COMP_SIZE (MAX_BLOCK_SIZE + (MAX_BLOCK_SIZE / 255) + 16)

void compress_and_write_v3(Array *sst_pairs, int data_len, char *write_buf) {
  // convert sst_pairs[0] to BlockEntry
  // set prev = sst_pairs[0].key
  // for i=1 to pairs.len
  //   curr = pairs[i]
  //   shared = find_shared_prefix(curr, prev)
  //   suffix_len = curr.len - shared
  //   blockItem = {
  //     shared,
  //     suffix_len,
  //     curr.key[shared:],
  //     curr.value.len,
  //     curr.value
  //   }
  //   out.append(blockItem.serialize())
  //
  // that's without the restart points
  // let's say we choose every 16 keys to restart
  // then in loop, add check
  // if i % 16 == 0 and i > 0
  //   blockItem = {
  //     0,
  //     curr.key.len
  //     curr.key,
  //     curr.value.len,
  //     curr.value
  //   }
  //   restartPoint = {
  //     curr.key,
  //     out.cursor
  //   }
  //   restartPoints.append(restartPoint)
  //
}

void compress_and_write_v2(Array *sst_pairs, int data_len, char *write_buf,
                           int *len) {

  struct SSTPair curr = array_index(sst_pairs, struct SSTPair, 0);
  struct DataBlock *block = malloc(sizeof(struct DataBlock));
  arrsetcap(block->blocks, 128);
  struct DataBlockSingle curr_block_single = {0};
  DataBlockSingle_init(&curr_block_single);
  // NOTE: curr_block_single points to BlockItem arr
  // that block must live beyond what happens to curr_block_single, which
  // is a transient variable. since we store the block BY copy in
  // block->blocks, it means those pointers get copied, and thus a handle
  // to them exists via block->blocks[i]
  arrpush(block->blocks, curr_block_single);

  DataBlockSingle_append_entry(&curr_block_single, &curr.key, &curr.value,
                               NULL);
  // LString *b_item_serialized = BlockItem_serialize(&b_item);
  // memcpy(cursor, b_item_serialized->data, b_item_serialized->len);
  // cursor += b_item_serialized->len;
  // // free after copy
  // lstring_free(b_item_serialized);

  // TODO: for nicer programming experience, it is probably better in code to
  // convert between representations, i.e. convert SSTPair to some new struct
  // like BlockEntry and then rely on special methods to serialize it
  // that way you write the code once, test it, make sure it works right
  // and rely on simpler code to map from SSTPair to BlockEntry!
  // these are just data transformations, mappings! should be much simpler to
  // code
  LString *prev = &curr.key;
  // TODO: should we copy curr.key here or just "borrow" it or get its ref?
  struct IndexItem curr_index_item = {.key = &curr.key, .offset = 0};
  struct IndexBlock i_block = {0};
  arrsetcap(i_block.items, 128);
  arrpush(i_block.items, curr_index_item);
  for (u32 j = 1; j < sst_pairs->len; j++) {
    // curr = array_index(sst_pairs, struct SSTPair, j);
    // printf("CURR->KEY: %.*s\n", curr.key.len, curr.key.data);
    if (j % 128 == 0) {
      // it is  crucial that _init sets the .items and .restart_points fields
      // to NULL and thus arrsetcap CREATES new arrays (mallocs them)
      // recall that the reference to the old block's items and rps remains
      // when we append to block->blocks BY VALUE ! so the pointers are copied
      // and thus live, but curr_block gets refreshed!
      // essentially what we want each time is something like
      // curr_block_single = new DataBlockSingle();
      // where the constructor of that allocates the arrays (NEW ones)
      // appropriately
      // i.e. think of DataBlockSingle_init as
      // public void DataBlockSingle() {
      //   this.items = new ArrayList<BlockItem>();
      //   this.restartPoints = new ArrayList<RestartPoint>();
      // }
      // curr_block_single = new DataBlockSingle();
      // block.blocks.append(curr_block_single);
      // ...
      // curr_block_single.items.append(b_item);
      // ---
      // since curr_block_single is an Object, we can append to it and
      // block.blocks.append retains it
      // of course oen can argue that we should append once we're done
      // attending for more "idiomatic" usage!
      // maybe first append, and then initialize the new current block
      // that would work if we do the following:
      // (1) swap the order of _init and arrpush below
      // (2) remove the arrpush before the loop
      // (3) add a final arrpush AFTER the loop to cover the final block
      // it depends on if we want:
      // 1. push block before and then fill it with data
      // 2. fill it with data then push
      DataBlockSingle_init(&curr_block_single);
      arrpush(block->blocks, curr_block_single);
    }
    curr = array_index(sst_pairs, struct SSTPair, j);
    prev = &array_index(sst_pairs, struct SSTPair, j - 1).key;
    DataBlockSingle_append_entry(&curr_block_single, &curr.key, &curr.value,
                                 prev);
    if (j % 128 == 0) {
      curr_index_item.key = &curr.key;
      arrpush(i_block.items, curr_index_item);
    }
    // b_item_serialized = BlockItem_serialize(&b_item);
    // // TODO: yes not ideal that we first memcpy into an LString and
    // // then memcpy into cursor
    // // would be nice to have a serialize into a char* directly
    // memcpy(cursor, b_item_serialized->data, b_item_serialized->len);
    // cursor += b_item_serialized->len;
    // // free after copy
    // lstring_free(b_item_serialized);
  }

  LString *compressed_block = DataBlock_compress(block);
  LString *index_block_serialized = IndexBlock_serialize(&i_block);
  // TODO: use slice/bytebuf instead of write_buf
  printf("compressed->block->len: %d\n", compressed_block->len);
  u8 *cursor = (u8 *)write_buf;
  scribe_put_bytes(&cursor, compressed_block->data, compressed_block->len);
  *len += compressed_block->len;
  scribe_put_bytes(&cursor, index_block_serialized->data,
                   index_block_serialized->len);
  *len += index_block_serialized->len;
  array_set_length(sst_pairs, 0);
}

SSTable *dump_memtable_to_sst_v2(SkipList *mt, LString *filepath) {
  pthread_t thread_id = pthread_self();

  SSTable *sst = malloc(sizeof(SSTable));
  sst->filepath = filepath;

  char *fp __attribute__((__cleanup__(cleanup_char_buf)));
  fp = lstring_to_cstr(filepath); // 0-terminated means can copy 127 chars max
  int fd;
  fd = open(fp, O_RDWR | O_CREAT | O_TRUNC, 0644);
  size_t size = 1024 * 1024 * 128;
  if (ftruncate(fd, size) == -1) {
    perror("Error setting file size");
    return NULL;
  }

  char *map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  LString **keys;
  LString **values;
  u32 out_count;
  sl_s_get_data(mt, &keys, &values, &out_count);
  if (out_count == 0) {
    free(sst);
    free(keys);
    free(values);
    return 0;
  }
  // sst_pairs_init(keys, values);
  //
  Array *sst_pairs = array_sized_new(out_count, sizeof(struct SSTPair));
  for (int i = 0; i < out_count; i++) {
    struct SSTPair sst_pair = {0};
    sst_pair.tag = kPairTag;
    sst_pair.key = *keys[i];
    sst_pair.value = *values[i];
    array_push(sst_pairs, &sst_pair);
  }
  int written_len;
  compress_and_write_v2(sst_pairs, 0, map, &written_len);
  if (msync(map, size, MS_SYNC) == -1) {
    perror("Could not sync to disk");
  }
  if (munmap(map, size) == -1) {
    perror("Error unmapping");
  }
  if (ftruncate(fd, written_len) == -1) {
    perror("ftruncate failed");
  }
  close(fd);
  sst->fd = fd;
  return sst;
}

// SSTable *dump_memtable_to_sst(SkipList *mt, LString *filepath) {
//   pthread_t thread_id = pthread_self();
//
//   SSTable *sst = malloc(sizeof(SSTable));
//   sst->filepath = filepath;
//
//   char *fp __attribute__((__cleanup__(cleanup_char_buf)));
//   fp = lstring_to_cstr(filepath); // 0-terminated means can copy 127 chars
//   max FILE *fptr __attribute__((__cleanup__(cleanup_file))); fptr = fopen(fp,
//   "a");
//
//   if (fptr == NULL) {
//     return NULL;
//   }
//   sst->fd = fileno(fptr);
//
//   LString **keys;
//   LString **values;
//   u32 out_count;
//   sl_s_get_data(mt, &keys, &values, &out_count);
//
//   if (out_count == 0) {
//     free(sst);
//     free(keys);
//     free(values);
//     return 0;
//   }
//
//   // Record the starting position to calculate total bytes written at the
//   // end
//   long start_pos = ftell(fptr);
//
//   int idx_struct_size = sizeof(struct KeyOffsetPair);
//   Array *offset_sparse_index = array_sized_new(out_count, idx_struct_size);
//
//   u32 index_size = 0;
//   u32 index_offset = 0;
//   u32 kFooterSize = 4 + 4; // 4 bytes for size; 4 for offset
//   int data_len = 0;
//
//   Array *data_blocks = array_sized_new(
//       kBatchSize,
//       sizeof(struct SSTPair)); // k_max_key_len + k_max_value_len (upper
//       bound
//                                // for block)
//   // --- Write the data and build the index ---
//   for (u32 i = 0; i < out_count; i++) {
//
//     if (i > 0 && i % kBatchSize == 0) {
//       compress_and_write(data_blocks, data_len, fptr);
//       data_len = 0;
//     }
//     if (i % kBatchSize == 0) {
//       u32 current_offset = (u32)(ftell(fptr) - start_pos);
//       array_push(
//           offset_sparse_index,
//           &(struct KeyOffsetPair){.key = *keys[i], .offset =
//           current_offset});
//     }
//     LString *key = keys[i];
//     LString *value = values[i];
//     struct SSTPair tmpdata = {0};
//     tmpdata.tag = kPairTag;
//     tmpdata.key = *key;
//     tmpdata.value = *value;
//     array_push(data_blocks, &tmpdata);
//     data_len += SSTPair_get_size(&tmpdata);
//     //     printf("DATA_LEN: %d\n", data_len);
//   }
//   compress_and_write(data_blocks, data_len, fptr);
//   free(data_blocks->data);
//   free(data_blocks);
//
//   index_offset = ftell(fptr);
//
//   // --- Write the index ---
//   u32 index_count = (out_count - 1) / kBatchSize + 1;
//   printf("Writing index...\n");
//   for (u32 i = 0; i < index_count; i++) {
//     struct KeyOffsetPair *pair =
//         &array_index(offset_sparse_index, struct KeyOffsetPair, i);
//
//     printf("index[%d]: key len=%d data=%.*s offset=%d\n", i, pair->key.len,
//            pair->key.len, pair->key.data, pair->offset);
//     index_size += KeyOffsetPair_serialize(pair, fptr);
//   }
//
//   // write footer
//   //   printf("index_offset: %d\n", index_offset);
//   //   printf("index_size: %d\n", index_size);
//   fwrite(&index_offset, sizeof(index_offset), 1, fptr);
//   fwrite(&index_size, sizeof(index_size), 1, fptr);
//
//   // Return total bytes written
//   sst->size = (u32)(ftell(fptr) - start_pos);
//   // printf("[THREAD %lu] dump_memtable_to_sst: Successfully wrote %u "
//   //        "bytes to '%s', closing file\n",
//   //        thread_id, sst->size, fp);
//   // printf("[THREAD %lu] dump_memtable_to_sst: Returning SST with "
//   //        "filepath = '%s'\n ",
//   //        thread_id, sst->filepath->data);
//   array_set_length(offset_sparse_index, 0);
//   free(offset_sparse_index->data);
//   free(offset_sparse_index);
//   // for (u32 i = 0; i < out_count; i++) {
//   //   free(keys[i]->data);
//   //   free(values[i]->data);
//   // }
//   free(keys);
//   free(values);
//   return sst;
// }

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

  u8 *start = (u8 *)buf;
  u32 batch_size = 1 << 6; // sparse index
  // write the data and build the index
  for (u32 i = 0; i < out_count; i++) {
    if (i % batch_size == 0) {
      array_push(offset_sparse_index,
                 &(struct KeyOffsetPair){.key = *keys[i],
                                         .offset = (u32)((u8 *)buf - start)});
    }
    u8 *cursor = (u8 *)buf;
    scribe_put_u16(&cursor, kPairTag);
    scribe_put_u16(&cursor, keys[i]->len);
    scribe_put_bytes(&cursor, keys[i]->data, keys[i]->len);
    scribe_put_u16(&cursor, values[i]->len);
    scribe_put_bytes(&cursor, values[i]->data, values[i]->len);
    buf = (char *)cursor;
  }

  // write the index
  for (u32 i = 0; i < (out_count - 1) / batch_size + 1; i++) {
    //     printf("idx-write-loop iter %d\n", i);

    LString *key =
        &(array_index(offset_sparse_index, struct KeyOffsetPair, i).key);
    //     printf("key=%p\n", key);
    u32 *offset =
        &(array_index(offset_sparse_index, struct KeyOffsetPair, i).offset);
    u8 *cursor = (u8 *)buf;
    scribe_put_bytes(&cursor, key->data, key->len);
    scribe_put_u32(&cursor, *offset);
    buf = (char *)cursor;
  }

  return ((u8 *)buf) - start;
}

void deserialize_index(char *index, int len, Array *keys_out,
                       Array *offsets_out) {
  char buf[8];
  char *curr = index;
  while ((curr - index) < len) {
    struct KeyOffsetPair kop = {0};
    KeyOffsetPair_deserialize(&kop, &curr);
    array_push(keys_out, &kop.key);
    array_push(offsets_out, &kop.offset);
  }
}

FILE *sstable_open_file(SSTable *sst) {
  char *fp __attribute__((__cleanup__(cleanup_char_buf)));
  fp = lstring_to_cstr(sst->filepath);
  FILE *fptr = fopen(fp, "r");
  return fptr;
}

LString *search_in_sst(SSTable sst, LString *key) {
  FILE *fptr __attribute__((__cleanup__(cleanup_file)));
  //
  fptr = sstable_open_file(&sst);
  if (fptr == NULL) {
    int err = errno;
    printf("ERROR [%d]: cannot open file '%.*s'\n", err, sst.filepath->len,
           sst.filepath->data);
    return NULL;
  }

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

  hex_dump(index, index_size);
  Array *keys = array_sized_new(32, sizeof(LString));
  Array *offsets = array_sized_new(32, sizeof(u32));

  deserialize_index(index, index_size, keys, offsets);
  free(index);

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
      offset = array_index(offsets, u32, mid);
      low = mid + 1;
    } else {
      high = mid - 1;
    }
  }
  printf("Offset: %d\n", offset);
  if (offset == -1) {
    offset = array_index(offsets, u32, keys->len - 1);
    printf("Offset max: %d\n", offset);
  }
  // TODO: find nearest point here!!, do not set offset=-1 if not found
  fseek(fptr, offset, SEEK_SET);
  int block_len, uncompressed_len;
  fread(&block_len, 1, sizeof(block_len), fptr);
  fread(&uncompressed_len, 1, sizeof(uncompressed_len), fptr);
  char *buf = malloc(block_len);
  fread(buf, 1, block_len, fptr);
  char *dst = malloc(uncompressed_len);
  LZ4_decompress_safe(buf, dst, block_len, uncompressed_len);
  char *cursor = dst;
  struct SSTPair p = {0};
  while (cursor - dst <= uncompressed_len) {
    SSTPair_deserialize(&p, &cursor);
    if (lstring_compare(&p.key, key) == 0) {
      if (p.tombstone == 1) {
        return NULL;
      }
      LString *value = malloc(sizeof(LString));
      *value = p.value;
      return value;
    }
  }
  return NULL;

  // TODO: do linear search here
  printf("Beginning linear search\n");
  printf("Searching for key: %.*s\n", key->len, key->data);
  const u8 *src;
  while (memcmp(cursor, &kPairTag, kTagSize) == 0) {
    cursor += kTagSize;
    LString k;
    src = (const u8 *)cursor;
    k.len = scribe_get_u16(&src);
    k.data = malloc(k.len);
    scribe_get_bytes(&src, k.data, k.len);
    cursor = (char *)src;
    printf("Current key: %.*s\n", k.len, k.data);
    if (lstring_compare(&k, key) == 0) {
      printf("Found match...\n");
      LString *value = malloc(sizeof(LString));
      src = (const u8 *)cursor;
      value->len = scribe_get_u16(&src);
      value->data = malloc(value->len);
      scribe_get_bytes(&src, value->data, value->len);
      cursor = (char *)src;
      free(buf);
      free(dst);
      free(k.data);
      for (u32 i = 0; i < keys->len; i++) {
        free(array_index(keys, LString, i).data);
      }
      free(keys->data);
      free(keys);
      free(offsets->data);
      free(offsets);
      return value;
    } else {
      u16 value_len;
      src = (const u8 *)cursor;
      value_len = scribe_get_u16(&src);
      src += value_len;
      cursor = (char *)src;
      free(k.data);
      continue;
    }
  }
  free(buf);
  free(dst);
  for (u32 i = 0; i < keys->len; i++) {
    free(array_index(keys, LString, i).data);
  }
  free(keys->data);
  free(keys);
  free(offsets->data);
  free(offsets);
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

void SSTPair_print(struct SSTPair *pair) {
  printf("key=[%d]%.*s value=[%d]%.*s\n", pair->key.len, pair->key.len,
         pair->key.data, pair->value.len, pair->value.len, pair->value.data);
}

void SSTPair_deserialize_without_vals(struct SSTPair *pair, char *cursor) {
  const u8 *src = (const u8 *)cursor;
  pair->tag = scribe_get_u16(&src);
  pair->key.len = scribe_get_u16(&src);
  pair->key.data = malloc(pair->key.len);
  scribe_get_bytes(&src, pair->key.data, pair->key.len);
  pair->tombstone = scribe_get_u8(&src);
  pair->value.len = scribe_get_u16(&src);
  SSTPair_print(pair);
}

char *sstable_get_data_block(SSTable *sst, u32 *data_block_size) {
  FILE *fptr = sstable_open_file(sst);
  if (fptr == NULL) {
    int err = errno;
    printf("ERROR [%d]: cannot open file '%.*s'\n", err, sst->filepath->len,
           sst->filepath->data);
    fclose(fptr);
    return NULL;
  }

  struct stat st;
  fstat(fileno(fptr), &st);

  fseek(fptr, st.st_size - 8, SEEK_SET);
  u32 index_offset;
  u32 index_size;
  fread(&index_offset, 1, sizeof(index_offset), fptr);
  fread(&index_size, 1, sizeof(index_size), fptr);

  fseek(fptr, 0, SEEK_SET);

  char *data_block = malloc(index_offset);
  *data_block_size = index_offset;
  fread(data_block, 1, index_offset, fptr);
  return data_block;
}

struct CursorArray {
  u32 size;
  u32 cursor;
  char *data;
};

void SSTPair_deserialize_without_vals_ca(struct SSTPair *pair,
                                         struct CursorArray *ca) {
  u32 cursor = ca->cursor;
  const u8 *src = (const u8 *)&ca->data[cursor];
  pair->tag = scribe_get_u16(&src);
  pair->key.len = scribe_get_u16(&src);
  pair->key.data = malloc(pair->key.len);
  scribe_get_bytes(&src, pair->key.data, pair->key.len);
  pair->tombstone = scribe_get_u8(&src);
  // memcpy(&pair->value.len, &ca->data[ca->cursor], kLStringLenSize);
  // ca->cursor += kLStringLenSize;
}

char *decompress_data_block(char *data_block) {}

struct ByteRing *decompress_block_br(char *block, int *compressed_block_size,
                                     int *data_len) {
  const u8 *src = (const u8 *)block;
  *compressed_block_size = scribe_get_u32(&src);
  *data_len = scribe_get_u32(&src);

  struct ByteRing *decompressed = byte_ring_init(*compressed_block_size);

  LZ4_decompress_safe((const char *)src, decompressed->data, *data_len,
                      *compressed_block_size);
  decompressed->start = 0;
  decompressed->size = *compressed_block_size;
  return decompressed;
}

char *decompress_block(char *block, int *compressed_block_size, int *data_len) {
  const u8 *src = (const u8 *)block;
  *compressed_block_size = scribe_get_u32(&src);
  *data_len = scribe_get_u32(&src);

  char *decompressed = malloc(*data_len);
  LZ4_decompress_safe((const char *)src, decompressed, *compressed_block_size,
                      *data_len);
  return decompressed;
}

void merge_and_compact_level_zero(Array *sstables) {
  char *cursors[sstables->len];
  char *data_blocks[sstables->len];
  u32 data_block_sizes[sstables->len];
  u32 indices[sstables->len];
  struct SSTPair pairs[sstables->len];
  for (u32 i = 0; i < sstables->len; i++) {
    data_blocks[i] = sstable_get_data_block(&array_index(sstables, SSTable, i),
                                            &data_block_sizes[i]);
    cursors[i] = data_blocks[i];
    indices[i] = 0;
  }
  // for (u32 i = 0; i < sstables->len; i++);
  //
  // for (u32 i = 0; i < sstables->len; i++) {
  //   int cbs;
  //   int dl;
  //   // todo: repeatedly decompress until done
  //   // may need size of entire data block to know when to stop
  //   // diff between single compressed block AND entire data block
  //   //
  //   // compressed_size real_size <data>
  //   // compressed_size real_size <data>
  //   // ...
  //   // /DONE
  //   // <index_block>
  //   decompress_block(data_blocks[i], &cbs, &dl);
  // }

  FILE *out = fopen("out.sst", "a");

  Array *batch = array_sized_new(kBatchSize, sizeof(struct SSTPair));
  struct CursorArray dc_block_cursors[sstables->len];
  memset(dc_block_cursors, 0, sizeof(dc_block_cursors));
  u32 batch_size_in_bytes = 0;

  int iteration = 1;
  while (1) {
    printf("Iteration = %d\n", iteration);
    int processed = 0;
    for (u32 i = 0; i < sstables->len; i++) {

      printf("[%d] ptrdiff cursors - data_blocks = %d\n", i,
             (cursors[i] - data_blocks[i]));
      printf("[%d] size= %d\n", i, (data_block_sizes[i]));
      if (cursors[i] - data_blocks[i] >= data_block_sizes[i]) {
        printf("done processing idx=%d\n", i);
        continue;
      }
      processed++;
      int compressed_block_size;
      int uncompressed_size;
      if (dc_block_cursors[i].cursor == dc_block_cursors[i].size) {

        printf("iter=%d ssidx=%d cursor==size\n", iteration, i);
        dc_block_cursors[i].cursor = 0;
        dc_block_cursors[i].data = decompress_block(
            cursors[i], &compressed_block_size, &uncompressed_size);
        cursors[i] += compressed_block_size;
        dc_block_cursors[i].size = uncompressed_size;
      }
      // char *decompressed = decompress_block(cursors[i],
      // &compressed_block_size,
      //                                       &uncompressed_size);

      SSTPair_deserialize_without_vals_ca(&pairs[i], &dc_block_cursors[i]);
      // free(decompressed);
    }
    LString min_key = pairs[0].key;
    u32 min_index = 0;
    for (u32 i = 1; i < sstables->len; i++) {
      if (lstring_compare(&min_key, &pairs[i].key) >= 0) {
        min_key = pairs[i].key;
        min_index = i;
      }
    }
    // jump to value
    dc_block_cursors[min_index].cursor += kTagSize + kLStringLenSize +
                                          min_key.len +
                                          sizeof(pairs[min_index].tombstone);

    printf("iter=%d min_idx=%d dc[0..32]=%.*s\n", iteration, min_index, 32,
           dc_block_cursors[min_index].data +
               dc_block_cursors[min_index].cursor);
    printf("++ iter=%d min_idx=%d dc[0..32]=%.*x\n", iteration, min_index, 32,
           dc_block_cursors[min_index].data +
               dc_block_cursors[min_index].cursor);

    // get value
    LString value;
    const u8 *src = (const u8 *)&dc_block_cursors[min_index]
                        .data[dc_block_cursors[min_index].cursor];
    value.len = scribe_get_u16(&src);
    value.data = malloc(value.len);
    scribe_get_bytes(&src, value.data, value.len);
    dc_block_cursors[min_index].cursor += kLStringLenSize + value.len;
    pairs[min_index].value = value;

    batch_size_in_bytes += SSTPair_get_size(&pairs[min_index]);

    printf("pairs[min_index]\n");
    SSTPair_print(&pairs[min_index]);

    array_push(batch, &pairs[min_index]);
    if (batch->len == kBatchSize) {
      // compress and dump
      // compress_and_write(batch, batch_size_in_bytes, out);

      for (u32 i = 0; i < batch->len; i++) {
        struct SSTPair batch_item = array_index(batch, struct SSTPair, i);
        printf("Batch item [%i]\n", i);
        printf("key=%.*s value=%.*s\n", batch_item.key.len, batch_item.key.data,
               batch_item.value.len, batch_item.value.data);
        fwrite(batch_item.key.data, batch_item.key.len, 1, out);
        fwrite(batch_item.value.data, batch_item.value.len, 1, out);
      }
      fflush(out);
      printf("Batch dumped!\n");
      array_make_empty(batch);
      batch_size_in_bytes = 0;
      break;
    }
    iteration++;
  }

  for (u32 i = 0; i < sstables->len; i++) {
    free(data_blocks[i]);
  }
}

//
//
//
//
// SST1                        SST2
// compressed_block_1          compressed_block_1
//   antioch, av                 alabama, av
//   burma, bv                   boston, bv
//   cyprus, cv                  canada, cv
// compressed_block_2          compressed_block_2
//   danzig, dv                  dorset, dv
//   estonia, ev                 enterprise, ev
//   finland, fv                 fiji, fv
//
// merged
//   alabama, avdst
//   antioch, av
//   boston, bv
//   burma, bv
//   canada, cv
//   cyprus, cv

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
//   // 2. Insert Test Data (using distinct values to make them easy to find
//   in
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
//     printf("Len: %-4u | Data: \"%.*s\" | Value: %.*s\n",
//     debug_keys[i]->len,
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
