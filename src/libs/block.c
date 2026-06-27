#include "block.h"
#include "block_item.h"
#include "hex_dump.h"
#include "lstr.h"
#include "scribe.h"
#include "stb_ds.h"
#include <lz4.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void DataBlock_init(struct DataBlock *block) {
  block->items = NULL;
  block->restart_points = NULL;
  block->items_size_bytes = 0;
  arrsetcap(block->items, 512);
  arrsetcap(block->restart_points, 128);
}

void DataBlock_compressed_serialize_into(struct DataBlock *block, u8 **buf) {
  // TODO: need to add a ptr/offset to the restart points for faster searching
  // or maybe not since we anyway need to deser entire block !!
  size_t original_size = block->items_size_bytes;
  scribe_put_u32(buf, original_size);
  scribe_put_u32(buf, original_size);
  DataBlock_serialize_into(block, buf);
  // LString *compressed_block = DataBlock_compress(block, &original_size);
  // scribe_put_u32(buf, original_size);
  // scribe_put_u32(buf, compressed_block->len);
  // scribe_put_bytes(buf, compressed_block->data, compressed_block->len);
  // lstring_free(compressed_block);
}

void DataBlock_destroy(struct DataBlock *block) {
  // we do not own the keys inside blockitem
  arrfree(block->items);
  arrfree(block->restart_points);
}

LString *RestartPoint_serialize(struct RestartPoint *rp) {
  u16 len =
      rp->key.len + sizeof(rp->offset); // TODO: if rp->key.len > sizeof(u16) -
                                        // 4 this will overflow
  u8 *data = malloc(len);
  u8 *dataptr = data;
  scribe_put_bytes(&dataptr, rp->key.data, rp->key.len);
  scribe_put_u32(&dataptr, rp->offset);
  LString *serialized = malloc(sizeof(LString));
  serialized->len = len;
  serialized->data = data;
  return serialized;
}

void RestartPoint_serialize_into(struct RestartPoint *rp, u8 **buf) {
  // u16 len =
  //     rp->key.len + sizeof(rp->offset); // TODO: if rp->key.len > sizeof(u16)
  //     -
  //                                       // 4 this will overflow
  u8 *dataptr = *buf;
  // NOTE: only offset is serialized!
  // scribe_put_u16(&dataptr, rp->key.len);
  // scribe_put_bytes(&dataptr, rp->key.data, rp->key.len);
  scribe_put_u32(&dataptr, rp->offset);
  *buf = dataptr;
}

LString *DataBlock_serialize(struct DataBlock *block) {
  u8 *buf = malloc(block->items_size_bytes);
  u8 *cursor = buf;

  for (int i = 0; i < arrlen(block->items); i++) {
    BlockItem_serialize_into(&block->items[i], &cursor);
  }
  for (int i = 0; i < arrlen(block->restart_points); i++) {
    RestartPoint_serialize_into(&block->restart_points[i], &cursor);
  }
  LString *serialized = lstring_create(cursor - buf);
  memcpy(serialized->data, buf, serialized->len);
  free(buf);
  return serialized;
}

void DataBlock_serialize_into(struct DataBlock *block, u8 **buf) {
  for (int i = 0; i < arrlen(block->items); i++) {
    BlockItem_serialize_into(&block->items[i], buf);
  }
  for (int i = 0; i < arrlen(block->restart_points); i++) {
    RestartPoint_serialize_into(&block->restart_points[i], buf);
  }
  // footer
  scribe_put_u32(buf, arrlen(block->restart_points));
}

LString *DataBlock_compress(struct DataBlock *block, size_t *original_size) {
  u8 *buf = malloc(1024 * 1024 * 1024);
  u8 *cursor = buf;
  DataBlock_serialize_into(block, &cursor);

  size_t written = cursor - buf;
  int bound = LZ4_compressBound((int)(written));
  u8 *compressed = malloc(bound);
  int size = LZ4_compress_default((char *)buf, (char *)compressed, (int)written,
                                  bound);
  LString *c = malloc(sizeof(LString)); // TODO: we need a type for generic
                                        // length-prefixed string (with u32 or
                                        // u64 len) and type for Key with u16
                                        // or some other way to enforce key_len
                                        // is u16
  *original_size = written;
  c->data = compressed;
  c->len = size;
  return c;
}

