#include "block.h"
#include "hex_dump.h"
#include "index_block.h"
#include "stb_ds.h"
#include "u_array.h"
#include <lz4.h>
#include <stdio.h>
#include "persistence.h"

struct SSTPair {
  u16 tag;
  LString key;
  u8 tombstone;
  LString value;
};

int main(void) {

  Array *pairs = array_sized_new(512, sizeof(struct SSTPair));
  for (int i = 0; i < 512; i++) {
    u8 *key = malloc(6);
    snprintf(key, 128, "key%03d", i);
    u8 *value = malloc(4);
    snprintf(value, 128, "v%03d", i);
    struct SSTPair pair = {.tag = 0,
                           .key = {.data = key, .len = 6},
                           .tombstone = 0,
                           .value = {.data = value, .len = 4}};
    array_push(pairs, &pair);
  }
  char *buf = malloc(1024 * 1024 * 1024);
  char *decomp = malloc(1024 * 1024 * 1024);
  int len = 0;
  compress_and_write_v2(pairs, 0, buf, &len);
  int decomp_len = LZ4_decompress_safe(buf, decomp, len, 1024 * 1024 * 1024);
  printf("DECOMPL_LEN: %d\n", decomp_len);
  hex_dump(decomp, 1024 * 1024 * 8);
}
