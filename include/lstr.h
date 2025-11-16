#ifndef LSTR_H
#define LSTR_H
#include <stdint.h>

#ifndef u32
typedef uint32_t u32;
#endif
#ifndef u16
typedef uint16_t u16;
#endif
#ifndef u8
typedef uint8_t u8;
#endif
typedef struct {
  u16 len;
  u8 *data;
} LString;

void lstring_free(void *data);
LString *lstring_create(int size);
LString *lstring_create_from_buf(int size, const char *buf);
int lstring_compare(const LString *a, const LString *b);

#endif // LSTR_H