LString *DataSection_compress(struct DataSection *section) {
  u8 *buf = malloc(1024 * 1024 * 1024);
  u8 *bufptr = buf;
  for (int j = 0; j < arrlen(section->blocks); j++) {
    struct DataBlock *block = &section->blocks[j];

    for (int i = 0; i < arrlen(block->items); i++) {
      LString *serialized = BlockItem_serialize(&block->items[i]);
      scribe_put_bytes(&bufptr, serialized->data, serialized->len);
    }
    for (int i = 0; i < arrlen(block->restart_points); i++) {
      LString *serialized = RestartPoint_serialize(&block->restart_points[i]);
      scribe_put_bytes(&bufptr, serialized->data, serialized->len);
    }
  }
  hex_dump((char *)buf, bufptr - buf);
  u8 *compressed = malloc(LZ4_compressBound(bufptr - buf));
  int size = LZ4_compress_default((char *)buf, (char *)compressed, bufptr - buf,
                                  LZ4_compressBound(bufptr - buf));
  LString *c = malloc(sizeof(LString)); // TODO: we need a type for generic
                                        // length-prefixed string (with u32 or
                                        // u64 len) and type for Key with u16
                                        // or some other way to enforce key_len
                                        // is u16
  c->data = compressed;
  c->len = size;
  return c;
}

void DataBlock_append_item(struct DataBlock *block, struct BlockItem *item) {
  int len = arrlen(block->items);
  if (len % kRestartPointInterval == 0) {
    // restart point
    struct RestartPoint rp;
    rp.key.data = item->suffix;
    rp.key.len = item->suffix_len;
    rp.offset = block->items_size_bytes;
    arrpush(block->restart_points, rp);
  }
  arrpush(block->items, *item);
  block->items_size_bytes += BlockItem_size(item);
}

void DataBlock_append_entry(struct DataBlock *block, LString *key,
                            LString *value, LString *prev) {
  if (arrlen(block->items) % 32 == 0) {
    prev = NULL;
  }

  u16 shared = 0;
  if (prev != NULL) {
    for (int k = 0; k < key->len && k < prev->len; k++) {
      if (key->data[k] != prev->data[k]) {
        break;
      }
      shared++;
    }
  }
  u16 suffix_len = key->len - shared;

  struct BlockItem b_item = {0};
  b_item.shared = shared;
  b_item.suffix_len = suffix_len;
  b_item.suffix = key->data + shared;
  b_item.value_len = value->len;
  b_item.value = value->data;
  DataBlock_append_item(block, &b_item);
}

struct DataBlock *DataSection_add_new_block(struct DataSection *section) {
  struct DataBlock *new_block = arraddnptr(section->blocks, 1);
  memset(new_block, 0, sizeof(*new_block));
  DataBlock_init(new_block);
  return new_block;
}

void DataSection_destroy(struct DataSection *section) {
  for (int i = 0; i < arrlen(section->blocks); i++) {
    DataBlock_destroy(&section->blocks[i]);
  }
  arrfree(section->blocks);
}

struct DataBlock *DataBlock_deserialize(u8 *data, u32 size) {
  struct DataBlock *block = malloc(sizeof(struct DataBlock));
  DataBlock_init(block);
  u8 *data_end = data + size;
  const u8 *restart_points_len_ptr = data_end - sizeof(u32);
  u32 restart_points_len;
  memcpy(&restart_points_len, restart_points_len_ptr, sizeof(u32));
  u8 *restart_points_ptr =
      (data_end - sizeof(u32)) - (restart_points_len * sizeof(u32));
  const u8 *cursor = data;
  printf("data_ptr = %p\n", data);
  printf("data_end = %p\n", data_end);
  printf("restart_points_ptr = %p\n", restart_points_ptr);
  printf("restart_points_len_ptr = %p\n", restart_points_len_ptr);
  printf("restart_points_len = %d\n", restart_points_len);
  int i = 1;
  while (cursor < restart_points_ptr) {
    // TODO: add a deserialize block item method
    printf("iter [%d] cu=%p rp=%p\n", i, cursor, restart_points_ptr);
    i++;
    struct BlockItem item = {0};
    item.shared = scribe_get_u16(&cursor);
    item.suffix_len = scribe_get_u16(&cursor);
    // NOTE: if we use scribe_get_bytes, we have to do
    // item.suffix = malloc(suffix_len)
    // same holds for item.value
    // that means that at the end we must free decompressed
    // but my logic is, if we already have that data in a nice, linear block of
    // memory why free it? Then our suffix strings would be spread out with this
    // malloc but decompressed has them already packed nice
    // of course we must be very careful; when at the end we may need to free
    // something we need to keep the original `decompressed` pointer somewhere!
    // because only that can be freed so the suffix and value here are VIEWS of
    // the decompressed buffer
    item.suffix = cursor;
    cursor += item.suffix_len;
    item.value_len = scribe_get_u16(&cursor);
    item.value = cursor;
    cursor += item.value_len;
    arrpush(block->items, item);
  }
  for (int i = 0; i < restart_points_len; i++) {
    struct RestartPoint rp = {0};
    rp.offset = scribe_get_u32(&cursor);
    rp.key.len = block->items[i * kRestartPointInterval].suffix_len;
    rp.key.data = block->items[i * kRestartPointInterval].suffix;
    arrpush(block->restart_points, rp);
  }
  return block;
}

