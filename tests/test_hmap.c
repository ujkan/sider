#include "hmap.h"
#include "log_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int dumb_hashfn(char *key, int size) { return 0; }

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
#ifdef DEBUG
  hashmap_print_keys(&map);
#endif /* ifdef DEBUG */
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

#ifdef DEBUG
    if ((i & (i - 1)) == 0) {
      hashmap_print_keys_compact(&map);
      PRINT("-----------");
    }
#endif /* ifdef DEBUG */
  }
#ifdef DEBUG
  hashmap_print_keys_compact(&map);
#endif

  hashmap_destroy(&map);
}

void test_get() {
  printf("test_get - ");
  int failed = 0;
  hashmap map;
  hashmap_init(&map, 3);
  hashmap_upsert(&map, "k1", "v1");
  hashmap_upsert(&map, "k2", "v2");
  hashmap_upsert(&map, "k3", "v3");
  hashmap_upsert(&map, "k4", "v4");
  hashmap_upsert(&map, "k5", "v5");
  if (strcmp(hashmap_get(&map, "k1"), "v1") != 0) {
    failed = 1;
  }
  if (strcmp(hashmap_get(&map, "k2"), "v2") != 0) {
    failed = 1;
  }
  if (strcmp(hashmap_get(&map, "k3"), "v3") != 0) {
    failed = 1;
  }
  if (strcmp(hashmap_get(&map, "k4"), "v4") != 0) {
    failed = 1;
  }
  if (strcmp(hashmap_get(&map, "k5"), "v5") != 0) {
    failed = 1;
  }
  if (failed == 1) {
    FAIL;
  } else {
    PASS;
  }
}

int main(void) {
  test_bksearch();
  test_bkappend_null();
  test_bkappend_not_null();
  test_upsert_bucket_is_null();
  test_upsert_bucket_not_null();
  test_upsert_rehash_bucket_not_null();
  test_upsert();
  test_get();
}
