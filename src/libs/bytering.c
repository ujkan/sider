#include "bytering.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// calculates (a + b) % n without using %
// by assuming (a + b) < 2n which should hold in a ring data struct
#define ADD_MOD_N(a, b, n)                                                     \
  (((a) + (b) >= (n)) ? ((a) + (b) - (n)) : ((a) + (b)))
#define ADD1_MOD_N(a, n) ADD_MOD_N(a, 1, n)
#define WRAP_INCR_N(i, n, cap)                                                 \
  ((i) = ((i) + (n) >= (cap) ? (((i) + (n)) - (cap)) : (i) + (n)))
#define WRAP_INCR(i, cap) WRAP_INCR_N(i, 1, cap)

#define SENTINEL UINT32_MAX

struct ByteRing *byte_ring_init(u32 cap) {
  struct ByteRing *br = malloc(sizeof(struct ByteRing));
  br->data = malloc(cap * sizeof(u8));
  br->cap = cap;
  br->start = SENTINEL;
  br->size = 0;
  return br;
}

u32 byte_ring_end(struct ByteRing *br) {
  if (br->size == 0) {
    return SENTINEL;
  }
  u32 br_end = ADD_MOD_N(br->start, br->size - 1, br->cap);
  return br_end;
}

void byte_ring_expand(struct ByteRing *br, u32 new_cap) {
  if (new_cap < br->cap) {
    // should expand!
    return;
  }
  br->data = realloc(br->data, new_cap);
  u32 br_end = byte_ring_end(br);
  if (br_end < br->start) {
    memcpy(br->data + br->cap, br->data, br_end + 1);
  }
  br->cap = new_cap;
}

void byte_ring_append_n(struct ByteRing *br, const u8 *buf, u32 n) {
  if (n == 0) {
    return;
  }
  if (n + br->size > br->cap) {
    // not enough space ==> expand
    byte_ring_expand(br, (n + br->size) * 2);
  }
  if (br->size == 0) {
    br->start = 0;
  }
  u32 write_idx = ADD1_MOD_N(byte_ring_end(br), br->cap);
  memcpy(br->data + write_idx, buf, n);
  br->size += n;
}

void byte_ring_pop_first_n(struct ByteRing *br, u32 len) {
  if (len > br->size) {
    // not enough elements
    return;
  }
  br->start = ADD_MOD_N(br->start, len, br->cap);
  br->size -= len;
}

void byte_ring_copy_n(struct ByteRing *br, u8 *buf, u32 start, u32 len) {
  if (len > br->size) {
    // not enough elements
    return;
  }
  start = ADD_MOD_N(start, br->start, br->cap);
  if (br->cap - start >= len) {
    memcpy(buf, br->data + start, len);
  } else {
    int first_part = (start + len) - br->cap;
    memcpy(buf, br->data + start, first_part);
    memcpy(buf, br->data, len - first_part);
  }
}

void byte_ring_copy_first_n(struct ByteRing *br, u8 *buf, u32 len) {
  byte_ring_copy_n(br, buf, 0, len);
}

void byte_ring_print_logical_order(struct ByteRing *br) {
  if (br->size == 0) {
    printf("[]\n");
    return;
  }
  printf("[");

  u32 j = br->start;
  for (u32 i = 0; i + 1 < br->size; i++) { // all but last el.
    isprint(br->data[j]) ? printf("%c", br->data[j]) : printf("_");
    printf(","); // comma!
    j = ADD1_MOD_N(j, br->cap);
  }
  printf("%c", br->data[j]); // last element without comma

  printf("]\n");
}

void byte_ring_print_physical_order(struct ByteRing *br) {
  if (br->size == 0) {
    printf("[]\n");
    return;
  }
  printf("[");

  u32 i;
  for (i = 0; i + 1 < br->cap; i++) { // all but last el.
    u32 br_end = byte_ring_end(br);

    if (((br->start < br_end) && (i >= br->start && i <= br_end)) ||
        ((br->start >= br_end) && !(i > br_end && i < br->start))) {
      isprint(br->data[i]) ? printf("%c", br->data[i]) : printf("_");
    } else {
      printf(" - ");
    }
    printf(","); // comma!
  }
  printf("%c", br->data[i]); // last element without comma

  printf("]\n");
}
void byte_ring_debug_print(struct ByteRing *br) {
  printf("br.start=%d\n", br->start);
  printf("br_end=%d\n", byte_ring_end(br));
  printf("br.size=%d\n", br->size);
  printf("br.cap=%d\n", br->cap);
  byte_ring_print_logical_order(br);
  byte_ring_print_physical_order(br);
}
