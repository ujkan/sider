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
              "gala"
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
  TEST_ASSERT_EQUAL_MEMORY("gala", s->items[1].key->data, 4);
  TEST_ASSERT_EQUAL_UINT16(5, s->items[2].key->len);
  TEST_ASSERT_EQUAL_MEMORY("hello", s->items[2].key->data, 5);
  TEST_ASSERT_EQUAL_UINT16(1, s->items[3].key->len);
  TEST_ASSERT_EQUAL_MEMORY("x", s->items[3].key->data, 1);
}

void test_IndexSection_search() {
  char *index = "\x03\x00"
                "foo"
                "\x01\x00\x00\x00"
                "\x04\x00"
                "gala"
                "\x10\x00\x00\x00"
                "\x05\x00"
                "hello"
                "\x20\x00\x00\x00"
                "\x01\x00"
                "x"
                "\x40\x00\x00\x00";
  u32 restart_points[4] = {0};
  restart_points[0] = 0;
  restart_points[1] = 9;
  restart_points[2] = 19;
  restart_points[3] = 30;

  LString *key1 = lstring_create_from_buf(3, "foo");
  struct IndexItem *item1 = IndexSection_search(index, restart_points, 4, key1);
  TEST_ASSERT_EQUAL_UINT32(0x1, item1->offset);

  LString *key2 = lstring_create_from_buf(4, "gala");
  struct IndexItem *item2 = IndexSection_search(index, restart_points, 4, key2);
  TEST_ASSERT_EQUAL_UINT32(0x10, item2->offset);

  LString *key3 = lstring_create_from_buf(5, "hello");
  struct IndexItem *item3 = IndexSection_search(index, restart_points, 4, key3);
  TEST_ASSERT_EQUAL_UINT32(0x20, item3->offset);

  LString *key4 = lstring_create_from_buf(1, "x");
  struct IndexItem *item4 = IndexSection_search(index, restart_points, 4, key4);
  TEST_ASSERT_EQUAL_UINT32(0x40, item4->offset);

  // nearest to "goal" is "hello" (it rounds 'up')
  LString *key5 = lstring_create_from_buf(4, "goal");
  struct IndexItem *item5 = IndexSection_search(index, restart_points, 4, key5);
  TEST_ASSERT_EQUAL_UINT32(item3->offset, item5->offset);

  // nearest to "a"  is "foo"
  LString *key6 = lstring_create_from_buf(1, "a");
  struct IndexItem *item6 = IndexSection_search(index, restart_points, 4, key6);
  TEST_ASSERT_EQUAL_UINT32(item1->offset, item6->offset);

  // nearest to "z" is "x"
  LString *key7 = lstring_create_from_buf(1, "z");
  struct IndexItem *item7 = IndexSection_search(index, restart_points, 4, key7);
  TEST_ASSERT_EQUAL_UINT32(item4->offset, item7->offset);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_IndexItem_serialize);
  RUN_TEST(test_IndexSection_deserialize);
  RUN_TEST(test_IndexSection_search);
  return UNITY_END();
}
