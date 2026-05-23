#include "block_item.h"
#include "lstr.h"
#include "scribe.h"
#include <stdlib.h>

LString *BlockItem_serialize(struct BlockItem *item) {
  u16 len = sizeof(item->shared) + sizeof(item->suffix_len) + item->suffix_len +
            sizeof(item->value_len) + item->value_len;
  u8 *data = malloc(len);
  u8 *dataptr = data;
  scribe_put_u16(&dataptr, item->shared);
  scribe_put_u16(&dataptr, item->suffix_len);
  scribe_put_bytes(&dataptr, item->suffix, item->suffix_len);
  scribe_put_u16(&dataptr, item->value_len);
  scribe_put_bytes(&dataptr, item->value, item->value_len);

  LString *ret = malloc(sizeof(LString));
  ret->len = len;
  ret->data = data;
  return ret;
}

void BlockItem_serialize_into(struct BlockItem *item, u8 **data) {
  // u16 len = sizeof(item->shared) + sizeof(item->suffix_len) +
  // item->suffix_len +
  //           sizeof(item->value_len) + item->value_len;
  u8 *dataptr = *data;
  scribe_put_u16(&dataptr, item->shared);
  scribe_put_u16(&dataptr, item->suffix_len);
  scribe_put_bytes(&dataptr, item->suffix, item->suffix_len);
  scribe_put_u16(&dataptr, item->value_len);
  scribe_put_bytes(&dataptr, item->value, item->value_len);

  *data = dataptr;
}

u32 BlockItem_size(struct BlockItem *item) {
  return sizeof(item->shared) + sizeof(item->suffix_len) + item->suffix_len +
         sizeof(item->value_len) + item->value_len;
}
