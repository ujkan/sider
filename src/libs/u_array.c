#include "u_array.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))

typedef struct {
  void *data;
  u64 len;
  u64 cap;
  u64 element_size;
  void (*element_free_func)(void *);
} FullArray;

Array *array_sized_new(u64 reserved_size, u64 element_size) {
  FullArray *farray = malloc(sizeof(FullArray));
  farray->data = malloc(reserved_size * element_size);
  farray->len = 0;
  farray->cap = reserved_size;
  farray->element_free_func = NULL;
  farray->element_size = element_size;
  return (Array *)farray;
}

Array *array_new_full(u64 reserved_size, u64 element_size,
                      void (*element_free_func)(void *)) {
  FullArray *farray = malloc(sizeof(FullArray));
  farray->data = malloc(reserved_size * element_size);
  farray->len = 0;
  farray->cap = reserved_size;
  farray->element_free_func = element_free_func;
  farray->element_size = element_size;
  return (Array *)farray;
}

void array_set(Array *array, u64 index, void *item) {
  FullArray *farray = (FullArray *)array;
  // unlikely
  if (index < 0 || index >= farray->len) {
    return;
  }
  memcpy(&farray->data[index * farray->element_size], item,
         farray->element_size);
}

static void array_maybe_expand(FullArray *farray, u64 length) {
  if (length > farray->cap) {
    size_t new_cap = MAX(length, farray->cap * 2);

    // Check for multiplication overflow
    if (new_cap > SIZE_MAX / farray->element_size) {
      fprintf(stderr, "ERROR: Allocation size overflow\n");
      exit(1);
    }

    void *new_data = realloc(farray->data, new_cap * farray->element_size);
    if (!new_data) {
      fprintf(stderr, "ERROR: Could not reallocate array to length %llu\n",
              length);
      exit(1);
    }
    farray->data = new_data;
    farray->cap = new_cap; // <- Fixed: was 'length', should be 'new_cap'
  }
}

void array_push_many(Array *array, const void *start, u64 n) {
  FullArray *farray = (FullArray *)array;
  array_maybe_expand(farray, farray->len + n);
  memcpy(&farray->data[(farray->len) * farray->element_size], start,
         n * farray->element_size);
  farray->len += n;
}

void array_push(Array *array, void *const element) {
  array_push_many(array, element, 1);
}

void array_make_empty(Array *array) { array->len = 0; }

void array_set_length(Array *array, u64 length) {
  FullArray *farray = (FullArray *)array;
  if (length == farray->len)
    return;
  if (length < farray->len) {
    if (farray->element_free_func) {
      for (u64 i = length; i < farray->len; i++) {
        farray->element_free_func(&farray->data[i * farray->element_size]);
        memset(&farray->data[i * farray->element_size], 0,
               farray->element_size);
      }
    }
  } else {
    array_maybe_expand(farray, length);
    for (u64 i = farray->len; i < length; i++) {
      memset(&farray->data[i * farray->element_size], 0, farray->element_size);
    }
  }
  farray->len = length;
}

void array_free(Array *array, bool free_seg) {
  FullArray *farray = (FullArray *)array;
  if (farray->element_free_func) {
    for (u64 i = 0; i < farray->len; i++) {
      farray->element_free_func(&farray->data[i * farray->element_size]);
    }
  }
  if (free_seg) {
    free(farray->data);
  }
}
