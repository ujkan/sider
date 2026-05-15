#ifndef INDEX_BLOCK_H
#define INDEX_BLOCK_H

#include "types.h"
#include "lstr.h"
#include "stb_ds.h"

struct IndexItem {
    LString *key;
    u32 offset;
};

struct IndexBlock {
  struct IndexItem *items;
};

u32 IndexBlock_search(struct IndexBlock *iblock, LString *key);
LString *IndexBlock_serialize(struct IndexBlock *iblock);

#endif // INDEX_BLOCK_H
