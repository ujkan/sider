

#define DEFINE_VECTOR(TYPE)                                                    \
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
  }

typedef struct vector {
  int *data;
  int size;
  int cap;
} vector;
void vector_init(vector *v, int cap);
void vector_destroy(vector *v);
void vector_push(vector *v, int value);
int vector_get(vector *v, int index);
int vector_get_safe(vector *v, int index, int *value);
void vector_set(vector *v, int index, int value);
int vector_set_safe(vector *v, int index, int value);
int vector_pop(vector *v);
int vector_pop_safe(vector *v, int *value);
int vector_remove(vector *v, int index);
int vector_remove_safe(vector *v, int index, int *value);
int vector_find(vector *v, int value);
