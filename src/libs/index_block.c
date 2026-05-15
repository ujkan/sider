#include "index_block.h"
#include "scribe.h"
#include <stdlib.h>

LString *IndexItem_serialize(struct IndexItem *iitem) {
  u16 len = iitem->key->len + sizeof(iitem->offset);
  u8 *data = malloc(len);
  u8 *dataptr = data;
  scribe_put_bytes(&dataptr, iitem->key->data, iitem->key->len);
  scribe_put_u32(&dataptr, iitem->offset);
  LString *serialized = malloc(sizeof(LString));
  serialized->len = len;
  serialized->data = data;
  return serialized;
}
LString *IndexBlock_serialize(struct IndexBlock *iblock) {
  u8 *buf = malloc(1024 * 1024);
  u8 *bufptr = buf;
  for (int i = 0; i < arrlen(iblock->items); i++) {
    LString *serialized = IndexItem_serialize(&iblock->items[i]);
    scribe_put_bytes(&bufptr, serialized->data, serialized->len);
    lstring_free(serialized);
  }
  LString *serialized = malloc(sizeof(LString));
  serialized->len = bufptr - buf;
  serialized->data = buf;
  return serialized;
}
