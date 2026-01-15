#ifndef SKIPLIST_STR_H
#define SKIPLIST_STR_H

#include "lstr.h"

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
  Node *head;
};

int sl_s_find(SkipList *sl, LString *key);
int sl_s_insert(SkipList *sl, LString *key, LString * value);
int sl_s_remove(SkipList *sl, LString *key);
int sl_s_get_data(SkipList *sl, LString ***keys, LString ***values, u32 *out_count);
void pretty_print_skiplist(struct SkipList *list);

#endif
