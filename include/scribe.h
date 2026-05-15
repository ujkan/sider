#ifndef SCRIBE_H
#define SCRIBE_H

#include "lstr.h"
#include "types.h"
#include <stddef.h>

/*
 * All integer helpers use little-endian byte order.
 * `scribe_put_bytes` / `scribe_get_bytes` are for raw payload bytes only.
 */

void scribe_put_u8(u8 **dst, u8 value);
u8 scribe_get_u8(const u8 **src);

void scribe_put_u16(u8 **dst, u16 value);
u16 scribe_get_u16(const u8 **src);

void scribe_put_u32(u8 **dst, u32 value);
u32 scribe_get_u32(const u8 **src);

void scribe_put_u64(u8 **dst, u64 value);
u64 scribe_get_u64(const u8 **src);

void scribe_put_i16(u8 **dst, i16 value);
i16 scribe_get_i16(const u8 **src);

void scribe_put_i32(u8 **dst, i32 value);
i32 scribe_get_i32(const u8 **src);

void scribe_put_i64(u8 **dst, i64 value);
i64 scribe_get_i64(const u8 **src);

void scribe_put_ptr(u8 **dst, const void *ptr);
void *scribe_get_ptr(const u8 **src);

void scribe_put_bytes(u8 **dst, const void *src, size_t len);
void scribe_get_bytes(const u8 **src, void *dst, size_t len);

void scribe_put_lstring(u8 **dst, const LString *s);
LString *scribe_get_lstring(const u8 **src);

#endif // SCRIBE_H
