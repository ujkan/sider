#ifndef HMAP_SI_H
#define HMAP_SI_H

typedef struct pair_si {
  char *key;
  int value;
} pair_si;

typedef struct bucket_item_si {
  pair_si p;
  struct bucket_item_si *next;
} bucket_item_si;

typedef bucket_item_si
    *bucket_si; // a bucket is a LL of bucket_item (ptr to root item)

typedef struct hashmap_si {
  bucket_si *data; // dynamic array of buckets
  int size;
  int cap;
  int (*hashfn)(char *, int);
} hashmap_si;

int hash(char *key, int kssize);

// bucket fns
bucket_item_si *bkappend(bucket_si b, char *key, int value);
bucket_item_si *bksearch(bucket_si b, char *key);
int bkdelete(bucket_si b, char *key);

// hashmap fns
void hashmap_init(hashmap_si *map, int init_cap);
void hashmap_destroy(hashmap_si *map);
void hashmap_rehash(hashmap_si *map);

int hashmap_upsert(hashmap_si *map, char *key, int value);
int hashmap_delete(hashmap_si *map, char *key);
int hashmap_get(hashmap_si *map, char *key);

void hashmap_print_keys_compact(hashmap_si *map);
void hashmap_print_keys(hashmap_si *map);

#endif // HMAP_SI_H