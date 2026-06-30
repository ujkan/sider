#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

LString *test_utils_make_lstring(const char *buf) {
  return lstring_create_from_buf((int)strlen(buf), buf);
}

struct SSTPair test_utils_make_sst_pair(u16 tag, const char *key, u8 tombstone,
                                        const char *value) {
  struct SSTPair pair = {0};
  pair.tag = tag;
  pair.key = *test_utils_make_lstring(key);
  pair.tombstone = tombstone;
  pair.value = *test_utils_make_lstring(value);
  return pair;
}

static const u8 *kx[] = {
    "a01",   "a04", "a08",  "a15", "a211", "a212", "a214",
    "a2141", "b",   "b000", "b1",  "c",    "z12",
};

void test_utils_free_sst_pair(void *ptr) {
  struct SSTPair *pair = ptr;
  free(pair->key.data);
  free(pair->value.data);
}

Array *test_utils_generate_kv_pairs_with_prefix(u32 count,
                                                const char *key_prefix,
                                                const char *value_prefix) {
  Array *pairs =
      array_new_full(count, sizeof(struct SSTPair), test_utils_free_sst_pair);
  for (u32 i = 0; i < count; i++) {
    char key_buf[64];
    char value_buf[64];
    snprintf(key_buf, sizeof(key_buf), "%s%03u", key_prefix, i);
    snprintf(value_buf, sizeof(value_buf), "%s%03u", value_prefix, i);
    struct SSTPair pair = test_utils_make_sst_pair(0, key_buf, 0, value_buf);
    array_push(pairs, &pair);
  }
  return pairs;
}

Array *test_utils_generate_kv_pairs(u32 count) {
  return test_utils_generate_kv_pairs_with_prefix(count, "key", "v");
}

Array *generate_random_pairs(void) { return test_utils_generate_kv_pairs(512); }

void test_utils_free_sst_pairs(Array *pairs) {
  if (pairs == NULL) {
    return;
  }
  array_set_length(pairs, 0);
  free(pairs->data);
  free(pairs);
}
