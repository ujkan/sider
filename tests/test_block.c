#include "block.h"
#include "hex_dump.h"
#include "stb_ds.h"
#include "u_array.h"
#include "unity.h"
#include "utils.h"
#include <lz4.h>

#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_append_entry_computes_shared_prefix(void) {
  struct DataBlockSingle block;
  DataBlockSingle_init(&block);

  LString *key = lstring_create_from_buf(6, "foobar");
  LString *value = lstring_create_from_buf(5, "value");
  LString *prev = lstring_create_from_buf(3, "foo");

  DataBlockSingle_append_entry(&block, prev, value, NULL);
  DataBlockSingle_append_entry(&block, key, value, prev);

  TEST_ASSERT_EQUAL_INT(2, arrlen(block.items));

  TEST_ASSERT_EQUAL_UINT16(0, block.items[0].shared);
  TEST_ASSERT_EQUAL_UINT16(3, block.items[0].suffix_len);
  TEST_ASSERT_EQUAL_MEMORY("foo", block.items[0].suffix, 3);
  TEST_ASSERT_EQUAL_UINT16(5, block.items[0].value_len);
  TEST_ASSERT_EQUAL_MEMORY("value", block.items[0].value, 5);

  TEST_ASSERT_EQUAL_UINT16(3, block.items[1].shared);
  TEST_ASSERT_EQUAL_UINT16(3, block.items[1].suffix_len);
  TEST_ASSERT_EQUAL_MEMORY("bar", block.items[1].suffix, 3);
  TEST_ASSERT_EQUAL_UINT16(5, block.items[1].value_len);
  TEST_ASSERT_EQUAL_MEMORY("value", block.items[1].value, 5);

  arrfree(block.items);
  arrfree(block.restart_points);
  lstring_free(key);
  lstring_free(value);
  lstring_free(prev);
}

static void test_append_item_stores_block_item(void) {
  struct DataBlockSingle block;
  DataBlockSingle_init(&block);

  struct BlockItem item = {
      .shared = 2,
      .suffix_len = 3,
      .suffix = (u8 *)"bar",
      .value_len = 4,
      .value = (u8 *)"data",
  };

  DataBlockSingle_append_item(&block, &item);

  TEST_ASSERT_EQUAL_INT(1, arrlen(block.items));
  TEST_ASSERT_EQUAL_UINT16(2, block.items[0].shared);
  TEST_ASSERT_EQUAL_UINT16(3, block.items[0].suffix_len);
  TEST_ASSERT_EQUAL_MEMORY("bar", block.items[0].suffix, 3);
  TEST_ASSERT_EQUAL_UINT16(4, block.items[0].value_len);
  TEST_ASSERT_EQUAL_MEMORY("data", block.items[0].value, 4);

  arrfree(block.items);
  arrfree(block.restart_points);
}

static void test_RestartPoint_serialize_into(void) {
  struct RestartPoint rp = {
      .key = {.len = 3, .data = (u8 *)"abc"},
      .offset = 0x11223344,
  };

  u8 *buf = malloc(32);
  u8 *cursor = buf;
  RestartPoint_serialize_into(&rp, &cursor);

  TEST_ASSERT_EQUAL_UINT16(9, cursor - buf);
  TEST_ASSERT_EQUAL_MEMORY("\x03\x00"
                           "abc",
                           buf, 5);
  TEST_ASSERT_EQUAL_MEMORY(&rp.offset, buf + 5, sizeof(rp.offset));

  free(buf);
}

static void test_BlockItem_serialize_into(void) {
  struct BlockItem item = {
      .shared = 2,
      .suffix_len = 3,
      .suffix = (u8 *)"bar",
      .value_len = 4,
      .value = (u8 *)"data",
  };
  u8 *buf = malloc(32);
  u8 *cursor = buf;
  BlockItem_serialize_into(&item, &cursor);
  // 02 00 03 00 bar 04 00 data
  TEST_ASSERT_EQUAL_INT(13, cursor - buf);
  TEST_ASSERT_EQUAL_MEMORY("\x02\x00\x03\x00"
                           "bar"
                           "\x04\x00"
                           "data",
                           buf, 13);
  free(buf);
}

