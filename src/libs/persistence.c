#include "bytering.h"
#include "hmap_si.h"
#include "lstr.h"
#include <alloca.h>
#include <byteswap.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
// Store in data segments (files)
// Once file gets beyond certain size, create new segment
// Periodically perform *compaction + merge* on segments
//
// For each segment we need a hash table mapping keys to offsets in file
// To find key, search from most recent to last
//
//
//

struct Segment {
  int fd;
  hashmap_si offset_map;
} Segment;

#define MAX_SEGMENTS 10
#define SENTINEL UINT32_MAX
#define WRAP_INCR_N(i, n, cap)                                                 \
  ((i) = ((i) + (n) >= (cap) ? (((i) + (n)) - (cap)) : (i) + (n)))
#define WRAP_INCR(i, cap) WRAP_INCR_N(i, 1, cap)

struct SegmentList {
  struct Segment segments[MAX_SEGMENTS];
  uint32_t head;
  uint32_t tail;
} SegmentList;

int8_t segment_list_add(struct SegmentList *seg_list, struct Segment *seg) {
  if (seg_list->head == seg_list->tail) {
    return -1; // full
  }
  seg_list->segments[seg_list->tail] = *seg;
  WRAP_INCR(seg_list->tail, MAX_SEGMENTS);
  return 0;
}

void serialize_kv_pair(LString *key, LString *value, char *response) {
  // char buf[8 + (4 + key->len + 4) + (4 + value->len + 4) +
  //          8]; // len of entire entry is 8 bytes, double-ended so can read
  //          both dirs

  uint64_t length = (4 + key->len + 4) + (4 + value->len + 4);
  memcpy(response, &length, 8);
  response += 8;

  // WARN: dangerous to copy struct as it may copy padding
  memcpy(response, &key->len, 4);
  response += 4;
  memcpy(response, key->data, key->len);
  response += key->len;
  memcpy(response, &key->len, 4);
  response += 4;

  memcpy(response, &value->len, 4);
  response += 4;
  memcpy(response, value->data, value->len);
  response += value->len;
  memcpy(response, &value->len, 4);
  response += 4;

  memcpy(response, &length, 8);
}

void deserialize_kv_pair(char **fbuf, LString *key, LString *value) {
  // char buf[8 + (4 + key->len) + (4 + value->len) +
  //          8]; // len of entire entry is 8 bytes, double-ended so can read
  //          both dirs

  uint64_t length;
  char *bufptr = *fbuf;
  memcpy(&length, bufptr - 8, 8);
  // printf("bufptr: %p\n", bufptr);
  bufptr -= 8;
  // printf("length: %ld\n", length);
  // if (bufptr < length) {
  //   return;
  // }

  //          4B  ; lenB ; 4B
  // value = [len ; data ; len]
  memcpy(&value->len, bufptr - 4, 4);
  bufptr -= 4;
  value->data = malloc(value->len);
  memcpy(value->data, bufptr - value->len, value->len);
  // printf("value->len: %d\n", value->len);
  bufptr -= value->len;
  bufptr -= 4; // skip the frontal length

  memcpy(&key->len, bufptr - 4, 4);
  bufptr -= 4;
  key->data = malloc(key->len);
  memcpy(key->data, bufptr - key->len, key->len);
  // printf("key->len: %d\n", key->len);
  bufptr -= key->len;
  bufptr -= 4; // skip the frontal length

  bufptr -= 8;

  *fbuf = bufptr;
}

int calc_entry_len(int key_len, int value_len) {
  return 8 + (4 + key_len + 4) + (4 + value_len + 4) + 8;
}

int fastest_append(int fd, const char *data, size_t data_len, int offset) {
  // 1. Move file pointer to the end of the file

  // 2. Write the data
  if (pwrite(fd, data, data_len, offset) == (ssize_t)-1) {
    perror("write failed");
    return -1;
  }

  // 3. (Optional but recommended) Ensure data is on disk
  if (fsync(fd) == -1) {
    perror("fsync failed"); // Important for durability
    return -1;
  }

  return 0;
}

