#include "index_block.h"
#include "lstr.h"
#include "scribe.h"
#include <stdlib.h>

LString *IndexItem_serialize(struct IndexItem *iitem) {
  u16 len = iitem->key->len + sizeof(iitem->offset) + sizeof(iitem->key->len);
  u8 *data = malloc(len);
  u8 *dataptr = data;
  scribe_put_u16(&dataptr, iitem->key->len);
  scribe_put_bytes(&dataptr, iitem->key->data, iitem->key->len);
  scribe_put_u32(&dataptr, iitem->offset);
  LString *serialized = malloc(sizeof(LString));
  serialized->len = len;
  serialized->data = data;
  return serialized;
}
LString *IndexSection_serialize(struct IndexSection *iblock) {
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

struct IndexSection *IndexSection_deserialize(char *buf, u32 len) {
  struct IndexSection *section = malloc(sizeof(struct IndexSection));
  section->items = NULL;
  u8 *bufptr = (u8 *)buf;
  arrsetcap(section->items, 32);
  while ((bufptr - (u8 *)buf) < len) {
    LString *key = lstring_deserialize((const u8 **)&bufptr);
    u32 offset = scribe_get_u32((const u8 **)(&bufptr));
    struct IndexItem item = {0};
    item.key = key;
    item.offset = offset;
    arrpush(section->items, item);
  }
  return section;
}
