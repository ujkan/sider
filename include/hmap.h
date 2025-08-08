#ifndef HMAP_H
#define HMAP_H

typedef struct pair {
  char *key;
  char *value;
} pair;

typedef struct bucket_item {
  pair p;
  struct bucket_item *next;
} bucket_item;


typedef struct hashmap {
  bucket_item **data; // dynamic array of buckets
  int size;
  int cap;
  int (*hashfn)(char *, int);
} hashmap;

int hash(char *key, int kssize);

// bucket fns
bucket_item *bucket_append_entry(bucket_item *head, char *key, char *value);
bucket_item *bucket_search_key(bucket_item *head, char *key);
int bucket_delete_key(bucket_item **b, char *key);

// hashmap fns
void hashmap_init(hashmap *map, int init_cap);
void hashmap_destroy(hashmap *map);
void hashmap_rehash(hashmap *map);

int hashmap_upsert(hashmap *map, char *key, char *value);
int hashmap_delete(hashmap *map, char *key);
char *hashmap_get(hashmap *map, char *key);

void hashmap_print_keys_compact(hashmap *map);
void hashmap_print_entries_compact(hashmap *map);
void hashmap_print_keys(hashmap *map);

#endif // HMAP_H
