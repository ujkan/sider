#include "vector.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Test counter
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name)                                                             \
  do {                                                                         \
    printf("Testing %s... ", #name);                                           \
    tests_run++;                                                               \
    if (test_##name()) {                                                       \
      printf("PASSED\n");                                                      \
      tests_passed++;                                                          \
    } else {                                                                   \
      printf("FAILED\n");                                                      \
    }                                                                          \
  } while (0)

// Helper function to create and initialize a vector for testing
vector *create_test_vector(int cap) {
  vector *v = malloc(sizeof(vector));
  vector_init(v, cap);
  return v;
}

// Helper function to free test vector
void free_test_vector(vector *v) {
  if (v && v->data) {
    free(v->data);
  }
  if (v) {
    free(v);
  }
}

// Test vector_init
int test_vector_init() {
  vector v;

  // Test normal initialization
  vector_init(&v, 10);
  if (v.cap != 10 || v.size != 0 || v.data == NULL) {
    free(v.data);
    return 0;
  }
  free(v.data);

  // Test initialization with capacity 1
  vector_init(&v, 1);
  if (v.cap != 1 || v.size != 0 || v.data == NULL) {
    free(v.data);
    return 0;
  }
  free(v.data);

  // Test initialization with capacity 0 (edge case)
  vector_init(&v, 0);
  if (v.cap != 0 || v.size != 0) {
    if (v.data)
      free(v.data);
    return 0;
  }
  if (v.data)
    free(v.data);

  return 1;
}

// Test vector_get
int test_vector_get() {
  vector *v = create_test_vector(5);

  // Add some test data
  v->data[0] = 10;
  v->data[1] = 20;
  v->data[2] = 30;
  v->size = 3;

  // Test getting valid indices
  if (vector_get(v, 0) != 10 || vector_get(v, 1) != 20 ||
      vector_get(v, 2) != 30) {
    free_test_vector(v);
    return 0;
  }

  free_test_vector(v);
  return 1;
}

// Test vector_get_safe
int test_vector_get_safe() {
  vector *v = create_test_vector(5);
  int value;

  // Add some test data
  v->data[0] = 100;
  v->data[1] = 200;
  v->size = 2;

  // Test valid indices
  if (vector_get_safe(v, 0, &value) != 0 || value != 100) {
    free_test_vector(v);
    return 0;
  }

  if (vector_get_safe(v, 1, &value) != 0 || value != 200) {
    free_test_vector(v);
    return 0;
  }

  // Test invalid indices (should return -1)
  if (vector_get_safe(v, -1, &value) != -1) {
    free_test_vector(v);
    return 0;
  }

  if (vector_get_safe(v, 2, &value) != -1) {
    free_test_vector(v);
    return 0;
  }

  if (vector_get_safe(v, 100, &value) != -1) {
    free_test_vector(v);
    return 0;
  }

  // Test empty vector
  v->size = 0;
  if (vector_get_safe(v, 0, &value) != -1) {
    free_test_vector(v);
    return 0;
  }

  free_test_vector(v);
  return 1;
}

// Test vector_push
int test_vector_push() {
  vector *v = create_test_vector(2);

  // Test pushing within capacity
  vector_push(v, 10);
  if (v->size != 1 || v->data[0] != 10) {
    free_test_vector(v);
    return 0;
  }

  vector_push(v, 20);
  if (v->size != 2 || v->data[1] != 20) {
    free_test_vector(v);
    return 0;
  }

  // Test pushing beyond capacity (should trigger realloc)
  // Note: The original code has a bug here - realloc(v, v->cap) should be
  // realloc(v->data, sizeof(int) * v->cap)
  int original_cap = v->cap;
  vector_push(v, 30);

  // These checks might fail due to the bug in the original code
  if (v->size != 3 || v->cap != original_cap * 2) {
    free_test_vector(v);
    return 0;
  }

  free_test_vector(v);
  return 1;
}

// Test vector_set
int test_vector_set() {
  vector *v = create_test_vector(5);

  // Initialize some data
  v->data[0] = 1;
  v->data[1] = 2;
  v->data[2] = 3;
  v->size = 3;

  // Test setting values
  vector_set(v, 0, 100);
  vector_set(v, 1, 200);
  vector_set(v, 2, 300);

  if (v->data[0] != 100 || v->data[1] != 200 || v->data[2] != 300) {
    free_test_vector(v);
    return 0;
  }

  // Size should remain unchanged
  if (v->size != 3) {
    free_test_vector(v);
    return 0;
  }

  free_test_vector(v);
  return 1;
}

// Test vector_set_safe
int test_vector_set_safe() {
  vector *v = create_test_vector(5);

  // Initialize some data
  v->data[0] = 1;
  v->data[1] = 2;
  v->size = 2;

  // Test valid indices
  if (vector_set_safe(v, 0, 100) != 0) {
    free_test_vector(v);
    return 0;
  }

  if (vector_set_safe(v, 1, 200) != 0) {
    free_test_vector(v);
    return 0;
  }

  if (v->data[0] != 100 || v->data[1] != 200) {
    free_test_vector(v);
    return 0;
  }

  // Test invalid indices
  if (vector_set_safe(v, -1, 50) != -1) {
    free_test_vector(v);
    return 0;
  }

  if (vector_set_safe(v, 2, 50) != -1) {
    free_test_vector(v);
    return 0;
  }

  if (vector_set_safe(v, 100, 50) != -1) {
    free_test_vector(v);
    return 0;
  }

  // Test empty vector
  v->size = 0;
  if (vector_set_safe(v, 0, 50) != -1) {
    free_test_vector(v);
    return 0;
  }

  free_test_vector(v);
  return 1;
}

// Test vector_pop
int test_vector_pop() {
  vector *v = create_test_vector(5);

  // Add test data
  v->data[0] = 10;
  v->data[1] = 20;
  v->data[2] = 30;
  v->size = 3;

  // Test popping elements
  int popped = vector_pop(v);
  if (popped != 30 || v->size != 2) {
    free_test_vector(v);
    return 0;
  }

  popped = vector_pop(v);
  if (popped != 20 || v->size != 1) {
    free_test_vector(v);
    return 0;
  }

  popped = vector_pop(v);
  if (popped != 10 || v->size != 0) {
    free_test_vector(v);
    return 0;
  }

  free_test_vector(v);
  return 1;
}

// Test vector_pop_safe
int test_vector_pop_safe() {
  vector *v = create_test_vector(5);
  int value;

  // Add test data
  v->data[0] = 100;
  v->data[1] = 200;
  v->size = 2;

  // Test valid pops
  if (vector_pop_safe(v, &value) != 0 || value != 200 || v->size != 1) {
    free_test_vector(v);
    return 0;
  }

  if (vector_pop_safe(v, &value) != 0 || value != 100 || v->size != 0) {
    free_test_vector(v);
    return 0;
  }

  // Test popping from empty vector
  if (vector_pop_safe(v, &value) != -1) {
    free_test_vector(v);
    return 0;
  }

  // Size should still be 0
  if (v->size != 0) {
    free_test_vector(v);
    return 0;
  }

  free_test_vector(v);
  return 1;
}

// Test vector_remove
int test_vector_remove() {
  vector *v = create_test_vector(5);

  // Add test data
  v->data[0] = 10;
  v->data[1] = 20;
  v->data[2] = 30;
  v->data[3] = 40;
  v->size = 4;

  // Test removing middle element
  int removed = vector_remove(v, 1);
  if (removed != 20 || v->size != 3) {
    free_test_vector(v);
    return 0;
  }

  // Check that elements shifted correctly
  if (v->data[0] != 10 || v->data[1] != 30 || v->data[2] != 40) {
    free_test_vector(v);
    return 0;
  }

  // Test removing first element
  removed = vector_remove(v, 0);
  if (removed != 10 || v->size != 2) {
    free_test_vector(v);
    return 0;
  }

  if (v->data[0] != 30 || v->data[1] != 40) {
    free_test_vector(v);
    return 0;
  }

  // Test removing last element
  removed = vector_remove(v, 1);
  if (removed != 40 || v->size != 1) {
    free_test_vector(v);
    return 0;
  }

  if (v->data[0] != 30) {
    free_test_vector(v);
    return 0;
  }

  free_test_vector(v);
  return 1;
}

// Test vector_remove_safe
int test_vector_remove_safe() {
  vector *v = create_test_vector(5);
  int value;

  // Add test data
  v->data[0] = 100;
  v->data[1] = 200;
  v->data[2] = 300;
  v->size = 3;

  // Test valid removal
  if (vector_remove_safe(v, 1, &value) != 0 || value != 200 || v->size != 2) {
    free_test_vector(v);
    return 0;
  }

  if (v->data[0] != 100 || v->data[1] != 300) {
    free_test_vector(v);
    return 0;
  }

  // Test invalid indices
  if (vector_remove_safe(v, -1, &value) != -1) {
    free_test_vector(v);
    return 0;
  }

  if (vector_remove_safe(v, 2, &value) != -1) {
    free_test_vector(v);
    return 0;
  }

  if (vector_remove_safe(v, 100, &value) != -1) {
    free_test_vector(v);
    return 0;
  }

  // Test empty vector
  v->size = 0;
  if (vector_remove_safe(v, 0, &value) != -1) {
    free_test_vector(v);
    return 0;
  }

  free_test_vector(v);
  return 1;
}

// Test vector_find
int test_vector_find() {
  vector *v = create_test_vector(5);

  // Add test data
  v->data[0] = 10;
  v->data[1] = 20;
  v->data[2] = 30;
  v->data[3] = 20; // duplicate
  v->size = 4;

  // Test finding existing elements
  if (vector_find(v, 10) != 0) {
    free_test_vector(v);
    return 0;
  }

  if (vector_find(v, 20) != 1) { // Should return first occurrence
    free_test_vector(v);
    return 0;
  }

  if (vector_find(v, 30) != 2) {
    free_test_vector(v);
    return 0;
  }

  // Test finding non-existing element
  if (vector_find(v, 100) != -1) {
    free_test_vector(v);
    return 0;
  }

  // Test empty vector
  v->size = 0;
  if (vector_find(v, 10) != -1) {
    free_test_vector(v);
    return 0;
  }

  // Test single element vector
  v->data[0] = 42;
  v->size = 1;
  if (vector_find(v, 42) != 0) {
    free_test_vector(v);
    return 0;
  }

  if (vector_find(v, 43) != -1) {
    free_test_vector(v);
    return 0;
  }

  free_test_vector(v);
  return 1;
}

// Test edge cases with null pointers and extreme values
int test_edge_cases() {
  vector *v = create_test_vector(1);
  int value;

  // Test with maximum and minimum integer values
  vector_push(v, INT_MAX);
  if (v->data[0] != INT_MAX) {
    free_test_vector(v);
    return 0;
  }

  vector_set(v, 0, INT_MIN);
  if (v->data[0] != INT_MIN) {
    free_test_vector(v);
    return 0;
  }

  // Test finding extreme values
  if (vector_find(v, INT_MIN) != 0) {
    free_test_vector(v);
    return 0;
  }

  // Test with zero values
  vector_set(v, 0, 0);
  if (vector_find(v, 0) != 0) {
    free_test_vector(v);
    return 0;
  }

  free_test_vector(v);
  return 1;
}

int main() {
  printf("Running Vector Test Suite\n");
  printf("========================\n\n");

  TEST(vector_init);
  TEST(vector_get);
  TEST(vector_get_safe);
  TEST(vector_push);
  TEST(vector_set);
  TEST(vector_set_safe);
  TEST(vector_pop);
  TEST(vector_pop_safe);
  TEST(vector_remove);
  TEST(vector_remove_safe);
  TEST(vector_find);
  TEST(edge_cases);

  printf("\nTest Results: %d/%d tests passed\n", tests_passed, tests_run);

  if (tests_passed == tests_run) {
    printf("All tests passed!\n");
    return 0;
  } else {
    printf("Some tests failed.\n");
    return 1;
  }
}
