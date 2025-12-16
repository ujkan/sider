#ifndef SKIPLIST_STR_H
#define SKIPLIST_STR_H

#include "lstr.h"

typedef struct Node Node;
typedef struct SkipList SkipList;

struct Node {
  LString *data;
  int level;
  Node *next[];
};

struct SkipList {
  int num_levels;
  Node *head;
};

int sl_s_find(SkipList *sl, LString *data);
int sl_s_insert(SkipList *sl, LString *data);
int sl_s_remove(SkipList *sl, LString *data);
int sl_s_get_data(SkipList *sl, LString **keys,  u32 *out_count);

#endif
