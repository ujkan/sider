#include "vector.h"
#include <stdlib.h>

#define CHECK_BOUNDS(index, size)                                              \
  do {                                                                         \
    if (index < 0 || index >= size) {                                          \
      return -1;                                                               \
    }                                                                          \
  } while (0)

void vector_init(vector *v, int cap) {
  v->data = malloc(sizeof(int) * cap);
  v->cap = cap;
  v->size = 0;
}

int vector_get_safe(vector *v, int index, int *value) {
  CHECK_BOUNDS(index, v->size);
  *value = vector_get(v, index);
  return 0;
}

int vector_get(vector *v, int index) { return v->data[index]; }

void vector_push(vector *v, int value) {
  if (v->size == v->cap) {
    v->cap *= 2;
    v->data = realloc(v->data, sizeof(int) * v->cap);
  }
  v->data[v->size] = value;
  v->size++;
}

void vector_set(vector *v, int index, int value) { v->data[index] = value; }
int vector_set_safe(vector *v, int index, int value) {
  CHECK_BOUNDS(index, v->size);
  vector_set(v, index, value);
  return 0;
}

int vector_pop(vector *v) {
  int el = v->data[v->size - 1];
  v->size--;
  return el;
}

int vector_pop_safe(vector *v, int *value) {
  if (v->size <= 0) {
    return -1;
  }
  *value = vector_pop(v);
  return 0;
}

int vector_remove(vector *v, int index) {
  int el = v->data[index];
  for (int i = index + 1; i < v->size; i++) {
    v->data[i - 1] = v->data[i];
  }
  v->size--;
  return el;
}

int vector_remove_safe(vector *v, int index, int *value) {
  CHECK_BOUNDS(index, v->size);
  *value = vector_remove(v, index);
  return 0;
}

int vector_find(vector *v, int value) {
  for (int i = 0; i < v->size; i++) {
    if (v->data[i] == value) {
      return i;
    }
  }
  return -1;
}