static void test_DataBlockSingle_compress(void) {
  struct DataBlockSingle block;
  DataBlockSingle_init(&block);

  LString *prev = lstring_create_from_buf(8, "prefix/a");
  LString *key = lstring_create_from_buf(8, "prefix/b");
  LString *value = lstring_create_from_buf(5, "value");

  DataBlockSingle_append_entry(&block, prev, value, NULL);
  DataBlockSingle_append_entry(&block, key, value, prev);

  size_t original_size;
  LString *out = DataBlockSingle_compress(&block, &original_size);
  u8 *decompressed = malloc(original_size);
  LZ4_decompress_safe(out->data, decompressed, out->len, original_size);

  hex_dump(decompressed, 48);

  printf("block->restart_points[0]: %s\n", block.restart_points[0].key.data);
  // shared suff_len suffix    val
  // 0      8        prefix/a  value
  // 7      1        b         value
  TEST_ASSERT_EQUAL_MEMORY("\x00\x00\x08\x00"
                           "prefix/a",
                           decompressed, 12);
  TEST_ASSERT_EQUAL_MEMORY("\x05\x00value", decompressed + 12, 7);
  TEST_ASSERT_EQUAL_MEMORY("\x07\x00\x01\x00"
                           "b",
                           decompressed + 12 + 7, 5);
  TEST_ASSERT_EQUAL_MEMORY("\x05\x00value", decompressed + 12 + 7 + 5, 7);
  // one restart point only, and that is "prefix/a -> 0"
  TEST_ASSERT_EQUAL_MEMORY("\x08\x00"
                           "prefix/a",
                           decompressed + 12 + 7 + 5 + 7, 10);
  TEST_ASSERT_EQUAL_MEMORY("\x00\x00\x00\x00",
                           decompressed + 12 + 7 + 5 + 7 + 10, 4);

  arrfree(block.items);
  arrfree(block.restart_points);
  lstring_free(key);
  lstring_free(value);
  lstring_free(prev);
}
static void test_DataBlockSingle_append_many_buggy(void) {
  struct DataBlockSingle block;
  DataBlockSingle_init(&block);
  LString *prev = NULL;
  Array *pairs = test_utils_generate_kv_pairs(64);
  for (int i = 0; i < 64; i++) {
    struct SSTPair pair = array_index(pairs, struct SSTPair, i);
    // printf("%s\n", pair.key.data);
    DataBlockSingle_append_entry(&block, &pair.key, &pair.value, prev);
    prev = &pair.key;
  }

  LString *s = DataBlockSingle_serialize(&block);
  hex_dump(s->data, s->len);

  TEST_ASSERT_EQUAL_INT(2, arrlen(block.restart_points));
  TEST_ASSERT_EQUAL_INT(0, (block.restart_points[0].offset));
  // calculation:
  // 32 entries * length of value entries = 32 * len(vXXX) = 32 * 4
  // 32 entries * (sizeof(shared) + sizeof(suffix_len) + sizeof(value_len)) = 32
  // * 6 key000 in full so that's 6 then you have 1...9 as 1 char, and every 10s
  // you have a change, so 2 chars for that
  //    --> key000, 1, 2, 3, ..., 9, 10, 1, 2, 3, ..., 9, 20, ...
  //    --> so that's 11 chars per block of 10s
  // 3 * 11 + length(key000) + length(1) representing the "31" = 33 + 6 + 1 = 40
  // TOTAL = 32 * 6 + 32 * 4 + 40 = 32 * 10 + 40 = 320 + 40 = 360
  TEST_ASSERT_EQUAL_INT(360, (block.restart_points[1].offset));
  TEST_ASSERT_EQUAL_MEMORY("\x00\x00\x06\x00", s->data, 4);
  TEST_ASSERT_EQUAL_MEMORY("key000", s->data + 4, 6);
  for (int i = 1; i <= 9; i++) {
    char b[3];
    snprintf(b, sizeof(b), "%01d", i);
    TEST_ASSERT_EQUAL_MEMORY(b, (s->data + 20) + (i - 1) * 11, 1);
  }
}

static void test_DataBlockSingle_append_many(void) {
  struct DataBlockSingle block;
  DataBlockSingle_init(&block);
  struct SSTPair prev = {0};
  Array *pairs = test_utils_generate_kv_pairs(64);
  for (int i = 0; i < 64; i++) {
    struct SSTPair pair = array_index(pairs, struct SSTPair, i);
    // printf("%s\n", pair.key.data);
    DataBlockSingle_append_entry(&block, &pair.key, &pair.value, &prev.key);
    prev = pair;
  }

  LString *s = DataBlockSingle_serialize(&block);
  hex_dump(s->data, s->len);

  TEST_ASSERT_EQUAL_INT(2, arrlen(block.restart_points));
  TEST_ASSERT_EQUAL_INT(0, (block.restart_points[0].offset));
  // calculation:
  // 32 entries * length of value entries = 32 * len(vXXX) = 32 * 4
  // 32 entries * (sizeof(shared) + sizeof(suffix_len) + sizeof(value_len)) = 32
  // * 6 key000 in full so that's 6 then you have 1...9 as 1 char, and every 10s
  // you have a change, so 2 chars for that
  //    --> key000, 1, 2, 3, ..., 9, 10, 1, 2, 3, ..., 9, 20, ...
  //    --> so that's 11 chars per block of 10s
  // 3 * 11 + length(key000) + length(1) representing the "31" = 33 + 6 + 1 = 40
  // TOTAL = 32 * 6 + 32 * 4 + 40 = 32 * 10 + 40 = 320 + 40 = 360
  TEST_ASSERT_EQUAL_INT(360, (block.restart_points[1].offset));
  TEST_ASSERT_EQUAL_MEMORY("\x00\x00\x06\x00", s->data, 4);
  TEST_ASSERT_EQUAL_MEMORY("key000", s->data + 4, 6);
  for (int i = 1; i <= 9; i++) {
    char b[3];
    snprintf(b, sizeof(b), "%01d", i);
    TEST_ASSERT_EQUAL_MEMORY(b, (s->data + 20) + (i - 1) * 11, 1);
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_append_entry_computes_shared_prefix);
  RUN_TEST(test_append_item_stores_block_item);
  RUN_TEST(test_RestartPoint_serialize_into);
  RUN_TEST(test_BlockItem_serialize_into);
  RUN_TEST(test_DataBlockSingle_compress);
  RUN_TEST(test_DataBlockSingle_append_many);
  return UNITY_END();
}
