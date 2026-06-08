#pragma once

#include "block.h"
#include "index_block.h"
#include "types.h"

#pragma pack(push, 1)
typedef struct {
    u32 index_offset;
    u32 index_size;
    u32 index_restart_array_offset;
    u32 index_restart_array_size;
} SSTFileFooter;
#pragma pack(pop)


typedef struct {
    struct DataSection data;
    struct IndexSection index;
    SSTFileFooter footer;
} SSTFileContents;


// TODO: each DataBlock has a footer showing number of restart points;
// then one can go from <end-of-block> - sizeof(footer) - <num-of-points> * sizeof(point)
