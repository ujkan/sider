#include "hmap_si.h"
#include "log_utils.h"
#include "lstr.h"
#include <stdio.h>
#include <stdlib.h>

u32 hash_si(LString *key, int kssize) {
  unsigned int h = 0;
  for (u16 i = 0; i < key->len; i++) {
    h += (unsigned char)(key->data[i]);
  }
  return h % kssize;
}

u32 hash_fnv1_si(LString *key, int kssize) {
  // FNV-1a hash algorithm constants
  const unsigned int FNV_PRIME = 16777619;
  const unsigned int FNV_OFFSET_BASIS = 2166136261;

  // Initialize hash with the offset basis
  unsigned int hash = FNV_OFFSET_BASIS;

  // Process each byte in the key
  for (u32 i = 0; i < key->len; i++) {
    hash ^= key->data[i]; // XOR with the current byte
    hash *= FNV_PRIME;    // Multiply by the prime
  }

  // Return the hash value within the key space
  return hash % kssize;
}

bucket_item_si *bucket_si_append_entry(bucket_item_si *b, LString *key,
                                       int value) {
  bucket_item_si *new_item = malloc(sizeof(bucket_item_si));
  new_item->key = key;
  new_item->value = value;
  new_item->next = NULL;

  bucket_item_si *curr = b;
  if (curr == NULL) {
    return new_item; // return new root
  }
  // go to end and adjust (old_last)->next
  for (; curr->next != NULL; curr = curr->next) {
  }
  curr->next = new_item;
  return b; // return root
}

bucket_item_si *bucket_si_search_key(bucket_item_si *b, LString *key) {
  for (bucket_item_si *curr = b; curr != NULL; curr = curr->next) {
    if (lstring_compare(curr->key, key) == 0) {
      return curr;
    }
  }
  return NULL;
}

i32 bucket_si_delete_key(bucket_item_si **b, LString *key) {
  if (b == NULL || *b == NULL) {
    return 0;
  }
  bucket_item_si *prev = NULL;
  bucket_item_si *curr = *b;
  while (curr != NULL) {
    if (lstring_compare(curr->key, key) == 0) {
      if (prev) {
        prev->next = curr->next;
      } else {
        *b = curr->next;
      }
      lstring_free(curr->key);
      free(curr);
      return 1;
    }
    prev = curr;
    curr = curr->next;
  }
  return 0;
}

void hashmap_si_rehash(hashmap_si *map) {
  int old_cap = map->cap;
  map->cap *= 2;
  bucket_item_si **new_data =
      calloc(map->cap, sizeof(bucket_item_si *)); // sizeof correct here?
  // when rehashing, can we break collisions? probably should try
  bucket_item_si *curr;
  for (int i = 0; i < old_cap; i++) {
    curr = (map->data)[i];
    // for each entry in old bucket, PREPEND this entry to the bucket
    // in the new_data, but do not reallocate anything
    bucket_item_si *prev;
    while (curr != NULL) {
      prev = curr;
      curr = curr->next;
      LString *key = prev->key;
      u32 index = (map->hashfn)(key, map->cap);
      prev->next = new_data[index];
      new_data[index] = prev;
    }
  }

  // TODO: we need to free old buckets !!
  // maybe that's why we should use realloc instead of calloc
  free(map->data);

  map->data =
      new_data; // TODO: delete this // use after free? or assignment ok?
}

void hashmap_si_print_keys_compact(hashmap_si *map) {
  bucket_item_si *curr;
  for (int i = 0; i < map->cap; i++) {
    curr = map->data[i];
    if (curr == NULL) {
      continue;
    }
//     printf("%3d  ", i);
    for (; curr != NULL; curr = curr->next) {
//       printf("%.*s -> ", (int)curr->key->len, curr->key->data);
    }
//     printf("/\n");
  }
}

