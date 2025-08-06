#include "bytering.h"
#include <assert.h>
#include <stdio.h>

static const u8 buf[10] = "abcdefghij";

void test_length_empty() {
  struct ByteRing *br;
  br = byte_ring_init(10);

  assert(br->size == 0);
}
void test_append_n_empty() {
  printf("test_append_n_empty\n");
  struct ByteRing *br;
  br = byte_ring_init(10);
  byte_ring_append_n(br, buf, 5);
  assert(br->size == 5);
  byte_ring_debug_print(br);
  printf("-------------------\n");
}
void test_append_n_empty_non_zero_start_nowrap() {
  printf("test_append_n_empty_non_zero_start_nowrap\n");
  struct ByteRing *br;
  br = byte_ring_init(10);
  br->start = 2;
  byte_ring_append_n(br, buf, 5);
  assert(br->size == 5);
  assert(br->start == 0);
  byte_ring_debug_print(br);
  printf("-------------------\n");
}
void test_append_n_empty_non_zero_start_wrap() {
  printf("test_append_n_empty_non_zero_start_wrap\n");
  struct ByteRing *br;
  br = byte_ring_init(10);
  br->start = 9;
  byte_ring_append_n(br, buf, 5);
  assert(br->size == 5);
  assert(br->start == 0);
  byte_ring_debug_print(br);
  printf("-------------------\n");
}
void test_append_n_start_le_end_nowrap() {
  printf("test_append_n_start_le_end_nowrap\n");
  struct ByteRing *br;
  br = byte_ring_init(10);
  br->start = 3;
  br->data[3] = buf[0];
  br->data[4] = buf[1];
  br->data[5] = buf[2];
  br->size = 3;
  assert(br->size == 3);

  byte_ring_append_n(br, buf + 3, 3);
  assert(br->size == 6);
  byte_ring_debug_print(br);
  printf("-------------------\n");
}
void test_append_n_start_le_end_wrap() {
  printf("test_append_n_start_le_end_wrap\n");
  struct ByteRing *br;
  br = byte_ring_init(10);
  br->start = 9;
  br->size = 1;
  br->data[9] = buf[0];
  assert(br->size == 1);
  printf("br_end=%d\n", byte_ring_end(br));
  assert(byte_ring_end(br) == 9);

  byte_ring_append_n(br, buf + 1, 3);
  assert(br->size == 4);
  byte_ring_debug_print(br);
  printf("-------------------\n");
}

void test_append_n_start_ge_end() {
  printf("test_append_n_start_ge_end\n");
  struct ByteRing *br;
  br = byte_ring_init(10);
  br->start = 9;
  br->size = 3;
  br->data[9] = buf[0];
  br->data[0] = buf[1];
  br->data[1] = buf[2];
  assert(br->size == 3);
  printf("br_end=%d\n", byte_ring_end(br));
  assert(byte_ring_end(br) == 1);

  byte_ring_append_n(br, buf + 3, 3);
  assert(br->size == 6);
  byte_ring_debug_print(br);
  printf("-------------------\n");
}
void test_append_n_start_eq_end() {
  printf("test_append_n_start_eq_end\n");
  struct ByteRing *br;
  br = byte_ring_init(10);
  br->start = 7;
  br->size = 0;
  printf("-------------------\n");
}
void test_append_n_len_exceeds() {
  printf("test_append_n_len_exceeds\n");
  struct ByteRing *br;
  br = byte_ring_init(3);
  byte_ring_append_n(br, buf, 8);
  assert(br->size == 8);
  byte_ring_append_n(br, buf, 10);
  assert(br->size == 18);
  byte_ring_debug_print(br);
  printf("-------------------\n");
}

void test_append_n_len_exceeds_non_zero_start() {
  printf("test_append_n_len_exceeds\n");
  struct ByteRing *br;
  br = byte_ring_init(5);
  br->data[3] = 'X';
  br->start = 3;
  br->size = 1;
  byte_ring_append_n(br, buf, 8);
  assert(br->size == 9);
  byte_ring_append_n(br, buf, 10);
  assert(br->size == 19);
  byte_ring_debug_print(br);
  printf("-------------------\n");
}

void test_pop_first_n_size_zero() {
  printf("test_pop_first_n_size_zero\n");
  struct ByteRing *br;
  br = byte_ring_init(5);
  byte_ring_pop_first_n(br, 3);
  assert(br->size == 0);
  printf("-------------------\n");
}
void test_pop_first_n_n_zero() {
  printf("test_pop_first_n_n_zero\n");
  struct ByteRing *br;
  br = byte_ring_init(5);
  byte_ring_append_n(br, buf, 5);
  byte_ring_pop_first_n(br, 0);
  assert(br->size == 5);
  byte_ring_debug_print(br);
  printf("-------------------\n");
}
void test_pop_first_n_n_exceeds() {
  printf("test_pop_first_n_exceeds\n");
  struct ByteRing *br;
  br = byte_ring_init(5);
  byte_ring_append_n(br, buf, 5);
  byte_ring_pop_first_n(br, 7);
  assert(br->size == 5);
  byte_ring_debug_print(br);
  printf("-------------------\n");
}
void test_pop_first_n_happy_path() {
  printf("test_pop_first_n_exceeds\n");
  struct ByteRing *br;
  br = byte_ring_init(5);
  byte_ring_append_n(br, buf, 5);
  byte_ring_pop_first_n(br, 3);
  assert(br->size == 2);
  byte_ring_debug_print(br);
  printf("-------------------\n");
}
int main(void) {
  test_length_empty();
  test_append_n_empty();
  test_append_n_empty_non_zero_start_nowrap();
  test_append_n_empty_non_zero_start_wrap();
  test_append_n_start_le_end_nowrap();
  test_append_n_start_le_end_wrap();
  test_append_n_start_ge_end();
  test_append_n_len_exceeds();
  test_append_n_len_exceeds_non_zero_start();
  test_pop_first_n_size_zero();
  test_pop_first_n_n_zero();
  test_pop_first_n_n_exceeds();
  test_pop_first_n_happy_path();
}
