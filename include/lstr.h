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
typedef struct LString LString;
extern const int kLStringLenSize;
struct LString {
  u16 len;
  u8 *data;
} ;

void lstring_free(void *data);
LString *lstring_create(u16 size);
LString *lstring_create_from_buf(int size, const char *buf);
char *lstring_to_cstr(LString *s);
int lstring_compare(const LString *a, const LString *b);

#endif // LSTR_H
