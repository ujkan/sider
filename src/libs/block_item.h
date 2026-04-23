#include "lstr.h"
#include <stdlib.h>
#include <string.h>

struct BlockItem {
  u16 shared;
  u16 suffix_len;
  u8 *suffix;
  u16 value_len;
  u8 *value;
};

LString *BlockItem_serialize(struct BlockItem *item);
