#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct pair {
  char *key;
  char *value;
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

#define PRINT(format, ...) printf(format "\n", ##__VA_ARGS__)
#ifdef DEBUG
#define DPRINT(format, ...) printf(format "\n", ##__VA_ARGS__)
#else
#define DPRINT(format, ...) ((void)0) /* do nothing */
#endif
#define PASS PRINT("Test passed");
#define FAIL PRINT("Test failed");

int hash(char *key, int kssize);

int hash(char *key, int kssize) {
  unsigned int h = 0;
  while (*key) {
    h += (unsigned char)(*key++);
  }
  return h % kssize;
}
int hash_fnv1(char *key, int kssize) {
  // FNV-1a hash algorithm constants
  const unsigned int FNV_PRIME = 16777619;
  const unsigned int FNV_OFFSET_BASIS = 2166136261;

  // Initialize hash with the offset basis
  unsigned int hash = FNV_OFFSET_BASIS;

  // Process each byte in the key
  for (char *p = key; *p != '\0'; p++) {
    hash ^= (unsigned char)*p; // XOR with the current byte
    hash *= FNV_PRIME;         // Multiply by the prime
  }

  // Return the hash value within the key space
  return (int)(hash % kssize);
}

bucket_item *bkappend(bucket b, char *key, char *value) {
  bucket_item *new_item = malloc(sizeof(bucket_item));
  new_item->p = (pair){.key = key, .value = value};

  bucket_item *curr = b;
  if (curr == NULL) {
    return new_item; // return new root
  }
  // go to end and adjust (old_last)->next
  for (; curr->next != NULL; curr = curr->next) {
  }
  curr->next = new_item;
  return b; // return root
}

// maybe return ptr to bucket_item to avoid returning structs
bucket_item *bksearch(bucket b, char *key) {
  // db "123"
  for (bucket_item *curr = b; curr != NULL; curr = curr->next) {
    if (strcmp((curr->p).key, key) == 0) {
      return curr;
    }
  }
  // db guess: -1 since curr == NULL
  return NULL;
}

void hashmap_rehash(hashmap *map) {
  map->cap *= 2;
  bucket *new_data = malloc(map->cap * sizeof(bucket)); // sizeof correct here?
  // when rehashing, can we break collisions? probably should try
  bucket curr;
  for (int i = 0; i < map->size; i++) {
    curr = (map->data)[i];
    // for each entry in old bucket append to a bucket in the new data
    while (curr != NULL) {
      char *key = (curr->p).key;
      int index = (map->hashfn)(key, map->cap);
      new_data[index] = bkappend(new_data[index], key, curr->p.value);
      curr = curr->next;
    }
  }

  free(map->data);

  map->data = new_data; // use after free? or assignment ok?
}

void hashmap_print_keys_compact(hashmap *map) {
  bucket curr;
  for (int i = 0; i < map->cap; i++) {
    curr = map->data[i];
    if (curr == NULL) {
      continue;
    }
    printf("%3d  ", i);
    for (; curr != NULL; curr = curr->next) {
      printf("%s -> ", curr->p.key);
    }
    printf("/\n");
  }
}

void hashmap_print_keys(hashmap *map) {
  bucket curr;
  for (int i = 0; i < map->cap; i++) {
    curr = map->data[i];
    PRINT("bucket %d", i);
    for (; curr != NULL; curr = curr->next) {
      PRINT("  key=%s", curr->p.key);
    }
  }
}

int hashmap_upsert(hashmap *map, char *key, char *value) {
  if (map->size < map->cap) {
    bucket *data = map->data;

    int index = (map->hashfn)(key, map->cap);
    // db index = some val <128
    //  data[i] stores pointer to heap
    //  where char* is stored
    //  question: why not just use char*?
    bucket_item *search_item = bksearch(data[index], key);
    if (search_item == NULL) { // no exist, insert
      pair p = {.key = key, .value = value};
      data[index] = bkappend(data[index], key, value);
      map->size++;
    } else { // exist
      search_item->p.value = value;
    }
    return index;
  } else {
    hashmap_rehash(map);
    return hashmap_upsert(
        map, key,
        value); // can i reuse? or is recursion dangerous; should i
                // keep a reusable safe upsert to avoid recursion?
  }
  return -1;
}

char *hashmap_get(hashmap *map, char *key) {
  int index = (map->hashfn)(key, map->cap);
  bucket bkt = map->data[index];
  bucket_item *search_item = bksearch(bkt, key);
  if (search_item == NULL) {
    return NULL;
  } else {
    return search_item->p.value;
  }
}

