#ifndef HMAP_SI_H
#define HMAP_SI_H

#include "lstr.h"

typedef struct bucket_item_si {
  LString *key;
  int value;
  struct bucket_item_si *next;
} bucket_item_si;


typedef struct hashmap_si {
  bucket_item_si **data; // dynamic array of buckets
  int size;
  int cap;
  int (*hashfn)(LString *, int);
} hashmap_si;

int hash(LString *key, int kssize);

// bucket fns
bucket_item_si *bucket_si_append_entry(bucket_item_si *head, LString *key, int value);
bucket_item_si *bucket_si_search_key(bucket_item_si *head, LString *key);
int bucket_si_delete_key(bucket_item_si **b, LString *key);

// hashmap fns
void hashmap_si_init(hashmap_si *map, int init_cap);
void hashmap_si_destroy(hashmap_si *map);
void hashmap_si_rehash(hashmap_si *map);

int hashmap_si_insert(hashmap_si *map, LString *key, int value);
int hashmap_si_upsert(hashmap_si *map, LString *key, int value);
int hashmap_si_delete(hashmap_si *map, LString *key);
int hashmap_si_get(hashmap_si *map, LString *key);

void hashmap_si_print_keys_compact(hashmap_si *map);
void hashmap_si_print_entries_compact(hashmap_si *map);
void hashmap_si_print_keys(hashmap_si *map);

#endif // HMAP_SI_H
