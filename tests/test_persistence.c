#include "hex_dump.h"
#include "persistence.h"
#include "unity.h"

#include "stb_ds.h"
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_compress_and_write_two_pairs(void) {
  LString first_key = {.data = (u8 *)strdup("key000"), .len = 6};
  LString first_value = {.data = (u8 *)strdup("v000"), .len = 4};
  LString second_key = {.data = (u8 *)strdup("key001"), .len = 6};
  LString second_value = {.data = (u8 *)strdup("v001"), .len = 4};

  u8 buf[4096] = {0};
  int written_len = 0;

  LString **keys = NULL;
  LString **values = NULL;
  arrpush(keys, &first_key);
  arrpush(values, &first_value);
  arrpush(keys, &second_key);
  arrpush(values, &second_value);

  compress_and_write(keys, values, (char *)buf, &written_len);

  TEST_ASSERT_GREATER_THAN_INT(0, written_len);
  hex_dump(buf, 60);
  TEST_ASSERT_EQUAL_MEMORY("\x1b\x00\x00\x00", buf, 4);

  arrfree(keys);
  arrfree(values);

  free(first_key.data);
  free(first_value.data);
  free(second_key.data);
  free(second_value.data);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_compress_and_write_two_pairs);
  return UNITY_END();
}
