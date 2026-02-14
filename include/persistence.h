#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include "lstr.h"
#include "skiplist_str.h"
#include "u_array.h"

typedef struct {
  LString *filepath;
  int fd;
  unsigned int size;
  LString min;
  LString max;

} SSTable;

SSTable *dump_memtable_to_sst(SkipList *sl, LString *filepath);
LString* search_in_sst(SSTable sst, LString *key);
void merge_and_compact_level_zero(Array *sstables);

#endif // PERSISTENCE_H
