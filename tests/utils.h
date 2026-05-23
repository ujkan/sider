#ifndef TESTS_UTILS_H
#define TESTS_UTILS_H

#include "lstr.h"
#include "types.h"
#include "u_array.h"

struct SSTPair {
  u16 tag;
  LString key;
  u8 tombstone;
  LString value;
};

LString *test_utils_make_lstring(const char *buf);
struct SSTPair test_utils_make_sst_pair(u16 tag, const char *key,
                                        u8 tombstone, const char *value);
Array *test_utils_generate_kv_pairs(u32 count);
Array *test_utils_generate_kv_pairs_with_prefix(u32 count,
                                                const char *key_prefix,
                                                const char *value_prefix);
void test_utils_free_sst_pair(void *ptr);
void test_utils_free_sst_pairs(Array *pairs);

/* Backward-compatible helper name for existing tests. */
Array *generate_random_pairs(void);

#endif // TESTS_UTILS_H
