#ifndef LSTR_H
#define LSTR_H
#include "types.h"
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
i32 lstring_compare(const LString *a, const LString *b);

#endif // LSTR_H
