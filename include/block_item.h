#include "types.h"
#include "lstr.h"

#ifndef BLOCK_ITEM_H
#define BLOCK_ITEM_H

struct BlockItem {
  u16 shared;
  u16 suffix_len;
  u8 *suffix;
  u16 value_len;
  u8 *value;
};

LString *BlockItem_serialize(struct BlockItem *item);

#endif // BLOCK_ITEM_H
