#include "hmap_si.h"
#include "log_utils.h"
#include "lstr.h"
#include <stdio.h>
#include <stdlib.h>

int hash(LString *key, int kssize) {
  unsigned int h = 0;
  for (uint i = 0; i < key->len; i++) {
    h += (unsigned char)(key->data[i]);
  }
  return h % kssize;
}

int hash_fnv1(LString *key, int kssize) {
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
  return (int)(hash % kssize);
}

bucket_item *bucket_append_entry(bucket_item *b, LString *key, int value) {
  bucket_item *new_item = malloc(sizeof(bucket_item));
  new_item->key = key;
  new_item->value = value;
  new_item->next = NULL;

  bucket_item *curr = b;
  if (curr == NULL) {
    return new_item; // return new root
  }
  // go to end and adjust (old_last)->next
  for (; curr->next != NULL; curr = curr->next) {
  }
  curr->next = new_item;
  return b; // return root
}

bucket_item *bucket_search_key(bucket_item *b, LString *key) {
  for (bucket_item *curr = b; curr != NULL; curr = curr->next) {
    if (lstring_compare(curr->key, key) == 0) {
      return curr;
    }
  }
  return NULL;
}

int bucket_delete_key(bucket_item **b, LString *key) {
  if (b == NULL || *b == NULL) {
    return 0;
  }
  bucket_item *prev = NULL;
  bucket_item *curr = *b;
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
  bucket_item **new_data =
      calloc(map->cap, sizeof(bucket_item *)); // sizeof correct here?
  // when rehashing, can we break collisions? probably should try
  bucket_item *curr;
  for (int i = 0; i < old_cap; i++) {
    curr = (map->data)[i];
    // for each entry in old bucket, PREPEND this entry to the bucket
    // in the new_data, but do not reallocate anything
    bucket_item *prev;
    while (curr != NULL) {
      prev = curr;
      curr = curr->next;
      LString *key = prev->key;
      int index = (map->hashfn)(key, map->cap);
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
  bucket_item *curr;
  for (int i = 0; i < map->cap; i++) {
    curr = map->data[i];
    if (curr == NULL) {
      continue;
    }
    printf("%3d  ", i);
    for (; curr != NULL; curr = curr->next) {
      printf("%.*s -> ", (int)curr->key->len, curr->key->data);
    }
    printf("/\n");
  }
}

void hashmap_si_print_entries_compact(hashmap_si *map) {
  bucket_item *curr;
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
  bucket_item *curr;
  for (int i = 0; i < map->cap; i++) {
    curr = map->data[i];
    PRINTLN("bucket %d", i);
    for (; curr != NULL; curr = curr->next) {
      PRINTLN("  key=%s", curr->key);
    }
  }
}

int hashmap_si_upsert(hashmap_si *map, LString *key, int value) {
  if (map->size >= map->cap) {
    hashmap_si_rehash(map);
  }
  bucket_item **data = map->data;

  int index = (map->hashfn)(key, map->cap);
  bucket_item *search_item = bucket_search_key(data[index], key);
  if (search_item == NULL) { // no exist, insert
    data[index] = bucket_append_entry(data[index], key, value);
    map->size++;
  } else { // exist
    lstring_free(search_item->key);
    search_item->key = key;
    search_item->value = value;
  }
  /*hashmap_si_print_keys_compact(map);*/
  return index;
}

int hashmap_si_get(hashmap_si *map, LString *key) {
  int index = (map->hashfn)(key, map->cap);
  bucket_item *bkt = map->data[index];
  bucket_item *search_item = bucket_search_key(bkt, key);
  if (search_item == NULL) {
    printf(" -------- X XXXXXXXX                X - NOT FOUND!");
    return NULL;
  } else {
    printf("FOUND!");
    return search_item->value;
  }
}

int hashmap_si_delete(hashmap_si *map, LString *key) {
  int index = (map->hashfn)(key, map->cap);
  int ret = bucket_delete_key(&map->data[index], key);
  return ret;
}

void hashmap_si_init(hashmap_si *map, int init_cap) {
  map->cap = init_cap;
  map->size = 0;
  map->data = calloc((map->cap), sizeof(map->data));
  map->hashfn = hash_fnv1;
}

void hashmap_si_destroy(hashmap_si *map) {
  for (int i = 0; i < map->cap; i++) {
    bucket_item *curr = map->data[i];
    while (curr != NULL) {
      bucket_item *tmp = curr;
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
//   printf("%d\n", hash("1", 128));
//   printf("%d\n", hash("2", 128));
//   printf("%d\n", hash("3", 128));
//   hashmap_si_upsert(&map, "1", "abc");
//   hashmap_si_upsert(&map, "2", "xyz");
//   hashmap_si_upsert(&map, "3", "abc");
//   printf("%s\n", map.data[0]->value);
// }
