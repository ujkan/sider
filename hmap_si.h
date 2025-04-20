typedef struct pair {
  char *key;
  int value;
} pair;

typedef struct bucket_item {
  pair p;
  struct bucket_item *next;
} bucket_item;

typedef bucket_item
    *bucket; // a bucket is a LL of bucket_item (ptr to root item)

typedef struct hashmap {
  bucket *data; // dynamic array of buckets
  int size;
  int cap;
  int (*hashfn)(char *, int);
} hashmap;

int hash(char *key, int kssize);

// bucket fns
bucket_item *bkappend(bucket b, char *key, int value);
bucket_item *bksearch(bucket b, char *key);
int bkdelete(bucket b, char *key);

// hashmap fns
void hashmap_init(hashmap *map, int init_cap);
void hashmap_destroy(hashmap *map);
void hashmap_rehash(hashmap *map);

int hashmap_upsert(hashmap *map, char *key, int value);
int hashmap_delete(hashmap *map, char *key);
int hashmap_get(hashmap *map, char *key);

void hashmap_print_keys_compact(hashmap *map);
void hashmap_print_keys(hashmap *map);
