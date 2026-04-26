#include "types.h"
#include "lstr.h"
#include "stb_ds.h"

#ifndef INDEX_BLOCK_H
#define INDEX_BLOCK_H

struct IndexItem {
    LString *key;
    u32 offset;
};

struct IndexBlock {
  IndexItem *items;
};

u32 IndexBlock_search(struct IndexBlock *iblock, LString *key);

#endif // INDEX_BLOCK_H
