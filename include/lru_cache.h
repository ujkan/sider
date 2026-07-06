#pragma once

#include "types.h"

#include <stc/pqueue.h>

typedef struct LruCache {
    u32 cap;
    pqueue_DataBlock blocks;
} BlockCache;


BlockCache *BlockCache_init(u32 cap);


