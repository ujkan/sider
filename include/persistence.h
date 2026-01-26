#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include "lstr.h"
#include "skiplist_str.h"

typedef struct {
  LString *filepath;
  int fd;
  unsigned int size;

} SSTable;

SSTable *dump_memtable_to_sst(SkipList *sl, LString *filepath);
LString* search_in_sst(SSTable sst, LString *key);

#endif // PERSISTENCE_H
