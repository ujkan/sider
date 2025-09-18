#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
typedef int KeyType;
struct NodeType;
struct MarkPtrType;
typedef uint64_t TagType;

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

int8_t tag_compare(TagType *t1, TagType *t2) {
    // TODO IMPL
    return 0;
}

int8_t key_compare(KeyType *t1, KeyType *t2) {
    // TODO IMPL
    return 0;
}

int foo() {
    struct LockfreeSet x;
    x.search_state.prev_ptr = 0;
    return 0;
}
