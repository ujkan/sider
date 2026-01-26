#include "skiplist_str.h"
#include "lstr.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

const int kLStringLenSize = 2;

void pretty_print_skiplist(struct SkipList *list) {
  if (!list || !list->head) {
    printf("SkipList is empty.\n");
    return;
  }

  // The print format for a node is " -> [%3d]". This is 8 characters wide.
  const char *NODE_PRESENT_FORMAT = " -> [%.*s]";
  // The spacer must match the width: 8 spaces/dashes.
  const char *NODE_SPACER_FORMAT =
      " --------"; // Use dashes for easy visualization of the gap

  printf("\n--- SkipList Structure (Max Level: %d) ---\n",
         list->num_levels - 1);

  // Iterate from the highest active level down to level 0 (base list)
  for (int i = list->num_levels - 1; i >= 0; i--) {
    printf("Level %2d: HEAD", i);

    // Pointer for the current level (i) traversal
    struct Node *current_level_ptr = list->head->next[i];

    // Pointer for the base level (0) traversal (used for column alignment)
    struct Node *level_0_ptr = list->head->next[0];

    // Traverse the entire list using the level 0 path for alignment
    while (level_0_ptr != NULL) {

      // Check if the current column (defined by level_0_ptr->data) is present
      // at level i
      if (current_level_ptr != NULL &&
          lstring_compare(current_level_ptr->key, level_0_ptr->key) == 0) {
        // Node is present at this level: print the key
        printf(NODE_PRESENT_FORMAT, current_level_ptr->key->len,
               current_level_ptr->key->data);

        // Advance the current level pointer to the next connected node at level
        // i
        current_level_ptr = current_level_ptr->next[i];
      } else {
        // Node is NOT present at this level (it was skipped): print a
        // gap/spacer
        printf("%s", NODE_SPACER_FORMAT);
      }

      // Always advance the base level pointer to move to the next column
      // position
      level_0_ptr = level_0_ptr->next[0];
    }

    printf(" -> NULL\n");
  }

  printf("-----------------------------------------\n");
}

SkipList *sl_s_init() {

  SkipList *sl = malloc(sizeof(SkipList));
  Node *nmax = NULL;
  Node *nmin = malloc(sizeof(Node) + 3 * sizeof(Node *));
  nmin->key = lstring_create(0);
  nmin->level = 2;
  for (int i = 0; i < 3; i++)
    nmin->next[i] = NULL;
  sl->head = nmin;
  sl->num_levels = 3;
  sl->size_in_bytes = 0;
  sl->num_elements = 0;
  return sl;
}

LString *sl_s_find(SkipList *sl, LString *key) {
  Node *prev = NULL;
  Node *curr = sl->head;
  int level = curr->level;

  while (level >= 0) {

    if (curr == NULL) {
      curr = prev;
      level--;
      continue;
    }
    if (lstring_compare(key, curr->key) > 0) {
      printf("L%d key(%.*s) > curr->key(%.*s)\n", level, key->len, key->data,
             curr->key->len, curr->key->data);
      prev = curr;
      curr = curr->next[level];
    } else if (lstring_compare(key, curr->key) < 0) {
      printf("L%d key(%.*s) < curr->key(%.*s)\n", level, key->len, key->data,
             curr->key->len, curr->key->data);
      level--;
      curr = prev;
      prev = curr;
    } else {
      return curr->value;
    }
  }
  return NULL;
}

