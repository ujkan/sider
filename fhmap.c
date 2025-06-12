struct hashmap {
  // char** data[]; // may need to store as char*** due to unknown size
  // actually, better as ptr because can be freed later when resizing, whereas stack alloc is lost
  bucket* data; // list of bucket's
  int size;
  int cap;
} hashmap;

struct pair {
  char* key;
  char* value;
} pair;

struct bucket_item {
  pair p;
  bucket_item *next;
} bucket_item;

typedef bucket bucket_item*;
int hash(char* key, int kssize);

int hashmap_upsert(hashmap* map, char* key, char* value) {
  if (map->size < map->cap) {
    auto data = map->data;
    int index = hash(key, map->cap);
    // data[i] stores pointer to heap
    // where char* is stored
    // question: why not just use char*?
    int bidx = bsearch(data[index], key);
    if (bidx == -1) { // no exist, insert
      bucket_item new_item;
      new_item.key = key;
      new_item.value = value;
      data[index] = bappend(data[index], new_item);
      map->size++;
    } else { // exist 
      data[index][bidx].p.key = key;
      data[index][bidx].p.value = value;
    }
  } else {
    hashmap_rehash(map);
    hashmap_upsert(map, key, value); // can i reuse? or is recursion dangerous; should i keep a reusable safe upsert to avoid recursion?
  }
}

int hashmap_rehash(hashmap* map) {
  map->cap *= 2;
  bucket *new_data = malloc(map->cap * sizeof(bucket)); // sizeof correct here?
  // when rehashing, can we break collisions? probably should try
  bucket curr;
  for (int i = 0; i < map->size; i++) {
    curr = (map->data)[i];
    // for each entry in old bucket append to a bucket in the new data
    while (curr != NULL) {
      char* key = (curr->p).key
      int index = hash(key, map->cap);
      new_data[index] = bappend(new_data[index], curr->p);
      curr = curr->next;
    }
  }

  free(map->data);

  map->data = new_data; // use after free? or assignment ok?
}



bucket bappend(bucket b, bucket_item p) {
  bucket_item *nb = malloc(sizeof(bucket));
  (nb->p).key = p.key;
  (nb->p).value = p.value;
  nb->next = NULL;
  bucket_item *curr = b;
  if (curr == NULL) {
     return nb;
  }

  while (curr != NULL) {
    curr = curr->next;
  }
  curr->next = nb;
  return b;
}



void bappend(bucket b, bucket_item p) {
  bucket_item curr = *b;
  while (curr->next != NULL) {
    curr = curr->next;
  }
  curr->next = p;
}



// maybe return ptr to bucket_item to avoid returning structs
int bsearch(bucket b, char* key) {
  int index;
  bucket_item *curr;
  for(curr = b; curr != NULL; curr = curr->next;) {
    if (strcmp((curr->p).key, key)) {
      return index;
    }
    index++;
  }
  return -1;
}



void hashmap_rehash(hashmap* map) {
  map->cap *= 2;
  bucket *new_data = malloc(map->cap * sizeof(bucket)); // sizeof correct here?

  // when rehashing, can we break collisions? probably should try

  for (int i = 0; i < map -> size; i++) {
    int index = hash((map->data)[i].key, map -> cap);
    new_data[index] = (map->data)[i].value;
  }

  free(map->data);

  map->data = new_data; // use after free? or assignment ok?

}

