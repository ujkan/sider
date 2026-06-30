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

#include "persistence.h"
#include "block.h"
#include "bytering.h"
#include "hex_dump.h"
#include "index_block.h"
#include "lstr.h"
#include "scribe.h"
#include "skiplist_str.h"
#include "sst_file.h"
#include "stb_ds.h"
#include "u_array.h"
#include <ctype.h>
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

#define MAX_BLOCK_SIZE (320 * 1024)
#define MAX_COMP_SIZE (MAX_BLOCK_SIZE + (MAX_BLOCK_SIZE / 255) + 16)
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

void compress_and_write(LString **keys, LString **values, char *write_buf,
                        int *written_len) {
  // if (arrlen(keys) == 0 || arrlen(values) == 0) {
  //   *written_len = 0;
  //   return;
  // }
  LString *curr_key = keys[0];
  LString *curr_value = values[0];
  struct DataSection d_section = {0};
  arrsetcap(d_section.blocks, 128);
  struct DataBlock *curr_block = DataSection_add_new_block(&d_section);

  DataBlock_append_entry(curr_block, curr_key, curr_value, NULL);
  LString *prev_key = curr_key;
  // TODO: should we copy curr.key here or just "borrow" it or get its ref?
  struct IndexItem curr_index_item = {.key = curr_key, .offset = 0};
  struct IndexSection i_section = {0};
  arrsetcap(i_section.items, 128);
  arrpush(i_section.items, curr_index_item);

  u8 *cursor = (u8 *)write_buf;
  u32 num_items = arrlen(keys);
  for (u32 j = 1; j < num_items; j++) {
    curr_key = keys[j];
    curr_value = values[j];
    prev_key = keys[j - 1];
    if (j % 128 == 0) {
      DataBlock_compressed_serialize_into(curr_block, &cursor);
      curr_block = DataSection_add_new_block(&d_section);
      curr_index_item.key = curr_key;
      curr_index_item.offset = cursor - (u8 *)write_buf;
      arrpush(i_section.items, curr_index_item);
    }
    DataBlock_append_entry(curr_block, curr_key, curr_value, prev_key);
  }
  DataBlock_compressed_serialize_into(curr_block, &cursor);
  u32 index_offset = cursor - ((u8 *)write_buf);
  u32 index_restart_array_offset;
  LString *index_block_serialized =
      IndexSection_serialize(&i_section, &index_restart_array_offset);
  scribe_put_bytes(&cursor, index_block_serialized->data,
                   index_block_serialized->len);
  // TODO: add footer!!
  SSTFileFooter footer;
  footer.index_size = index_block_serialized->len;
  footer.index_restart_array_offset = index_restart_array_offset;
  footer.index_offset = index_offset;
  footer.index_restart_array_num_elements =
      arrlen(i_section.items) / kIndexRestartPointInterval;
  scribe_put_bytes(&cursor, &footer, sizeof(SSTFileFooter));
  DataSection_destroy(&d_section);
  lstring_free(index_block_serialized);
  arrfree(i_section.items);
  *written_len = (cursor - (u8 *)write_buf);
}

SSTable *dump_memtable_to_sst(SkipList *mt, LString *filepath) {
  SSTable *sst = NULL;
  SSTable *result = NULL;
  LString **keys = NULL;
  LString **values = NULL;
  char *fp = NULL;
  int fd = -1;
  char *map = MAP_FAILED;
  size_t size = 1024 * 1024 * 128;

  sst = malloc(sizeof(SSTable));
  sst->filepath = filepath;

  u32 out_count;
  sl_s_get_data(mt, &keys, &values, &out_count);
  if (out_count == 0)
    goto cleanup;

  fp = lstring_to_cstr(filepath); // 0-terminated means can copy 127 chars max
  fd = open(fp, O_RDWR | O_CREAT | O_TRUNC, 0644);
  if (ftruncate(fd, size) == -1)
    goto cleanup;

  map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (map == MAP_FAILED)
    goto cleanup;

  int written_len;
  compress_and_write(keys, values, map, &written_len);
  hex_dump(map, written_len);
  if (ftruncate(fd, written_len) == -1) {
    perror("ftruncate failed");
    goto cleanup;
  }

  if (msync(map, written_len, MS_SYNC) == -1) {
    perror("Could not sync to disk");
    goto cleanup;
  }
  result = sst;
  sst = NULL;
cleanup:
  free(sst);
  if (fp)
    free(fp);
  arrfree(keys);
  arrfree(values);

  if (map != MAP_FAILED)
    munmap(map, size);
  if (fd != -1)
    close(fd);
  return result;
}

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
    scribe_put_u16(&cursor, key->len);
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
  FILE *fptr = fopen(fp, "rb");
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

  fseek(fptr, -(long)(sizeof(SSTFileFooter)), SEEK_END);

  SSTFileContents sst_fc = {0};
  fread(&sst_fc.footer, 1, sizeof(sst_fc.footer), fptr);
  fseek(fptr, sst_fc.footer.index_offset, SEEK_SET);
  char *index = malloc(sst_fc.footer.index_size);
  fread(index, sst_fc.footer.index_size, 1, fptr);
  u32 *index_restart_points =
      (u32 *)&index[sst_fc.footer.index_restart_array_offset];
  struct IndexItem *idx_item =
      IndexSection_search(index, index_restart_points,
                          sst_fc.footer.index_restart_array_num_elements, key);
  if (!idx_item) {
    return NULL;
  }
  u32 block_start = idx_item->offset;

  fseek(fptr, block_start, SEEK_SET);
  u32 block_hdr[2];
  fread(block_hdr, 1, sizeof(block_hdr), fptr);
  u32 block_original_size = block_hdr[0];
  u32 block_compressed_size = block_hdr[1];
  char *block = malloc(block_compressed_size);
  fread(block, block_compressed_size, 1, fptr);
  // steps:
  // 1. decompress
  // 2. deserialize the block
  struct DataBlock *data_block = DataBlock_compressed_deserialize(
      block, block_original_size, block_compressed_size);
  LString *value = NULL;
  DataBlock_get(data_block, key, &value);
  return value;
}

