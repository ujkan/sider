#include "types.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
typedef int KeyType;
struct NodeType;
struct MarkPtrType;
typedef u64 TagType;

struct MarkPtrType {
    bool mark;
    struct NodeType *next;
    TagType tag;
};

struct NodeType {
    KeyType key;
    struct MarkPtrType m;
};

struct LockfreeSet {
    struct SearchState {
        _Atomic(struct MarkPtrType) *prev_ptr;
        struct MarkPtrType prev_val;
        struct MarkPtrType curr;
    } search_state;
    _Atomic(struct MarkPtrType) head;
};

bool set_find(struct LockfreeSet *set, KeyType key);

bool set_insert(struct LockfreeSet *set, struct NodeType *node);

i8 tag_compare(TagType *t1, TagType *t2) {
    // TODO IMPL
    return 0;
}

i8 key_compare(KeyType *t1, KeyType *t2) {
    // TODO IMPL
    return 0;
}

int foo() {
    struct LockfreeSet x;
    x.search_state.prev_ptr = 0;
    return 0;
}
