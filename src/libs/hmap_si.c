#include "hmap_si.h"
#include "log_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int hash_si(char *key, int kssize) {
  unsigned int h = 0;
  while (*key) {
    h += (unsigned char)(*key++);
  }
  return h % kssize;
}

int hash_fnv1_si(char *key, int kssize) {
  // FNV-1a hash algorithm constants
  const unsigned int FNV_PRIME = 16777619;
  const unsigned int FNV_OFFSET_BASIS = 2166136261;

  // Initialize hash with the offset basis
  unsigned int hash = FNV_OFFSET_BASIS;

  // Process each byte in the key
  for (char *p = key; *p != '\0'; p++) {
    hash ^= (unsigned char)*p; // XOR with the current byte
    hash *= FNV_PRIME;         // Multiply by the prime
  }

  // Return the hash value within the key space
  return (int)(hash % kssize);
}

bucket_item_si *bk_si_append(bucket_si b, char *key, int value) {
  bucket_item_si *new_item = malloc(sizeof(bucket_item_si));
  new_item->p = (pair_si){.key = key, .value = value};

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

// maybe return ptr to bucket_item_si to avoid returning structs
bucket_item_si *bk_si_search(bucket_si b, char *key) {
  for (bucket_item_si *curr = b; curr != NULL; curr = curr->next) {
    if (strcmp((curr->p).key, key) == 0) {
      return curr;
    }
  }
  return NULL;
}

int bk_si_delete(bucket_si b, char *key) {
  if (b == NULL) {
    return 0;
  }
  bucket_item_si *prev = NULL;
  bucket_item_si *curr = b;
  while (curr != NULL) {
    if (strcmp(curr->p.key, key) == 0) {
      prev->next = curr->next;
      free(curr);
      return 1;
    }
    prev = curr;
    curr = curr->next;
  }
  return 0;
}

void hashmap_si_rehash(hashmap_si *map) {
  map->cap *= 2;
  bucket_si *new_data =
      malloc(map->cap * sizeof(bucket_si)); // sizeof correct here?
  // when rehashing, can we break collisions? probably should try
  bucket_si curr;
  for (int i = 0; i < map->size; i++) {
    curr = (map->data)[i];
    // for each entry in old bucket_si append to a bucket in the new data
    while (curr != NULL) {
      char *key = (curr->p).key;
      int index = (map->hashfn)(key, map->cap);
      new_data[index] = bkappend(new_data[index], key, curr->p.value);
      curr = curr->next;
    }
  }

  free(map->data);

  map->data = new_data; // use after free? or assignment ok?
}

void hashmap_si_print_keys_compact(hashmap_si *map) {
  bucket_si curr;
  for (int i = 0; i < map->cap; i++) {
    curr = map->data[i];
    if (curr == NULL) {
      continue;
    }
    printf("%3d  ", i);
    for (; curr != NULL; curr = curr->next) {
      printf("%s -> ", curr->p.key);
    }
    printf("/\n");
  }
}

void hashmap_si_print_keys(hashmap_si *map) {
  bucket_si curr;
  for (int i = 0; i < map->cap; i++) {
    curr = map->data[i];
    PRINT("bucket_si %d", i);
    for (; curr != NULL; curr = curr->next) {
      PRINT("  key=%s", curr->p.key);
    }
  }
}

int hashmap_si_upsert(hashmap_si *map, char *key, int value) {
  if (map->size < map->cap) {
    bucket_si *data = map->data;

    int index = (map->hashfn)(key, map->cap);
    bucket_item_si *search_item = bksearch(data[index], key);
    if (search_item == NULL) { // no exist, insert
      pair_si p = {.key = key, .value = value};
      data[index] = bkappend(data[index], key, value);
      map->size++;
    } else { // exist
      search_item->p.value = value;
    }
    return index;
  } else {
    hashmap_si_rehash(map);
    return hashmap_si_upsert(
        map, key,
        value); // can i reuse? or is recursion dangerous; should i
                // keep a reusable safe upsert to avoid recursion?
  }
  return -1;
}

int hashmap_si_get(hashmap_si *map, char *key) {
  int index = (map->hashfn)(key, map->cap);
  bucket_si bkt = map->data[index];
  bucket_item_si *search_item = bksearch(bkt, key);
  if (search_item == NULL) {
    return NULL;
  } else {
    return search_item->p.value;
  }
}

int hashmap_si_delete(hashmap_si *map, char *key) {
  int index = (map->hashfn)(key, map->cap);
  bucket_si bkt = map->data[index];
  int ret = bkdelete(bkt, key);
  return ret;
}

void hashmap_si_init(hashmap_si *map, int init_cap) {
  map->cap = init_cap;
  map->size = 0;
  map->data = malloc((map->cap) * sizeof(map->data));
  map->hashfn = hash_fnv1_si;
}

void hashmap_si_destroy(hashmap_si *map) {
  for (int i = 0; i < map->cap; i++) {
    bucket_item_si *curr = map->data[i];
    while (curr != NULL) {
      bucket_item_si *tmp = curr;
      curr = curr->next;
      free(tmp);
    }
  }
  free(map->data);
}
