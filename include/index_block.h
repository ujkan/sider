#ifndef INDEX_BLOCK_H
#define INDEX_BLOCK_H

#include "types.h"
#include "lstr.h"
#include "stb_ds.h"

struct IndexItem {
    LString *key;
    u32 offset;
};

LString *IndexItem_serialize(struct IndexItem *iitem);

struct IndexSection {
  struct IndexItem *items;
  u32 *restart_points; // TODO: not needed here; only for in-file repr; once we have IndexItem *items it means we already know indices!
};

struct IndexItem *IndexSection_search(char *index_section, u32 *index_section_restart_points, u32 count, LString *key);
LString *IndexSection_serialize(struct IndexSection *iblock);
struct IndexSection *IndexSection_deserialize(char *buf, u32 len);

#endif // INDEX_BLOCK_H
