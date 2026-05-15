#include "block.h"
#include "hex_dump.h"
#include "stb_ds.h"
#include <lz4.h>

void DataBlockSingle_init(struct DataBlockSingle *block);
LString *DataBlockSingle_compress(struct DataBlockSingle *block);
void DataBlockSingle_append_item(
    struct DataBlockSingle *block,
    struct BlockItem *item); // implicitly manages RestartPoints !
LString *DataBlockSingle_to_LString(struct DataBlockSingle *block);

void DataBlockSingle_init(struct DataBlockSingle *block) {
  block->items = NULL;
  block->restart_points = NULL;
  arrsetcap(block->items, 512);
  arrsetcap(block->restart_points, 128);
}

LString *RestartPoint_serialize(struct RestartPoint *rp) {
  u16 len =
      rp->key.len + sizeof(rp->offset); // TODO: if rp->key.len > sizeof(u16) -
                                        // 4 this will overflow
  LString *serialized = lstring_create(len);
  memcpy(serialized->data, rp->key.data, rp->key.len);
  memcpy(serialized->data + rp->key.len, &rp->offset, sizeof(rp->offset));
  return serialized;
}

void RestartPoint_serialize_into(struct RestartPoint *rp, char **buf) {
  // u16 len =
  //     rp->key.len + sizeof(rp->offset); // TODO: if rp->key.len > sizeof(u16)
  //     -
  //                                       // 4 this will overflow
  memcpy(*buf, rp->key.data, rp->key.len);
  *buf += rp->key.len;
  memcpy(*buf, &rp->offset, sizeof(rp->offset));
}

LString *DataBlockSingle_compress(struct DataBlockSingle *block) {
  char *buf = malloc(1024 * 1024 * 1024);
  char *bufptr = buf;

  for (int i = 0; i < arrlen(block->items); i++) {
    BlockItem_serialize_into(&block->items[i], &bufptr);
  }
  for (int i = 0; i < arrlen(block->restart_points); i++) {
    RestartPoint_serialize_into(&block->restart_points[i], &bufptr);
  }
  // hex_dump(buf, bufptr - buf);
  char *compressed = malloc(LZ4_compressBound(bufptr - buf));
  int size = LZ4_compress_default(buf, compressed, bufptr - buf,
                                  LZ4_compressBound(bufptr - buf));
  LString *c = malloc(
      sizeof(LString)); // TODO: we need a type for generic length-prefixed
                        // string (with u32 or u64 len) and type for Key with
                        // u16 or some other way to enforce key_len is u16
  c->data = compressed;
  c->len = size;
  return c;
}

LString *DataBlock_compress(struct DataBlock *block) {
  char *buf = malloc(1024 * 1024 * 1024);
  char *bufptr = buf;
  for (int j = 0; j < arrlen(block->blocks); j++) {
    struct DataBlockSingle *b = &block->blocks[j];

    for (int i = 0; i < arrlen(b->items); i++) {
      LString *serialized = BlockItem_serialize(&b->items[i]);
      memcpy(bufptr, serialized->data, serialized->len);
      bufptr += serialized->len;
    }
    for (int i = 0; i < arrlen(b->restart_points); i++) {
      LString *serialized = RestartPoint_serialize(&b->restart_points[i]);
      memcpy(bufptr, serialized->data, serialized->len);
      bufptr += serialized->len;
    }
  }
  hex_dump(buf, bufptr - buf);
  char *compressed = malloc(LZ4_compressBound(bufptr - buf));
  int size = LZ4_compress_default(buf, compressed, bufptr - buf,
                                  LZ4_compressBound(bufptr - buf));
  LString *c = malloc(
      sizeof(LString)); // TODO: we need a type for generic length-prefixed
                        // string (with u32 or u64 len) and type for Key with
                        // u16 or some other way to enforce key_len is u16
  c->data = compressed;
  c->len = size;
  return c;
}

void DataBlockSingle_append_item(struct DataBlockSingle *block,
                                 struct BlockItem *item) {
  int len = arrlen(block->items);
  if (len % 32 == 0) {
    // restart point
    struct RestartPoint rp;
    rp.key.data = item->suffix;
    rp.key.len = item->suffix_len;
  }
  arrpush(block->items, *item);
}

void DataBlockSingle_append_entry(struct DataBlockSingle *block, LString *key,
                                  LString *value, LString *prev) {

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
  DataBlockSingle_append_item(block, &b_item);
}
