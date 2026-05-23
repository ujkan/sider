#ifndef INDEX_BLOCK_H
#define INDEX_BLOCK_H

#include "types.h"
#include "lstr.h"
#include "stb_ds.h"

struct IndexItem {
    LString *key;
    u32 offset;
};

struct IndexSection {
  struct IndexItem *items;
};

u32 IndexSection_search(struct IndexSection *iblock, LString *key);
LString *IndexSection_serialize(struct IndexSection *iblock);

#endif // INDEX_BLOCK_H
