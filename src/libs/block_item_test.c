#include "block_item.h"
#include "ctype.h"
#include "hex_dump.h"
#include <stdio.h>

int main(void) {

  struct BlockItem item = {.shared = 3,
                           .suffix_len = 2,
                           .suffix = "ab",
                           .value_len = 5,
                           .value = "hello"};

  LString *l = BlockItem_serialize(&item);

  printf("len=%d\n", l->len);

  hex_dump(l->data, l->len);
}
