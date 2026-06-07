#pragma once

#include "lstr.h"
#include "skiplist_str.h"
#include "u_array.h"

typedef struct {
  LString *filepath;
  unsigned int size;
  LString min;
  LString max;
} SSTable;

SSTable *dump_memtable_to_sst(SkipList *sl, LString *filepath);
void compress_and_write(LString **keys, LString **values, char *write_buf,
                           int *written_len);
LString* search_in_sst(SSTable sst, LString *key);
void merge_and_compact_level_zero(Array *sstables);