void hashmap_si_print_entries_compact(hashmap_si *map) {
  bucket_item_si *curr;
  for (int i = 0; i < map->cap; i++) {
    curr = map->data[i];
    if (curr == NULL) {
      continue;
    }
    printf("%3d  ", i);
    for (; curr != NULL; curr = curr->next) {
      printf("(%.*s :: %d) -> ", (int)curr->key->len, curr->key->data,
             (int)curr->value);
    }
    printf("/\n");
  }
}

void hashmap_si_print_keys(hashmap_si *map) {
  bucket_item_si *curr;
  for (int i = 0; i < map->cap; i++) {
    curr = map->data[i];
    PRINTLN("bucket %d", i);
    for (; curr != NULL; curr = curr->next) {
      PRINTLN("  key=%s", curr->key);
    }
  }
}

u32 hashmap_si_upsert(hashmap_si *map, LString *key, int value) {
  if (map->size >= map->cap) {
    hashmap_si_rehash(map);
  }
  bucket_item_si **data = map->data;

  u32 index = (map->hashfn)(key, map->cap);
  bucket_item_si *search_item = bucket_si_search_key(data[index], key);
  if (search_item == NULL) { // no exist, insert
    data[index] = bucket_si_append_entry(data[index], key, value);
    map->size++;
  } else { // exist
    lstring_free(search_item->key);
    search_item->key = key;
    search_item->value = value;
  }
  /*hashmap_si_print_keys_compact(map);*/
  return index;
}

i32 hashmap_si_insert(hashmap_si *map, LString *key, int value) {
  if (map->size >= map->cap) {
    hashmap_si_rehash(map);
  }
  bucket_item_si **data = map->data;

  u32 index = (map->hashfn)(key, map->cap);
  bucket_item_si *search_item = bucket_si_search_key(data[index], key);
  if (search_item == NULL) { // no exist, insert
    data[index] = bucket_si_append_entry(data[index], key, value);
    map->size++;
    return (i32)index;
  }
  lstring_free(key); // ownership transferred here, so i can free!
  return -1;         // key already exists
}

i32 hashmap_si_get(hashmap_si *map, LString *key, i32 *out_value) {
  if (out_value == NULL) {
    return -1; // Invalid parameter
  }

  u32 index = (map->hashfn)(key, map->cap);
  bucket_item_si *bkt = map->data[index];
  bucket_item_si *search_item = bucket_si_search_key(bkt, key);
  if (search_item == NULL) {
//     printf(" -------- X XXXXXXXX                X - NOT FOUND!");
    return -1; // Not found
  } else {
//     printf("FOUND!");
    *out_value = search_item->value;
    return 0; // Success
  }
}

i32 hashmap_si_delete(hashmap_si *map, LString *key) {
  u32 index = (map->hashfn)(key, map->cap);
  i32 ret = bucket_si_delete_key(&map->data[index], key);
  return ret;
}

void hashmap_si_init(hashmap_si *map, int init_cap) {
  map->cap = init_cap;
  map->size = 0;
  map->data = calloc((map->cap), sizeof(map->data));
  map->hashfn = hash_fnv1_si;
}

void hashmap_si_destroy(hashmap_si *map) {
  for (int i = 0; i < map->cap; i++) {
    bucket_item_si *curr = map->data[i];
    while (curr != NULL) {
      bucket_item_si *tmp = curr;
      curr = curr->next;
      lstring_free(tmp->key);
      free(tmp);
    }
  }
  free(map->data);
}

// int main(void) {
//   hashmap_si_si map;
//   hashmap_si_init(&map);
// printf("%d\n", hash("1", 128));
// printf("%d\n", hash("2", 128));
// printf("%d\n", hash("3", 128));
//   hashmap_si_upsert(&map, "1", "abc");
//   hashmap_si_upsert(&map, "2", "xyz");
//   hashmap_si_upsert(&map, "3", "abc");
// printf("%s\n", map.data[0]->value);
// }