struct DataBlock *DataBlock_compressed_deserialize(u8 *data, u32 original_size,
                                                   u32 compressed_size) {
  char *decompressed = malloc(original_size);
  LZ4_decompress_safe(data, decompressed, compressed_size, original_size);
  return DataBlock_deserialize(decompressed, original_size);
}

int lstring_cmp_prefix_suffix(LString *prefix, LString *suffix,
                              LString *other) {
  if (other->len < prefix->len) {
    return -1;
  }
  for (int i = 0; i < prefix->len; i++) {
    if (prefix->data[i] != other->data[i]) {
      return -1;
    }
  }
  if (other->len - prefix->len != suffix->len) {
    return -1;
  }
  for (int i = 0; i < suffix->len; i++) {
    if (suffix->data[i] != other->data[i + prefix->len]) {
      return -1;
    }
  }
  return 0;
}

int DataBlock_get(struct DataBlock *block, LString *key,
                  LString *return_value) {
  int start = 0;
  int end = arrlen(block->restart_points) - 1;
  int mid = (start + end) / 2;
  printf("s=%d m=%d e=%d\n", start, mid, end);
  while (start <= end) {
    printf("s=%d m=%d e=%d\n", start, mid, end);
    printf("rp[mid]=%s\n", block->restart_points[mid].key.data);
    int cmp = lstring_compare(&block->restart_points[mid].key, key);
    printf("cmp=%d\n", cmp);
    if (cmp == 0) {
      return_value->data = block->items[mid].value;
      return_value->len = block->items[mid].value_len;
      return 1;
    } else if (cmp < 0) {
      start = mid + 1;
    } else {
      end = mid - 1;
    }
    mid = (start + end) / 2;
    printf("_s=%d m=%d e=%d\n", start, mid, end);
  }

  // search from mid to mid+1 restart points, that's kRestartPointInterval
  // entries you must remember the prefix too because block->items stores
  // .suffix only
  int s = mid * kRestartPointInterval;
  LString *prev = lstring_create(4);

  printf("starting_mid=%d\n", mid);
  for (int i = 0; i < kRestartPointInterval; i++) {
    struct BlockItem bi = block->items[s + i];
    LString *suffix = lstring_create(bi.suffix_len);
    suffix->data = bi.suffix;
    // this is bad but avoids allocating
    // idea is prefix.data points to potentially longer string
    // but we take a slice/view of length bi.shared !
    // we are treating this string as a StringView but we don't have
    // string view semantics! must not ever free prefix.data; we do not own
    LString prefix = {0};
    prefix.data = prev->data;
    prefix.len = bi.shared;
    int cmp = lstring_cmp_prefix_suffix(&prefix, suffix, key);
    printf("prefix=");
    lstring_print(&prefix);
    printf(" suffix=");
    lstring_print(suffix);
    printf(" key=");
    lstring_print(key);
    printf(" cmp=%d", cmp);
    printf("\n");
    if (cmp == 0) {
      return_value->data = block->items[s + i].value;
      return_value->len = block->items[s + i].value_len;
      return 1;
    }

    if (bi.shared + bi.suffix_len > prev->len) {
      lstring_realloc(prev, bi.shared + bi.suffix_len);
    }
    // prev = prev[:curr.shared] + curr.suffix
    memcpy(&prev->data[bi.shared], suffix->data, suffix->len);

    // we also need to update the prefix somehow?
    //
    //
    //
    // 0 europe/ber europe/ber
    // 8 muc        europe/muc
    // 3 asia       eurasia
    // in each step, after you do whatever, you say
    // prev = prev[:curr.shared] + curr.suffix
  }

  return -mid;
}

//
// 0 1 2 3 4 5 6 7 8
// s       m       e
// s m   e
//     s e
//     m e
