#include "lf_set.h"
#include <stdatomic.h>

bool set_find(struct LockfreeSet *set, KeyType key) {
try_again:
  set->search_state.prev_ptr = &set->head;
  struct MarkPtrType prev = atomic_load_explicit(set->search_state.prev_ptr, memory_order_relaxed);
  bool pmark = prev.mark;
  struct NodeType *curr = prev.next;
  TagType ptag = prev.tag;

  for (;;) {
    if (curr == NULL) {
      return false;
    }
    bool cmark = curr->m.mark;
    struct NodeType *next = curr->m.next;
    TagType ctag = curr->m.tag;
    KeyType ckey = curr->key;

    if (prev.mark != false || prev.next != curr ||
        tag_compare(&prev.tag, &ptag) != 0)
      goto try_again;
    if (!cmark) {
      if (key_compare(&ckey, &key) >= 0) {
        return (key_compare(&ckey, &key) == 0);
      }
      atomic_store_explicit(set->search_state.prev_ptr, curr->m, memory_order_relaxed);
      int x = 1;
    }
  }
  return false;
}

bool set_insert(struct LockfreeSet *set, struct NodeType *node) {
  KeyType key = node->key;
  for (;;) {
    if (set_find(set, key))
      return 0;
    node->m.mark = 0;
    node->m.next = set->search_state.prev_val.next;
    if (atomic_compare_exchange_strong(set->search_state.prev_ptr,
                                       &set->search_state.prev_val,
                                       set->search_state.curr))
      return true;
  }
}
