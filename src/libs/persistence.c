#include "bytering.h"
#include "hmap_si.h"
#include "lstr.h"
#include "scribe.h"
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
  int size;
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

const uint32_t kKeySize = 2;                       // in bytes
const uint32_t kValueSize = 2;                     // in bytes
const uint32_t kEntrySize = 4;                     // in bytes
const uint32_t kNumSegments = 10;                  // in bytes
const uint32_t kMaxSegmentSize = 10 * 1024 * 1024; // 10 MiB

struct SegmentList {
  struct Segment inactive_segments[MAX_SEGMENTS];
  struct Segment active_segment;
  uint32_t size;
} SegmentList;

int8_t segment_list_add(struct SegmentList *seg_list, struct Segment *seg) {
  if (seg_list->size == MAX_SEGMENTS) {
    return -1; // full
  }
  seg_list->inactive_segments[seg_list->size] = *seg;
  seg_list->size += 1;
  return 0;
}

void serialize_kv_pair(LString *key, LString *value, char *response) {
  // char buf[8 + (4 + key->len + 4) + (4 + value->len + 4) +
  //          8]; // len of entire entry is 8 bytes, double-ended so can read
  //          both dirs

  uint32_t length =
      (kKeySize + key->len + kKeySize) + (kValueSize + value->len + kValueSize);
  u8 *cursor = (u8 *)response;
  scribe_put_u32(&cursor, length);

  // WARN: dangerous to copy struct as it may copy padding
  scribe_put_u16(&cursor, key->len);
  scribe_put_bytes(&cursor, key->data, key->len);
  scribe_put_u16(&cursor, key->len);

  scribe_put_u16(&cursor, value->len);
  scribe_put_bytes(&cursor, value->data, value->len);
  scribe_put_u16(&cursor, value->len);

  scribe_put_u32(&cursor, length);
}