int8_t segment_compact(struct Segment *seg) {
  // this has a few stages
  // 1. read from file
  // 2. do the compaction work
  // 3. write to new file

  struct stat statbuf;
  if (fstat(seg->fd, &statbuf) < 0) {
    printf("fstat error");
    return -1;
  }

  char *src;
  src = mmap(0, statbuf.st_size, PROT_READ, MAP_SHARED, seg->fd, 0);
  if (src == MAP_FAILED) {
    printf("mmap error");
    return -1;
  }
  printf("statbuf.st_size: %ld\n", statbuf.st_size);

  char *ptr = src + statbuf.st_size;
  printf("ptr: %p\n", ptr);

  hashmap_si map = {0};
  hashmap_si_init(&map, 1);

  FILE *outf = fopen("seg.tmp", "w");

  struct Segment new_seg = {.fd = fileno(outf), .offset_map = map};

  char batch[1 * 1024 * 1024]; // 1 MiB
  char *batch_tail = batch;

  int offset = 0;
  int rc;
  int total_bytes_written = 0;
  while (ptr > src) {
    LString *key = malloc(sizeof(LString));
    LString *value = malloc(sizeof(LString));
    deserialize_kv_pair(&ptr, key, value);
    printf("%.*s -> ", key->len, key->data);
    printf("%.*s\n", value->len, value->data);
    // printf("src: %p\n", src);
    // printf("ptr: %p\n", ptr);
    // printf("key_len:%d\n", key.len);
    // printf("val_len:%d\n", key.len);
    rc = hashmap_si_insert(&map, key, offset);
    if (rc >= 0) {
      int entry_len = calc_entry_len(key->len, value->len);
      // if entry larger than batch, serialize into large enough buf and flush
      // unlikely; in fact should be avoided TODO: make this impossible
      if (entry_len > sizeof(batch)) {
        // first flush batch
        fastest_append(new_seg.fd, batch, batch_tail - batch,
                       total_bytes_written);
        total_bytes_written += (batch_tail - batch);
        batch_tail = batch;

        char entry_buf[entry_len];
        serialize_kv_pair(key, value, entry_buf);
        fastest_append(new_seg.fd, entry_buf, entry_len, total_bytes_written);
        total_bytes_written += entry_len;
      } else {
        // will next entry fit?
        if ((batch_tail - batch) + entry_len > sizeof(batch)) {
          // no fit, flush
          fastest_append(new_seg.fd, batch, batch_tail - batch,
                         total_bytes_written);
          total_bytes_written += (batch_tail - batch);
          batch_tail = batch;
        }
        serialize_kv_pair(key, value, batch_tail);
        batch_tail += entry_len;
      }
      offset += entry_len;
    }
    free(value->data);
    free(value);
  }

  fastest_append(new_seg.fd, batch, batch_tail - batch, total_bytes_written);
  hashmap_si_print_entries_compact(&map);

  // LString key2, value2 = {0};
  // deserialize_kv_pair(&ptr, &key2, &value2);
  // printf("src: %p\n", src);
  // printf("ptr: %p\n", ptr);
  // printf("key2_len:%d\n", key2.len);
  // printf("val_len:%d\n", key2.len);
  // printf("%.*s -> ", key2.len, key2.data);
  // printf("%.*s -> ", value2.len, value2.data);

  return 0;
}

int8_t segment_list_compact_and_merge(struct SegmentList *seg_list) {
  if (seg_list->head == SENTINEL) {
    return -1; // empty
  }

  return 0;
}
int main() {
  u8 key_data[2] = "hi";
  LString key = {.len = 2, .data = key_data};
  u8 value_data[5] = "world";
  LString value = {.len = 5, .data = value_data};

  char resp[39];
  serialize_kv_pair(&key, &value, resp);
  for (int i = 0; i < 39; i++) {
    printf("%02d ", i);
  }
  printf("\n");
  for (int i = 0; i < 39; i++) {
    printf("%02x ", resp[i]);
  }
  printf("\n");

  FILE *f = fopen(
      "/home/usulejmani/development/_personal/build-your-own-redis/my_data.bin",
      "r+");
  int fd = fileno(f);
  struct Segment s = {.fd = fd, .offset_map = NULL};

  segment_compact(&s);
}
