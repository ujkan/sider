#include "index_block.h"
#include "lstr.h"
#include "unity.h"

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

  TEST_ASSERT_EQUAL_UINT16(6+2+4, serialized->len);

}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_IndexItem_serialize);
  return UNITY_END();
}
