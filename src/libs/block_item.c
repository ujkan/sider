#include "block_item.h"
#include "lstr.h"
#include <stdlib.h>
#include <string.h>

LString *BlockItem_serialize(struct BlockItem *item) {
  u16 len = sizeof(item->shared) + sizeof(item->suffix_len) + item->suffix_len +
            sizeof(item->value_len) + item->value_len;
  u8 *data = malloc(len);
  u8 *dataptr = data;
  memcpy(dataptr, &item->shared, sizeof(item->shared));
  dataptr += sizeof(item->shared);

  memcpy(dataptr, &item->suffix_len, sizeof(item->suffix_len));
  dataptr += sizeof(item->suffix_len);

  memcpy(dataptr, item->suffix, item->suffix_len);
  dataptr += item->suffix_len;

  memcpy(dataptr, &item->value_len, sizeof(item->value_len));
  dataptr += sizeof(item->value_len);

  memcpy(dataptr, item->value, item->value_len);
  dataptr += item->value_len;

  LString *ret = lstring_create(len);
  ret->data = data;
  return ret;
}

void BlockItem_serialize_into(struct BlockItem *item, char **data) {
  // u16 len = sizeof(item->shared) + sizeof(item->suffix_len) + item->suffix_len +
  //           sizeof(item->value_len) + item->value_len;
  char *dataptr = *data;
  memcpy(dataptr, &item->shared, sizeof(item->shared));
  dataptr += sizeof(item->shared);

  memcpy(dataptr, &item->suffix_len, sizeof(item->suffix_len));
  dataptr += sizeof(item->suffix_len);

  memcpy(dataptr, item->suffix, item->suffix_len);
  dataptr += item->suffix_len;

  memcpy(dataptr, &item->value_len, sizeof(item->value_len));
  dataptr += sizeof(item->value_len);

  memcpy(dataptr, item->value, item->value_len);
  dataptr += item->value_len;

  *data = dataptr;
}
