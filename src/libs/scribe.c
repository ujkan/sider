/*
* AI generated
*
*/
#include "scribe.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void scribe_put_u64_le(u8 **dst, u64 value, size_t width) {
  for (size_t i = 0; i < width; i++) {
    (*dst)[i] = (u8)((value >> (8 * i)) & 0xffu);
  }
  *dst += width;
}

static u64 scribe_get_u64_le(const u8 **src, size_t width) {
  u64 value = 0;
  for (size_t i = 0; i < width; i++) {
    value |= ((u64)(*src)[i]) << (8 * i);
  }
  *src += width;
  return value;
}

void scribe_put_u8(u8 **dst, u8 value) {
  **dst = value;
  *dst += 1;
}

u8 scribe_get_u8(const u8 **src) {
  u8 value = **src;
  *src += 1;
  return value;
}

void scribe_put_u16(u8 **dst, u16 value) {
  scribe_put_u64_le(dst, (u64)value, sizeof(value));
}

u16 scribe_get_u16(const u8 **src) {
  return (u16)scribe_get_u64_le(src, sizeof(u16));
}

void scribe_put_u32(u8 **dst, u32 value) {
  scribe_put_u64_le(dst, (u64)value, sizeof(value));
}

u32 scribe_get_u32(const u8 **src) {
  return (u32)scribe_get_u64_le(src, sizeof(u32));
}

void scribe_put_u64(u8 **dst, u64 value) {
  scribe_put_u64_le(dst, value, sizeof(value));
}

u64 scribe_get_u64(const u8 **src) {
  return scribe_get_u64_le(src, sizeof(u64));
}

void scribe_put_i16(u8 **dst, i16 value) {
  scribe_put_u16(dst, (u16)value);
}

i16 scribe_get_i16(const u8 **src) {
  return (i16)scribe_get_u16(src);
}

void scribe_put_i32(u8 **dst, i32 value) {
  scribe_put_u32(dst, (u32)value);
}

i32 scribe_get_i32(const u8 **src) {
  return (i32)scribe_get_u32(src);
}

void scribe_put_i64(u8 **dst, i64 value) {
  scribe_put_u64(dst, (u64)value);
}

i64 scribe_get_i64(const u8 **src) {
  return (i64)scribe_get_u64(src);
}

void scribe_put_ptr(u8 **dst, const void *ptr) {
  scribe_put_u64_le(dst, (u64)(uintptr_t)ptr, sizeof(uintptr_t));
}

void *scribe_get_ptr(const u8 **src) {
  return (void *)(uintptr_t)scribe_get_u64_le(src, sizeof(uintptr_t));
}

void scribe_put_bytes(u8 **dst, const void *src, size_t len) {
  memcpy(*dst, src, len);
  *dst += len;
}

void scribe_get_bytes(const u8 **src, void *dst, size_t len) {
  memcpy(dst, *src, len);
  *src += len;
}

void scribe_put_lstring(u8 **dst, const LString *s) {
  scribe_put_u16(dst, s->len);
  scribe_put_bytes(dst, s->data, s->len);
}

LString *scribe_get_lstring(const u8 **src) {
  u16 len = scribe_get_u16(src);
  LString *s = lstring_create(len);
  scribe_get_bytes(src, s->data, len);
  return s;
}