i32 sl_s_insert(SkipList *sl, LString *key, LString *value) {
  Node *insertion_points[sl->num_levels];
  Node *prev = NULL;
  Node *curr = sl->head;
  int level = curr->level;

  while (level >= 0) {
    if (curr == NULL) {
      curr = prev;
      level--;
      continue;
    }
    if (lstring_compare(key, curr->key) < 0) {
      printf("L%d key(%.*s) < curr->key(%.*s)\n", level, key->len, key->data,
             curr->key->len, curr->key->data);
      insertion_points[level] = prev;
      curr = prev;
      level--;
    } else if (lstring_compare(key, curr->key) > 0) {
      printf("L%d key(%.*s) > curr->key(%.*s)\n", level, key->len, key->data,
             curr->key->len, curr->key->data);
      insertion_points[level] = curr;
      prev = curr;
      curr = curr->next[level];
    } else {
      printf("EARLY RET!!\n");
      printf("=== L%d key(%.*s) > curr->key(%.*s)\n", level, key->len,
             key->data, curr->key->len, curr->key->data);
      return 1;
    }
  }
  int sum = 1;
  for (int i = 0; i < sl->num_levels - 1; i++) {
    if (rand() % 4)
      break;
    sum += 1;
  }
  printf("SUM=%d\n", sum);
  Node *new_node = malloc(sizeof(Node) + sum * sizeof(Node *));
  new_node->key = key;
  new_node->value = value;
  printf("key->len = %d ;; value->len = %d\n", key->len, value->len);
  sl->size_in_bytes += (key->len + value->len);
  for (int i = 0; i < sum; i++) {
    new_node->next[i] = insertion_points[i]->next[i];
    insertion_points[i]->next[i] = new_node;
  }
  pretty_print_skiplist(sl);
  return 0;
}

i32 sl_s_remove(SkipList *sl, LString *key) {
  Node *prev = NULL;
  Node *curr = sl->head;
  int level = curr->level;

  while (level >= 0) {

    if (curr == NULL) {
      curr = prev;
      level--;
      continue;
    }
    if (lstring_compare(key, curr->key) > 0) {
      printf("L%d key(%.*s) > curr->key(%.*s)\n", level, key->len, key->data,
             curr->key->len, curr->key->data);
      prev = curr;
      curr = curr->next[level];
    } else if (lstring_compare(key, curr->key) < 0) {
      printf("L%d key(%.*s) < curr->key(%.*s)\n", level, key->len, key->data,
             curr->key->len, curr->key->data);
      level--;
      curr = prev;
      prev = curr;
    } else {
      Node *level_prev = prev;
      for (int i = level; i >= 0; i--) {
        while (level_prev->next[i] != curr) {
          level_prev = level_prev->next[i];
        }
        level_prev->next[i] = curr->next[i];
      }
      sl->size_in_bytes += curr->key->len + curr->value->len;
      free(curr);
      return 0;
    }
  }
  return 1;
}

i32 sl_s_get_data(SkipList *sl, LString ***keys_out, LString ***values_out,
                  u32 *out_count) {
  Node *curr = sl->head->next[0];

  *out_count = 0;
  while (curr) {
    curr = curr->next[0];
    printf("-------> curr: %p\n", curr);
    (*out_count)++;
  }
  printf("OUT_)COUNT=%d\n", *out_count);

  *keys_out = calloc(*out_count, sizeof(LString *));
  *values_out = calloc(*out_count, sizeof(u32 *));

  curr = sl->head->next[0];
  int i = 0;
  while (curr) {
    (*keys_out)[i] = curr->key;
    printf("curr->data->len %d\n", curr->key->len);
    (*values_out)[i] = curr->value;
    curr = curr->next[0];
    i++;
  }

  return 0;
}

// int main() {
//   srand(time(0));
//   SkipList *sl = malloc(sizeof(SkipList));
//   // Node *nmax = malloc(sizeof(Node) + 3 * sizeof(Node *));
//   // nmax->data = 999;
//   // nmax->level = 2;
//   // nmax->next[0] = NULL;
//   // nmax->next[1] = NULL;
//   // nmax->next[2] = NULL;
//   Node *nmax = NULL;
//   Node *nmin = malloc(sizeof(Node) + 3 * sizeof(Node *));
//   nmin->data = lstring_create(0);
//   nmin->level = 2;
//   nmin->next[0] = nmax;
//   nmin->next[1] = nmax;
//   nmin->next[2] = nmax;
//
//   sl->head = nmin;
//   sl->num_levels = 3;
//
//   LString *s1 = lstring_create_from_buf(4, "AB00");
//   LString *s2 = lstring_create_from_buf(4, "AC00");
//   LString *s3 = lstring_create_from_buf(4, "AB01");
//   LString *s4 = lstring_create_from_buf(4, "XB00");
//   LString *s5 = lstring_create(4);
//   sl_s_insert(sl, s1);
//   sl_s_insert(sl, s3);
//   sl_s_insert(sl, s2);
//   sl_s_insert(sl, s4);
//   sl_s_remove(sl, s4);
//   pretty_print_skiplist(sl);
// }
