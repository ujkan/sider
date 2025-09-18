#include "u_ptr_array.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  void **data;
  uint64_t len;
  uint64_t cap;
  void (*element_free_func)(void *);
} FullPtrArray;

#define MAX(a, b) ((a) > (b) ? (a) : (b))

PtrArray *ptr_array_new_full(uint64_t reserved_size,
                             void (*element_free_func)(void *)) {

  FullPtrArray *farray = malloc(sizeof(FullPtrArray));
  farray->cap = reserved_size;
  farray->data =
      reserved_size == 0 ? NULL : malloc((farray->cap) * sizeof(void *));
  farray->len = 0;
  farray->element_free_func = element_free_func;

  return (PtrArray *)farray;
}

PtrArray *ptr_array_sized_new(uint64_t reserved_size) {
  return ptr_array_new_full(reserved_size, NULL);
}

static void ptr_array_maybe_expand(FullPtrArray *farray, uint64_t length) {
  if (length > farray->cap) {
    void *new_data = reallocarray(farray->data, MAX(length, farray->cap * 2),
                                  sizeof(void *));
    if (!new_data) {
      printf("ERROR: Could not reallocate ptr_array to length %ld\n", length);
      exit(1);
    }
    farray->data = new_data;
    farray->cap = length;
  }
}

PtrArray *ptr_array_new() { return ptr_array_new_full(0, NULL); }

void ptr_array_add(PtrArray *array, void *item) {
  FullPtrArray *farray = (FullPtrArray *)array;
  ptr_array_maybe_expand(farray, farray->len + 1);
  ptr_array_index(array, farray->len) = item;
  (farray->len)++;
  return;
}

void ptr_array_set(PtrArray *array, uint64_t index, void *item) {
  FullPtrArray *farray = (FullPtrArray *)array;
  if (index >= farray->len) {
    return;
  }
  void *old_item = ptr_array_index(farray, index);
  if (farray->element_free_func && old_item) {
    farray->element_free_func(old_item);
  }
  free((void *)old_item);
  ptr_array_index(farray, index) = item;
}

void ptr_array_free(PtrArray *array, bool free_seg) {
  FullPtrArray *farray = (FullPtrArray *)array;
  if (free_seg && farray->element_free_func) {
    for (uint64_t i = 0; i < farray->len; i++) {
      farray->element_free_func(ptr_array_index(array, i));
    }
  }
  free(array);
}

//
// 0           len         cap
//[__,__,__,__,__,__,__,__,__]
//
// Case 1: NEW < len < cap
// 0     NEW   len         cap
//[__,__,__,__,__,__,__,__,__]
// post:
//  - len = NEW
//  - data[NEW] to data[len-1] is freed
//
// Case 2: len < NEW < cap
// 0           len   NEW   cap
//[__,__,__,__,__,__,__,__,__]
// post:
//  - len = NEW
//  - data[len] to data[NEW-1] is init with 0
//
// Case 2: len < cap < NEW
// 0           len         cap         NEW
//[__,__,__,__,__,__,__,__,__]
// post:
//  - len = NEW
//  - cap = NEW
//  - data[len] to data[NEW-1] is init with 0
//  - realloced

void ptr_array_set_length(PtrArray *array, uint64_t length) {
  FullPtrArray *farray = (FullPtrArray *)array;
  if (length == farray->len)
    return;
  if (length < farray->len) {
    if (farray->element_free_func) {
      for (uint64_t i = length; i < farray->len; i++) {
        farray->element_free_func(ptr_array_index(farray, i));
        ptr_array_index(farray, i) = 0;
      }
    }
  } else {
    ptr_array_maybe_expand(farray, length);
    for (uint64_t i = farray->len; i < length; i++) {
      ptr_array_index(farray, i) = 0;
    }
  }
  farray->len = length;
}
