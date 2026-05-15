#ifndef BLOCK_ITEM_H
#define BLOCK_ITEM_H

#include "types.h"
#include "lstr.h"

struct BlockItem {
  u16 shared;
  u16 suffix_len;
  u8 *suffix;
  u16 value_len;
  u8 *value;
};

LString *BlockItem_serialize(struct BlockItem *item);
void BlockItem_serialize_into(struct BlockItem *item, char **buf);

#endif // BLOCK_ITEM_H