void hashmap_init(hashmap *map, int init_cap) {
  map->cap = init_cap;
  map->size = 0;
  map->data = malloc((map->cap) * sizeof(map->data));
  map->hashfn = hash_fnv1;
}

void hashmap_destroy(hashmap *map) {
  for (int i = 0; i < map->cap; i++) {
    bucket_item *curr = map->data[i];
    while (curr != NULL) {
      bucket_item *tmp = curr;
      curr = curr->next;
      free(tmp);
    }
  }
  free(map->data);
}

void test_bksearch() {
  printf("test_bksearch - ");
  bucket_item *item = bksearch(NULL, "abc");
  if (item == NULL) {
    PASS;
  } else {
    FAIL;
  }
}

void test_bkappend_null() {
  printf("test_bkappend_null - ");
  bucket_item *item = bkappend(NULL, "key", "value");
  if (strcmp(item->p.key, "key") == 0 && strcmp(item->p.value, "value") == 0 &&
      item->next == NULL) {
    PASS;
  } else {
    FAIL;
    PRINT("  item key: %s", item->p.key);
    PRINT("  item key strcmp: %d", strcmp(item->p.key, "key"));
    PRINT("  item value: %s", item->p.value);
    PRINT("  item next: %p", item->next);
  }
}

void test_bkappend_not_null() {
  printf("test_bkappend_not_null - ");
  int failed = 0;
  bucket root = malloc(sizeof(bucket));
  root->p = (pair){.key = "1", .value = "1"};
  bucket_item *item1 = malloc(sizeof(bucket_item));
  item1->p = (pair){.key = "2", .value = "2"};

  root->next = item1;

  char *key = "key";
  char *value = "value";
  bucket_item *retval = bkappend(root, key, value);

  if (retval != root) {
    failed = 1;
  }
  if (strcmp(item1->next->p.key, key) != 0 ||
      strcmp(item1->next->p.value, value) != 0) {
    failed = 1;
  }
  if (item1->next->next != NULL) {
    failed = 1;
  }

  if (!failed) {
    PASS;
  } else {
    FAIL;
    PRINT("  item1 -> next: %p", item1->next);
    PRINT("  item1 -> next -> key=%s", item1->next->p.key);
    PRINT("  item1 -> next -> value=%s", item1->next->p.value);
  }
}

void test_upsert_bucket_is_null() {
  printf("test_upsert_bucket_is_null - ");
  hashmap map;
  hashmap_init(&map, 6);
  int init_cap = map.cap;
  int init_size = map.size;
  int index = hashmap_upsert(&map, "key", "value");
  if (strcmp(map.data[index]->p.key, "key") == 0 &&
      strcmp(map.data[index]->p.value, "value") == 0 &&
      map.size == init_size + 1 && map.cap == init_cap) {
    PASS;
  } else {
    FAIL;
    bucket_item *item = map.data[index];
    PRINT("  item key: %s", item->p.key);
    PRINT("  item value: %s", item->p.value);
    PRINT("  item next: %p", item->next);
    PRINT("  map size: %d", map.size);
    PRINT("  map cap: %d", map.cap);
  }
  hashmap_destroy(&map);
}

int dumb_hashfn(char *key, int size) { return 0; }

void test_upsert_bucket_not_null() {
  printf("test_upsert_bucket_not_null - ");
  int failed = 0;

  // setting up
  hashmap map;
  hashmap_init(&map, 6);
  map.hashfn = dumb_hashfn;
  // inserting some elements in same bucket
  hashmap_upsert(&map, "key1", "value1");
  DPRINT("  after first upsert");
  DPRINT("    data[0] key=%s", map.data[0]->p.key);
  if (strcmp(map.data[0]->p.key, "key1") != 0) {
    failed = 1;
  }
  hashmap_upsert(&map, "key2", "value2");
  DPRINT("  after second upsert");
  DPRINT("    data[0] key=%s", map.data[0]->p.key);
  DPRINT("    data[0] next key=%s", map.data[0]->next->p.key);
  if (strcmp(map.data[0]->p.key, "key1") != 0) {
    failed = 1;
  }
  if (strcmp(map.data[0]->next->p.key, "key2") != 0) {
    failed = 1;
  }

  // this is starting state
  int init_cap = map.cap;
  int init_size = map.size;
  char *key = "expected_key";
  char *value = "expected_value";

  // when
  int index = hashmap_upsert(&map, key, value);

  // then
  if (strcmp(map.data[index]->next->next->p.key, key) == 0 &&
      strcmp(map.data[index]->next->next->p.value, value) == 0 &&
      map.size == init_size + 1 && map.cap == init_cap) {
    PASS;
  } else {
    FAIL;
    bucket_item *item = map.data[index]->next->next;
    PRINT("  index: %d", index);
    PRINT("  item key: %s", item->p.key);
    PRINT("  item value: %s", item->p.value);
    PRINT("  item next: %p", item->next);
    PRINT("  map size: %d", map.size);
    PRINT("  map cap: %d", map.cap);
  }
  hashmap_destroy(&map);
}