// LString *search_in_sst(SSTable sst, LString *key) {
//   FILE *fptr __attribute__((__cleanup__(cleanup_file)));
//   //
//   fptr = sstable_open_file(&sst);
//   if (fptr == NULL) {
//     int err = errno;
//     printf("ERROR [%d]: cannot open file '%.*s'\n", err, sst.filepath->len,
//            sst.filepath->data);
//     return NULL;
//   }
//
//   struct stat st;
//   fstat(fileno(fptr), &st);
//
//   fseek(fptr, st.st_size - 8, SEEK_SET);
//   u32 index_offset;
//   u32 index_size;
//   u8 index_hdr[sizeof(index_offset) + sizeof(index_size)];
//   fread(index_hdr, 1, sizeof(index_hdr), fptr);
//   const u8 *hdr_src = index_hdr;
//   index_offset = scribe_get_u32(&hdr_src);
//   index_size = scribe_get_u32(&hdr_src);
//   fseek(fptr, index_offset, SEEK_SET);
//
//   char *index = malloc(index_size);
//   fread(index, index_size, 1, fptr);
//
//   hex_dump(index, index_size);
//   Array *keys = array_sized_new(32, sizeof(LString));
//   Array *offsets = array_sized_new(32, sizeof(u32));
//
//   deserialize_index(index, index_size, keys, offsets);
//   free(index);
//
//   int low = 0;
//   int high = keys->len - 1;
//   int mid;
//   int offset = -1;
//   while (low <= high) {
//     mid = (high + low) / 2;
//     int cmp_val = lstring_compare(key, &array_index(keys, LString, mid));
//     if (cmp_val == 0) {
//       offset = array_index(offsets, u32, mid);
//       break;
//     } else if (cmp_val > 0) {
//       offset = array_index(offsets, u32, mid);
//       low = mid + 1;
//     } else {
//       high = mid - 1;
//     }
//   }
//   printf("Offset: %d\n", offset);
//   if (offset == -1) {
//     offset = array_index(offsets, u32, keys->len - 1);
//     printf("Offset max: %d\n", offset);
//   }
//   // TODO: find nearest point here!!, do not set offset=-1 if not found
//   fseek(fptr, offset, SEEK_SET);
//   int block_len, uncompressed_len;
//   u8 block_hdr[sizeof(block_len) + sizeof(uncompressed_len)];
//   fread(block_hdr, 1, sizeof(block_hdr), fptr);
//   const u8 *block_src = block_hdr;
//   block_len = scribe_get_u32(&block_src);
//   uncompressed_len = scribe_get_u32(&block_src);
//   char *buf = malloc(block_len);
//   fread(buf, 1, block_len, fptr);
//   char *dst = malloc(uncompressed_len);
//   LZ4_decompress_safe(buf, dst, block_len, uncompressed_len);
//   char *cursor = dst;
//   struct SSTPair p = {0};
//   while (cursor - dst <= uncompressed_len) {
//     SSTPair_deserialize(&p, &cursor);
//     if (lstring_compare(&p.key, key) == 0) {
//       if (p.tombstone == 1) {
//         return NULL;
//       }
//       LString *value = malloc(sizeof(LString));
//       *value = p.value;
//       return value;
//     }
//   }
//   return NULL;
//
//   // TODO: do linear search here
//   printf("Beginning linear search\n");
//   printf("Searching for key: %.*s\n", key->len, key->data);
//   const u8 *cursor_src;
//   while (memcmp(cursor, &kPairTag, kTagSize) == 0) {
//     cursor += kTagSize;
//     LString k;
//     cursor_src = (const u8 *)cursor;
//     k.len = scribe_get_u16(&cursor_src);
//     k.data = malloc(k.len);
//     scribe_get_bytes(&cursor_src, k.data, k.len);
//     cursor = (char *)cursor_src;
//     printf("Current key: %.*s\n", k.len, k.data);
//     if (lstring_compare(&k, key) == 0) {
//       printf("Found match...\n");
//       LString *value = malloc(sizeof(LString));
//       cursor_src = (const u8 *)cursor;
//       value->len = scribe_get_u16(&cursor_src);
//       value->data = malloc(value->len);
//       scribe_get_bytes(&cursor_src, value->data, value->len);
//       cursor = (char *)cursor_src;
//       free(buf);
//       free(dst);
//       free(k.data);
//       for (u32 i = 0; i < keys->len; i++) {
//         free(array_index(keys, LString, i).data);
//       }
//       free(keys->data);
//       free(keys);
//       free(offsets->data);
//       free(offsets);
//       return value;
//     } else {
//       u16 value_len;
//       cursor_src = (const u8 *)cursor;
//       value_len = scribe_get_u16(&cursor_src);
//       cursor_src += value_len;
//       cursor = (char *)cursor_src;
//       free(k.data);
//       continue;
//     }
//   }
//   free(buf);
//   free(dst);
//   for (u32 i = 0; i < keys->len; i++) {
//     free(array_index(keys, LString, i).data);
//   }
//   free(keys->data);
//   free(keys);
//   free(offsets->data);
//   free(offsets);
//   return NULL;
// }

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
  u8 index_hdr[sizeof(index_offset) + sizeof(index_size)];
  fread(index_hdr, 1, sizeof(index_hdr), fptr);
  const u8 *src = index_hdr;
  index_offset = scribe_get_u32(&src);
  index_size = scribe_get_u32(&src);

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
