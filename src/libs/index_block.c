#include "index_block.h"

LString *IndexItem_serialize(struct IndexItem *iitem) {
  LString *serialized = lstring_create(iitem->key->len + sizeof(iitem->offset));
  memcpy(serialized->data, iitem->key->data, iitem->key->len);
  memcpy(serialized->data + iitem->key->len, &iitem->offset,
         sizeof(iitem->offset));
  return serialized;
}
LString *IndexBlock_serialize(struct IndexBlock *iblock) {
  char *buf = malloc(1024 * 1024);
  char *bufptr = buf;
  for (int i = 0; i < arrlen(iblock->items); i++) {
    LString *serialized = IndexItem_serialize(&iblock->items[i]);
    memcpy(bufptr, serialized->data, serialized->len);
    bufptr += serialized->len;
    lstring_free(serialized);
  }
  return lstring_create_from_buf(bufptr - buf, buf);
}
