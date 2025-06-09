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
  int vector_##TYPE##_get_safe(vector_##TYPE *v, int index, TYPE *value) {     \
    CHECK_BOUNDS(index, v->size);                                              \
    value = vector_##TYPE##_get(v, index);                                     \
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
  }

// typedef struct vector {
//   int *data;
//   int size;
//   int cap;
// } vector;
// void vector_init(vector *v, int cap);
// void vector_destroy(vector *v);
// void vector_push(vector *v, int value);
// int vector_get(vector *v, int index);
// int vector_get_safe(vector *v, int index, int *value);
// void vector_set(vector *v, int index, int value);
// int vector_set_safe(vector *v, int index, int value);
// int vector_pop(vector *v);
// int vector_pop_safe(vector *v, int *value);
// int vector_remove(vector *v, int index);
// int vector_remove_safe(vector *v, int index, int *value);
// int vector_find(vector *v, int value);
//
