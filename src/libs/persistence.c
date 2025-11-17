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

struct DbFile {
  int fd;
  int flags;
  char filepath[256];
} DbFile;

struct Segment {
  struct DbFile file;
  hashmap_si *offset_map;
} Segment;

#define MAX_SEGMENTS 10
#define SENTINEL UINT32_MAX
#define WRAP_INCR_N(i, n, cap)                                                 \
  ((i) = ((i) + (n) >= (cap) ? (((i) + (n)) - (cap)) : (i) + (n)))
#define WRAP_INCR(i, cap) WRAP_INCR_N(i, 1, cap)

#define WRAP_DECR(i, cap) ((i) = ((i) - (1) >= (0) ? ((i) - (1)) : (cap) - (1)))

#define END_MINUS_ONE(tail, cap) ((tail) == 0 ? ((cap) - 1) : (tail) - 1)

const uint32_t kKeySize = 2;      // in bytes
const uint32_t kValueSize = 2;    // in bytes
const uint32_t kEntrySize = 4;    // in bytes
const uint32_t kNumSegments = 10; // in bytes

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

  uint32_t length =
      (kKeySize + key->len + kKeySize) + (kValueSize + value->len + kValueSize);
  memcpy(response, &length, sizeof(length));
  response += sizeof(length);

  // WARN: dangerous to copy struct as it may copy padding
  memcpy(response, &key->len, kKeySize);
  response += kKeySize;
  memcpy(response, key->data, key->len);
  response += key->len;
  memcpy(response, &key->len, kKeySize);
  response += kKeySize;

  memcpy(response, &value->len, kValueSize);
  response += kValueSize;
  memcpy(response, value->data, value->len);
  response += value->len;
  memcpy(response, &value->len, kValueSize);
  response += kValueSize;

  memcpy(response, &length, sizeof(length));
}

void deserialize_kv_pair(char **fbuf, LString *key, LString *value) {
  // char buf[8 + (4 + key->len) + (4 + value->len) +
  //          8]; // len of entire entry is 8 bytes, double-ended so can read
  //          both dirs

  uint32_t length;
  char *bufptr = *fbuf;
  memcpy(&length, bufptr - sizeof(length), sizeof(length));
  // printf("bufptr: %p\n", bufptr);
  bufptr -= sizeof(length);
  // printf("length: %ld\n", length);
  // if (bufptr < length) {
  //   return;
  // }

  //          4B  ; lenB ; 4B
  // value = [len ; data ; len]
  memcpy(&value->len, bufptr - kValueSize, kValueSize);
  bufptr -= kValueSize;
  value->data = malloc(value->len);
  memcpy(value->data, bufptr - value->len, value->len);
  // printf("value->len: %d\n", value->len);
  bufptr -= value->len;
  bufptr -= kValueSize; // skip the frontal length

  memcpy(&key->len, bufptr - kKeySize, kKeySize);
  bufptr -= kKeySize;
  key->data = malloc(key->len);
  memcpy(key->data, bufptr - key->len, key->len);
  // printf("key->len: %d\n", key->len);
  bufptr -= key->len;
  bufptr -= kKeySize; // skip the frontal length

  bufptr -= sizeof(length);

  *fbuf = bufptr;
}

inline __attribute__((always_inline)) int calc_entry_len(int key_len,
                                                         int value_len) {
  return kEntrySize + (kKeySize + key_len + kKeySize) +
         (kValueSize + value_len + kValueSize) + kEntrySize;
}

