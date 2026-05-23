#include "block.h"
#include "block_item.h"
#include "hex_dump.h"
#include "lstr.h"
#include "scribe.h"
#include "stb_ds.h"
#include <lz4.h>
#include <stdlib.h>

void DataBlock_init(struct DataBlock *block) {
  block->items = NULL;
  block->restart_points = NULL;
  block->items_size_bytes = 0;
  arrsetcap(block->items, 512);
  arrsetcap(block->restart_points, 128);
}

void DataBlock_compressed_serialize_into(struct DataBlock *block, u8 **buf) {
  size_t original_size = 0;
  LString *compressed_block = DataBlock_compress(block, &original_size);
  scribe_put_u32(buf, original_size);
  scribe_put_u32(buf, compressed_block->len);
  scribe_put_bytes(buf, compressed_block->data, compressed_block->len);
  lstring_free(compressed_block);
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
  scribe_put_u16(&dataptr, rp->key.len);
  scribe_put_bytes(&dataptr, rp->key.data, rp->key.len);
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
  if (len % 32 == 0) {
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
