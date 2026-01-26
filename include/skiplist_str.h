#ifndef SKIPLIST_STR_H
#define SKIPLIST_STR_H

#include "lstr.h"
#include <stddef.h>

typedef struct Node Node;
typedef struct SkipList SkipList;

struct Node {
  int level;
  LString *key;
  LString *value;
  Node *next[];
};

struct SkipList {
  int num_levels;
  int num_elements;
  size_t size_in_bytes;
  Node *head;
};

SkipList *sl_s_init();
LString *sl_s_find(SkipList *sl, LString *key);
i32 sl_s_insert(SkipList *sl, LString *key, LString * value);
i32 sl_s_remove(SkipList *sl, LString *key);
i32 sl_s_get_data(SkipList *sl, LString ***keys, LString ***values, u32 *out_count);
i32 sl_s_destroy(SkipList *sl);
void pretty_print_skiplist(struct SkipList *list);

#endif
