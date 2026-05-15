#ifndef BLOCK_H
#define BLOCK_H

#include "types.h"
#include "lstr.h"
#include "block_item.h"
#include <stddef.h>


struct RestartPoint {
  LString key;
  u32 offset;
};

struct DataBlockSingle {
  struct BlockItem *items; // type BlockItem
  struct RestartPoint *restart_points; // type RestartPoint
  int checksum;
  u32 items_size_bytes;
  // other metadata
};

struct DataBlock {
    struct DataBlockSingle *blocks;
};

void DataBlockSingle_init(struct DataBlockSingle *block);
LString *DataBlockSingle_serialize(struct DataBlockSingle *block);
LString *DataBlock_compress(struct DataBlock *block);
void DataBlockSingle_append_item(struct DataBlockSingle *block, struct BlockItem *item); // implicitly manages RestartPoints !
void DataBlockSingle_append_entry(struct DataBlockSingle *block, LString *key, LString *value, LString *prev); // nice helper
LString* DataBlockSingle_to_LString(struct DataBlockSingle *block);
LString *DataBlockSingle_compress(struct DataBlockSingle *block,
                                  size_t *original_size);

LString *RestartPoint_serialize(struct RestartPoint *rp);
void RestartPoint_serialize_into(struct RestartPoint *rp, u8 **buf);

#endif // BLOCK_H
