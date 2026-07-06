#pragma once

#include "block.h"
#include "types.h"

// #define i_key DataBlock
//
// #include <stc/hashmap.h>

typedef struct BlockCache {
    u32 cap;
    
    // hash map
    // doubly linked list -> but implemented as flat array!
} BlockCache;


BlockCache *BlockCache_init(u32 cap);


typedef struct Node {
    u64 data;
    u32 next;
    u32 prev;
} Node;


typedef struct TT {
    Node items[1024];
    u32 head;
    u32 tail;
    u32 free;
} TT;

void TT_append(TT tt, u64 data) {
    u32 old_free_next = tt.items[tt.free].next;
    tt.items[tt.free].data = data;
    tt.items[tt.free].next = tt.head;
    tt.items[tt.free].prev = tt.tail;
    tt.items[tt.tail].next = tt.free;
    tt.tail = tt.free;

    tt.free = old_free_next;

}





//    0    1    2    3    4    5
//    1    2    f1   3    4    f2
//
//