void deserialize_kv_pair(char **fbuf, LString *key, LString *value) {
  // char buf[8 + (4 + key->len) + (4 + value->len) +
  //          8]; // len of entire entry is 8 bytes, double-ended so can read
  //          both dirs

  uint32_t length;
  const u8 *end = (const u8 *)*fbuf;
  const u8 *bufptr = end - sizeof(length);
  length = scribe_get_u32(&bufptr);
  printf("bufptr: %p\n", bufptr);
  printf("length: %ld\n", length);
  // if (bufptr < length) {
  //   return;
  // }

  //          4B  ; lenB ; 4B
  // value = [len ; data ; len]
  const u8 *value_len_ptr = end - sizeof(length) - kValueSize;
  value->len = scribe_get_u16(&value_len_ptr);
  value->data = malloc(value->len);
  const u8 *value_data_ptr = value_len_ptr - value->len;
  scribe_get_bytes(&value_data_ptr, value->data, value->len);
  printf("value->len: %d\n", value->len);
  const u8 *key_len_ptr = value_len_ptr - value->len - kValueSize;

  key->len = scribe_get_u16(&key_len_ptr);
  key->data = malloc(key->len);
  const u8 *key_data_ptr = key_len_ptr - key->len;
  scribe_get_bytes(&key_data_ptr, key->data, key->len);
  printf("key->len: %d\n", key->len);

  *fbuf = (char *)(end - (length + 2 * sizeof(length)));
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

int8_t segment_compact_mrg(struct SegmentList *sl) {
  // this has a few stages
  // 1. read from file
  // 2. do the compaction work
  // 3. write to new file
  //
  char *segment_srcs[kNumSegments];
  char *segment_ptrs[kNumSegments];

  int maxdiff = 0;
  for (int i = 0; i < sl->size; i++) {
    struct Segment seg = sl->inactive_segments[i];
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
    for (int i = sl->size - 1; i >= 0 && segment_ptrs[i] - segment_srcs[i] > 0;
         i--) {
      printf("i=%d ; ptr = %p\nsrc = %p\n", i, segment_ptrs[i],
             segment_srcs[i]);
      LString *key = malloc(sizeof(LString));
      LString *value = malloc(sizeof(LString));
      char *ptr = segment_ptrs[i];
      deserialize_kv_pair(&ptr, key, value);
      segment_ptrs[i] = ptr;
      printf("[%d]%.*s -> ", i, key->len, key->data);
      printf("[%d]%.*s\n", i, value->len, value->data);
      printf("src: %p\n", src);
      printf("ptr: %p\n", ptr);
      printf("key_len:%d\n", key.len);
      printf("val_len:%d\n", key.len);
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
  printf("src: %p\n", src);
  printf("ptr: %p\n", ptr);
  printf("key2_len:%d\n", key2.len);
  printf("val_len:%d\n", key2.len);
  printf("%.*s -> ", key2.len, key2.data);
  printf("%.*s -> ", value2.len, value2.data);

  // TODO: err handling

  for (int i = sl->size - 1; i >= 0; i--) {
    struct Segment seg = sl->inactive_segments[i];
    close(seg.file.fd);
    unlink(seg.file.filepath);
  }
  sl->inactive_segments[0].file.fd = fileno(outf);
  sl->inactive_segments[0].offset_map = map;
  new_segment_file.size = total_bytes_written;
  sl->inactive_segments[0].file = new_segment_file;
  rename(new_segment_file.filepath,
         "/home/usulejmani/development/_personal/build-your-own-redis/"
         "seg_0.bin");
  sl->size = 1;

  return 0;
}

// if < max_segments - 1
// then sl_add and normal logic
// if >= max_segments - 1
// then sl_add + sl_compactmrg and normal logic

void segment_list_new_active(struct SegmentList *seg_list) {
  if (seg_list->size >= MAX_SEGMENTS) {
    return; // error
  }
  segment_list_add(seg_list, &seg_list->active_segment);
  if (seg_list->size == MAX_SEGMENTS) {
    segment_compact_mrg(seg_list);
  }

  char fp[128];
  sprintf(fp,
          "/home/usulejmani/development/_personal/build-your-own-redis/"
          "seg_%d.bin",
          seg_list->size);
  FILE *f = fopen(fp, "r+");
  int fd = fileno(f);
  struct DbFile dbFile = {.fd = fd, .flags = O_RDWR};
  memcpy(dbFile.filepath, fp, 128);
  hashmap_si *offset_map = malloc(sizeof(hashmap_si));
  hashmap_si_init(offset_map, 128);
  struct Segment active = {.file = dbFile, .offset_map = offset_map};
  seg_list->active_segment = active;
}

void persist(struct SegmentList *sl, LString *key, LString *value) {
  int entry_len = calc_entry_len(key->len, value->len);
  if (sl->active_segment.file.size + entry_len > kMaxSegmentSize) {
    segment_list_new_active(sl);
  }

  char buf[entry_len];
  serialize_kv_pair(key, value, buf);

  append_to_file(sl->active_segment.file.fd, buf, entry_len,
                 sl->active_segment.file.size);
  sl->active_segment.file.size += entry_len;
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
  for (int i = 1; i <= 3; i++) {
    char *fp = calloc(128, 1);
    sprintf(fp,
            "/home/usulejmani/development/_personal/build-your-own-redis/"
            "seg_%d.bin",
            i);
    FILE *f = fopen(fp, "r+");
    int fd = fileno(f);
    struct DbFile dbFile = {.fd = fd, .flags = O_RDWR};
    memcpy(dbFile.filepath, fp, 128);
    struct Segment s = {.file = dbFile, .offset_map = NULL};
    if (i != 3) {
      sl.inactive_segments[i - 1] = s;
      sl.size = i - 1;
    } else {
      sl.active_segment = s;
    }
  }
  segment_compact_mrg(&sl);
  persist(&sl, &key, &value);
}
