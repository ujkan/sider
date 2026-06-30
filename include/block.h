#ifndef BLOCK_H
#define BLOCK_H

#include "types.h"
#include "lstr.h"
#include "block_item.h"
#include <stddef.h>


static const u32 kDataRestartPointInterval = 16;
static const u32 kIndexRestartPointInterval = 1;

struct RestartPoint {
  LString key; // contains shared ref (u8 *data) to BlockItem's suffix
  u32 offset;
};

typedef struct DataBlock {
  struct BlockItem *items; // type BlockItem
  struct RestartPoint *restart_points; // type RestartPoint
  int checksum;
  u32 items_size_bytes;
  // other metadata
} DataBlock;

struct DataSection {
    struct DataBlock *blocks;
};

void DataBlock_init(struct DataBlock *block);
void DataBlock_destroy(struct DataBlock *block);
struct DataBlock * DataBlock_deserialize(u8 *data, u32 size);
void DataBlock_serialize_into(struct DataBlock *block, u8 **buf);
LString *DataBlock_serialize(struct DataBlock *block);
LString *DataSection_compress(struct DataSection *block);
void DataBlock_append_item(struct DataBlock *block, struct BlockItem *item); // implicitly manages RestartPoints !
void DataBlock_append_entry(struct DataBlock *block, LString *key, LString *value, LString *prev); // nice helper
LString* DataBlock_to_LString(struct DataBlock *block);
LString *DataBlock_compress(struct DataBlock *block,
                                  size_t *original_size);
void DataBlock_compressed_serialize_into(struct DataBlock *block, u8 **buf);

LString *RestartPoint_serialize(struct RestartPoint *rp);
void RestartPoint_serialize_into(struct RestartPoint *rp, u8 **buf);
struct DataBlock* DataSection_add_new_block(struct DataSection *section);
struct DataBlock *DataBlock_deserialize(u8 *data, u32 size);
struct DataBlock *DataBlock_compressed_deserialize(u8 *data, u32 original_size, u32 compressed_size);
int DataBlock_get(struct DataBlock *block, LString *key, LString **return_value);

void DataSection_destroy(struct DataSection *section);

#endif // BLOCK_H
