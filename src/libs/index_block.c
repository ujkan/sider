#include "index_block.h"
#include "lstr.h"
#include "scribe.h"
#include <stdio.h>
#include <stdlib.h>

LString *IndexItem_serialize(struct IndexItem *iitem) {
  u16 len = iitem->key->len + sizeof(iitem->offset) + sizeof(iitem->key->len);
  u8 *data = malloc(len);
  u8 *dataptr = data;
  scribe_put_u16(&dataptr, iitem->key->len);
  scribe_put_bytes(&dataptr, iitem->key->data, iitem->key->len);
  scribe_put_u32(&dataptr, iitem->offset);
  LString *serialized = malloc(sizeof(LString));
  serialized->len = len;
  serialized->data = data;
  return serialized;
}
LString *IndexSection_serialize(struct IndexSection *iblock) {
  // TODO: add restart points
  u8 *buf = malloc(1024 * 1024);
  u8 *bufptr = buf;
  for (int i = 0; i < arrlen(iblock->items); i++) {
    LString *serialized = IndexItem_serialize(&iblock->items[i]);
    scribe_put_bytes(&bufptr, serialized->data, serialized->len);
    lstring_free(serialized);
  }
  LString *serialized = malloc(sizeof(LString));
  serialized->len = bufptr - buf;
  serialized->data = buf;
  return serialized;
}

struct IndexSection *IndexSection_deserialize(char *buf, u32 len) {
  // TODO: add restart points
  struct IndexSection *section = malloc(sizeof(struct IndexSection));
  section->items = NULL;
  u8 *bufptr = (u8 *)buf;
  arrsetcap(section->items, 32);
  while ((bufptr - (u8 *)buf) < len) {
    LString *key = lstring_deserialize((const u8 **)&bufptr);
    u32 offset = scribe_get_u32((const u8 **)(&bufptr));
    struct IndexItem item = {0};
    item.key = key;
    item.offset = offset;
    arrpush(section->items, item);
  }
  return section;
}

struct IndexItem *IndexSection_search(char *index_section,
                                      u32 *index_section_restart_points,
                                      u32 count, LString *key) {
  i32 start = 0;
  i32 end = count - 1;
  const u8 *index_section_cursor = (u8 *)index_section;
  LString *curr_index_item_key = NULL;

  struct IndexItem *item = malloc(sizeof(struct IndexItem));
  while (start <= end) {
    int mid = start + (end - start) / 2;
    u32 idx = index_section_restart_points[mid];
    index_section_cursor = &index_section[idx];
    curr_index_item_key =
        lstring_deserialize(&index_section_cursor); // TODO: free
    i32 cmp = lstring_compare(key, curr_index_item_key);
    item->key = curr_index_item_key;
    item->offset = scribe_get_u32((const u8 **)(&index_section_cursor));
    if (cmp == 0) {
      return item;
    } else if (cmp > 0) {
      start = mid + 1;
    } else {
      end = mid - 1;
    }
  }
  return item;
}