void test_upsert_rehash_bucket_not_null() {
  printf("test_upsert_rehash_bucket_not_null - ");
  int failed = 0;

  // setting up
  hashmap map;
  hashmap_init(&map, 2);
  map.hashfn = dumb_hashfn;
  // inserting some elements in same bucket
  hashmap_upsert(&map, "key1", "value1");
  DPRINT("  after first upsert");
  DPRINT("    data[0] key=%s", map.data[0]->p.key);
  if (strcmp(map.data[0]->p.key, "key1") != 0) {
    failed = 1;
  }
  hashmap_upsert(&map, "key2", "value2");
  DPRINT("  after second upsert");
  DPRINT("    data[0] key=%s", map.data[0]->p.key);
  DPRINT("    data[0] next key=%s", map.data[0]->next->p.key);
  if (strcmp(map.data[0]->p.key, "key1") != 0) {
    failed = 1;
  }
  if (strcmp(map.data[0]->next->p.key, "key2") != 0) {
    failed = 1;
  }

  // this is starting state
  int init_cap = map.cap;
  int init_size = map.size;
  char *key = "expected_key";
  char *value = "expected_value";

  // when
  // rehash will occur, so expected cap is 2*init_cap
  int index = hashmap_upsert(&map, key, value);

  // then
  if (strcmp(map.data[index]->next->next->p.key, key) == 0 &&
      strcmp(map.data[index]->next->next->p.value, value) == 0 &&
      map.size == init_size + 1 && map.cap == 2 * init_cap) {
    PASS;
  } else {
    FAIL;
    bucket_item *item = map.data[index]->next->next;
    PRINT("  index: %d", index);
    PRINT("  item key: %s", item->p.key);
    PRINT("  item value: %s", item->p.value);
    PRINT("  item next: %p", item->next);
    PRINT("  map size: %d", map.size);
    PRINT("  map cap: %d", map.cap);
  }
  hashmap_print_keys(&map);
  hashmap_destroy(&map);
}
/**
 * C++ version 0.4 char* style "itoa":
 * Written by Lukás Chmela
 * Released under GPLv3.
 */
char *itoa(int value, char *result, int base) {
  // check that the base if valid
  if (base < 2 || base > 36) {
    *result = '\0';
    return result;
  }

  char *ptr = result, *ptr1 = result, tmp_char;
  int tmp_value;

  do {
    tmp_value = value;
    value /= base;
    *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrst"
             "uvwxyz"[35 + (tmp_value - value * base)];
  } while (value);

  // Apply negative sign
  if (tmp_value < 0)
    *ptr++ = '-';
  *ptr-- = '\0';

  // Reverse the string
  while (ptr1 < ptr) {
    tmp_char = *ptr;
    *ptr-- = *ptr1;
    *ptr1++ = tmp_char;
  }
  return result;
}
void test_upsert() {
  printf("test_upsert - ");
  hashmap map;
  hashmap_init(&map, 2);

  for (int i = 1; i <= 128; i++) {
    char *to_str = malloc(5 * sizeof(char));
    itoa(i, to_str, 10);
    hashmap_upsert(&map, to_str, "value");
    if ((i & (i - 1)) == 0) {
      hashmap_print_keys_compact(&map);
      PRINT("-----------");
    }
  }
  hashmap_print_keys_compact(&map);

  hashmap_destroy(&map);
}

int main(void) {
  test_bksearch();
  test_bkappend_null();
  test_bkappend_not_null();
  test_upsert_bucket_is_null();
  test_upsert_bucket_not_null();
  test_upsert_rehash_bucket_not_null();
}

// int main(void) {
//   hashmap map;
//   hashmap_init(&map);
//   printf("%d\n", hash("1", 128));
//   printf("%d\n", hash("2", 128));
//   printf("%d\n", hash("3", 128));
//   hashmap_upsert(&map, "1", "abc");
//   hashmap_upsert(&map, "2", "xyz");
//   hashmap_upsert(&map, "3", "abc");
//   printf("%s\n", map.data[0]->p.value);
// }
