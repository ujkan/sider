#include "types.h"
#include "lstr.h"
#include "block_item.h"
#include "stb_ds.h"

#ifndef BLOCK_H
#define BLOCK_H

struct RestartPoint {
  LString key;
  u32 offset;
};

struct DataBlock {
  struct BlockItem *items; // type BlockItem
  struct RestartPoint *restart_points; // type RestartPoint
  int checksum;
  // other metadata
};

void DataBlock_init(struct DataBlock *block);
LString *DataBlock_compress(struct DataBlock *block);
void DataBlock_append_item(struct DataBlock *block, struct BlockItem *item); // implicitly manages RestartPoints !
void DataBlock_append_key(struct DataBlock *block, struct LString *key, struct LString *prev); // nice helper
LString* DataBlock_to_LString(struct DataBlock *block);

#endif // BLOCK_H
