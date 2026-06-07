#include "hex_dump.h"
#include "index_block.h"
#include "lstr.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_IndexItem_serialize() {
  LString *key = lstring_create_from_buf(6, "foobar");
  u32 offset = 6;
  struct IndexItem iitem = {
      .key = key,
      .offset = offset,
  };
  LString *serialized = IndexItem_serialize(&iitem);

  int length = 6;
  int size_of_length = sizeof(key->len);
  int size_of_offset = sizeof(offset);
  TEST_ASSERT_EQUAL_UINT16(length + size_of_length + size_of_offset,
                           serialized->len);
}

void test_IndexSection_deserialize() {

  // key,offset
  // foo,1
  // lala,16
  // hello,32
  // x,64
  char *buf = "\x03\x00"
              "foo"
              "\x01\x00\x00\x00"
              "\x04\x00"
              "lala"
              "\x10\x00\x00\x00"
              "\x05\x00"
              "hello"
              "\x20\x00\x00\x00"
              "\x01\x00"
              "x"
              "\x40\x00\x00\x00";

  // len("foo") + len("lala") + len("hello") + len("x") = 3 + 4 + 5 + 1 + 4
  // sizeof(offset) = 4, sizeoff(key.len) = 2 => 4 * (2 + 4)
  struct IndexSection *s =
      IndexSection_deserialize(buf, 3 + 4 + 5 + 1 + 4 * (2 + 4));
  TEST_ASSERT_EQUAL_INT(4, arrlen(s->items));
  TEST_ASSERT_EQUAL_UINT16(3, s->items[0].key->len);
  TEST_ASSERT_EQUAL_MEMORY("foo", s->items[0].key->data, 3);
  TEST_ASSERT_EQUAL_UINT16(4, s->items[1].key->len);
  TEST_ASSERT_EQUAL_MEMORY("lala", s->items[1].key->data, 4);
  TEST_ASSERT_EQUAL_UINT16(5, s->items[2].key->len);
  TEST_ASSERT_EQUAL_MEMORY("hello", s->items[2].key->data, 5);
  TEST_ASSERT_EQUAL_UINT16(1, s->items[3].key->len);
  TEST_ASSERT_EQUAL_MEMORY("x", s->items[3].key->data, 1);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_IndexItem_serialize);
  RUN_TEST(test_IndexSection_deserialize);
  return UNITY_END();
}
