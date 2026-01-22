#include "lstr.h"
#include <stdlib.h>
#include <string.h>

void lstring_free(void *data) {
  free(((LString *)data)->data);
  free((void *)data);
}

char *lstring_to_cstr(LString *s) {
  char *cstr = malloc(s->len + 1);
  memcpy(cstr, s->data, s->len);
  cstr[s->len] = 0;
  return cstr;
}

LString *lstring_create(u16 size) {
  LString *lstr = malloc(sizeof(LString));
  lstr->len = size;
  lstr->data = malloc(size);
  return lstr;
}
LString *lstring_create_from_buf(int size, const char *buf) {
  LString *lstr = lstring_create(size);
  memcpy(lstr->data, buf, size);
  return lstr;
}

int lstring_compare(const LString *a, const LString *b) {
  u32 min_len = (a->len < b->len) ? a->len : b->len;

  // compare "common" part
  if (min_len > 0) {
    int cmp = memcmp(a->data, b->data, min_len);
    if (cmp != 0) { // differs in "common"
      return cmp;
    }
  }

  // "common" is identical, then length-based compare
  if (a->len < b->len) {
    return -1;
  }
  if (a->len > b->len) {
    return 1;
  }

  return 0;
}
