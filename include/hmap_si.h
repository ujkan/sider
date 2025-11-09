#ifndef HMAP_H
#define HMAP_H

#include "lstr.h"

typedef struct bucket_item {
  LString *key;
  int value;
  struct bucket_item *next;
} bucket_item;


typedef struct hashmap_si {
  bucket_item **data; // dynamic array of buckets
  int size;
  int cap;
  int (*hashfn)(LString *, int);
} hashmap_si;

int hash(LString *key, int kssize);

// bucket fns
bucket_item *bucket_append_entry(bucket_item *head, LString *key, int value);
bucket_item *bucket_search_key(bucket_item *head, LString *key);
int bucket_delete_key(bucket_item **b, LString *key);

// hashmap fns
void hashmap_si_init(hashmap_si *map, int init_cap);
void hashmap_si_destroy(hashmap_si *map);
void hashmap_si_rehash(hashmap_si *map);

int hashmap_si_upsert(hashmap_si *map, LString *key, int value);
int hashmap_si_delete(hashmap_si *map, LString *key);
int hashmap_si_get(hashmap_si *map, LString *key);

void hashmap_si_print_keys_compact(hashmap_si *map);
void hashmap_si_print_entries_compact(hashmap_si *map);
void hashmap_si_print_keys(hashmap_si *map);

#endif // HMAP_H