int append_to_file(int fd, const char *data, size_t data_len, int offset) {
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

int8_t segment_compact_mrg(struct SegmentList sl) {
  // this has a few stages
  // 1. read from file
  // 2. do the compaction work
  // 3. write to new file
  //
  char *segment_srcs[kNumSegments];
  char *segment_ptrs[kNumSegments];

  int maxdiff = 0;
  for (int i = 0; i < kNumSegments && sl.segments[i].file.fd != 0; i++) {
    struct Segment seg = sl.segments[i];
    struct stat statbuf;
    if (fstat(seg.file.fd, &statbuf) < 0) {
      printf("fstat error");
      return -1;
    }

    segment_srcs[i] =
        mmap(0, statbuf.st_size, PROT_READ, MAP_SHARED, seg.file.fd, 0);
    if (segment_srcs[i] == MAP_FAILED) {
      printf("mmap error");
      return -1;
    }
    printf("[%d] statbuf.st_size: %ld\n", i, statbuf.st_size);

    segment_ptrs[i] = segment_srcs[i] + statbuf.st_size;
    printf("ptr[%d]: %p\n", i, segment_ptrs[i]);
  }

  hashmap_si *map = malloc(sizeof(hashmap_si));
  hashmap_si_init(map, 1);

  FILE *outf = fopen("seg.tmp", "w");
  struct DbFile new_segment_file = {
      .fd = fileno(outf), .flags = O_RDWR, .filepath = "seg.tmp"};

  struct Segment new_seg = {.file = new_segment_file, .offset_map = NULL};

  char batch[1 * 1024 * 1024]; // 1 MiB
  char *batch_tail = batch;

  int offset = 0;
  int rc;
  int total_bytes_written = 0;
  maxdiff = 1;
  while (maxdiff > 0) {
    printf("maxdiff: %d\n", maxdiff);
    maxdiff = 0;
    for (int i = END_MINUS_ONE(sl.tail, 10);
         i + 1 != sl.head && segment_ptrs[i] - segment_srcs[i] > 0;
         WRAP_DECR(i, 10)) {
      printf("i=%d ; sl.head = %d\nptr = %p\nsrc = %p\n", i, sl.head,
             segment_ptrs[i], segment_srcs[i]);
      LString *key = malloc(sizeof(LString));
      LString *value = malloc(sizeof(LString));
      char *ptr = segment_ptrs[i];
      deserialize_kv_pair(&ptr, key, value);
      segment_ptrs[i] = ptr;
      printf("[%d]%.*s -> ", i, key->len, key->data);
      printf("[%d]%.*s\n", i, value->len, value->data);
      // printf("src: %p\n", src);
      // printf("ptr: %p\n", ptr);
      // printf("key_len:%d\n", key.len);
      // printf("val_len:%d\n", key.len);
      rc = hashmap_si_insert(map, key, offset);
      if (rc >= 0) {
        int entry_len = calc_entry_len(key->len, value->len);
        // if entry larger than batch, serialize into large enough buf and flush
        // unlikely; in fact should be avoided TODO: make this impossible
        if (entry_len > sizeof(batch)) {
          // first flush batch
          append_to_file(new_seg.file.fd, batch, batch_tail - batch,
                         total_bytes_written);
          total_bytes_written += (batch_tail - batch);
          batch_tail = batch;

          char entry_buf[entry_len];
          serialize_kv_pair(key, value, entry_buf);
          append_to_file(new_seg.file.fd, entry_buf, entry_len,
                         total_bytes_written);
          total_bytes_written += entry_len;
        } else {
          // will next entry fit?
          if ((batch_tail - batch) + entry_len > sizeof(batch)) {
            // no fit, flush
            append_to_file(new_seg.file.fd, batch, batch_tail - batch,
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
      if (segment_ptrs[i] - segment_srcs[i] > maxdiff) {
        maxdiff = segment_ptrs[i] - segment_srcs[i];
      }
    }
  }

  append_to_file(new_seg.file.fd, batch, batch_tail - batch,
                 total_bytes_written);
  hashmap_si_print_entries_compact(map);

  // LString key2, value2 = {0};
  // deserialize_kv_pair(&ptr, &key2, &value2);
  // printf("src: %p\n", src);
  // printf("ptr: %p\n", ptr);
  // printf("key2_len:%d\n", key2.len);
  // printf("val_len:%d\n", key2.len);
  // printf("%.*s -> ", key2.len, key2.data);
  // printf("%.*s -> ", value2.len, value2.data);

  // TODO: err handling

  for (int i = END_MINUS_ONE(sl.tail, 10); i != sl.head; WRAP_DECR(i, 10)) {
    struct Segment seg = sl.segments[i];
    close(seg.file.fd);
    // unlink(seg.file.filepath);
  }
  sl.segments[END_MINUS_ONE(sl.tail, 10)].file.fd = fileno(outf);
  sl.segments[END_MINUS_ONE(sl.tail, 10)].offset_map = map;
  sl.segments[END_MINUS_ONE(sl.tail, 10)].file = new_segment_file;

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

  struct SegmentList sl = {0};
  sl.head = 0;
  for (int i = 1; i <= 3; i++) {
    char *fp = calloc(128, 1);
    sprintf(fp,
            "/home/usulejmani/development/_personal/build-your-own-redis/"
            "seg_%d.bin",
            i);
    FILE *f = fopen(fp, "r+");
    int fd = fileno(f);
    struct DbFile dbFile = {.fd = fd, .flags = O_RDWR, .filepath = "fp"};
    struct Segment s = {.file = dbFile, .offset_map = NULL};
    sl.segments[i - 1] = s;
    sl.tail = i - 1;
  }
  segment_compact_mrg(sl);
}
