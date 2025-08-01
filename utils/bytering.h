#ifndef BYTERING_H
#define BYTERING_H

#include <stdint.h>

#ifndef u32
typedef uint32_t u32;
#endif
#ifndef u8
typedef uint8_t u8;
#endif

struct ByteRing {
  u8 *data;
  u32 cap;
  u32 start;
  u32 size;
};

void byte_ring_init(struct ByteRing *br, u32 cap);
u32 byte_ring_end(struct ByteRing *br);
void byte_ring_expand(struct ByteRing *br, u32 new_cap);
void byte_ring_append_n(struct ByteRing *br, const u8 *buf, u32 len);
void byte_ring_pop_first_n(struct ByteRing *br, u32 len);
void byte_ring_copy_first_n(struct ByteRing *br, u8 *buf, u32 len);
void byte_ring_copy_n(struct ByteRing *br, u8 *buf, u32 start, u32 len);
void byte_ring_print(struct ByteRing *br);
void byte_ring_debug_print(struct ByteRing *br);

#endif // BYTERING_H
