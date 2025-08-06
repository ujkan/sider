#ifndef VECTOR_H
#define VECTOR_H

#include <stdlib.h>

#define CHECK_BOUNDS(index, size)                                              \
  do {                                                                         \
    if (index < 0 || index >= size) {                                          \
      return -1;                                                               \
    }                                                                          \
  } while (0)

#define DEFINE_VECTOR(TYPE, COMPARE_FN)                                        \
  typedef struct vector_##TYPE {                                               \
    TYPE *data;                                                                \
    int32_t size;                                                              \
    int32_t cap;                                                               \
  } vector_##TYPE;                                                             \
                                                                               \
  void vector_##TYPE##_init(vector_##TYPE *v, int32_t cap) {                   \
    v->data = malloc(sizeof(TYPE) * cap);                                      \
    v->size = 0;                                                               \
    v->cap = cap;                                                              \
  }                                                                            \
  TYPE *vector_##TYPE##_get(vector_##TYPE *v, int index) {                     \
    return &v->data[index];                                                    \
  }                                                                            \
                                                                               \
  int vector_##TYPE##_get_safe(vector_##TYPE *v, int index, TYPE **value) {    \
    CHECK_BOUNDS(index, v->size);                                              \
    *value = vector_##TYPE##_get(v, index);                                    \
    return 0;                                                                  \
  }                                                                            \
                                                                               \
  void vector_##TYPE##_push(vector_##TYPE *v, TYPE value) {                    \
    if (v->size == v->cap) {                                                   \
      v->cap *= 2;                                                             \
      v->data = realloc(v->data, sizeof(TYPE) * v->cap);                       \
    }                                                                          \
    v->data[v->size] = value;                                                  \
    v->size++;                                                                 \
  }                                                                            \
                                                                               \
  void vector_##TYPE##_set(vector_##TYPE *v, int index, TYPE value) {          \
    v->data[index] = value;                                                    \
  }                                                                            \
  int vector_##TYPE##_set_safe(vector_##TYPE *v, int index, TYPE value) {      \
    CHECK_BOUNDS(index, v->size);                                              \
    vector_##TYPE##_set(v, index, value);                                      \
    return 0;                                                                  \
  }                                                                            \
                                                                               \
  TYPE *vector_##TYPE##_pop(vector_##TYPE *v) {                                \
    TYPE *el = &v->data[v->size - 1];                                          \
    v->size--;                                                                 \
    return el;                                                                 \
  }                                                                            \
                                                                               \
  int vector_##TYPE##_pop_safe(vector_##TYPE *v, TYPE *value) {                \
    if (v->size <= 0) {                                                        \
      return -1;                                                               \
    }                                                                          \
    value = vector_##TYPE##_pop(v);                                            \
    return 0;                                                                  \
  }                                                                            \
                                                                               \
  TYPE *vector_##TYPE##_remove(vector_##TYPE *v, int index) {                  \
    TYPE *el = &v->data[index];                                                \
    for (int i = index + 1; i < v->size; i++) {                                \
      v->data[i - 1] = v->data[i];                                             \
    }                                                                          \
    v->size--;                                                                 \
    return el;                                                                 \
  }                                                                            \
                                                                               \
  int vector_##TYPE##_remove_safe(vector_##TYPE *v, int index, TYPE *value) {  \
    CHECK_BOUNDS(index, v->size);                                              \
    value = vector_##TYPE##_remove(v, index);                                  \
    return 0;                                                                  \
  }                                                                            \
                                                                               \
  int vector_##TYPE##_find(vector_##TYPE *v, TYPE value) {                     \
    for (int i = 0; i < v->size; i++) {                                        \
      if (COMPARE_FN(v->data[i], value) == 0) {                                \
        return i;                                                              \
      }                                                                        \
    }                                                                          \
    return -1;                                                                 \
  }                                                                            \
  void vector_##TYPE##_insert(vector_##TYPE *v, TYPE *value, uint32_t len) {   \
    while (v->size + len >= v->cap) {                                          \
      v->cap *= 2;                                                             \
    }                                                                          \
    v->data = realloc(v->data, sizeof(TYPE) * v->cap);                         \
    for (uint32_t i = 0; i < len; i++) {                                       \
      v->data[v->size++] = value[i];                                           \
    }                                                                          \
  }                                                                            \
  void vector_##TYPE##_remove_first_n(vector_##TYPE *v, uint32_t n) {          \
    if (n <= v->size) {                                                        \
      for (int i = n; i < v->size; i++) {                                      \
        v->data[i - n] = v->data[i];                                           \
      }                                                                        \
      v->size -= n;                                                            \
    }                                                                          \
  }

#endif // VECTOR_H