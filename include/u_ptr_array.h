#ifndef PTR_ARRAY_H
#define PTR_ARRAY_H

#include "types.h"
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
typedef struct {
    void **data;
    u64 len;
} PtrArray;


#define   ptr_array_index(array,index_) ((array)->data)[index_]
PtrArray*  ptr_array_new        (void);
PtrArray*  ptr_array_sized_new  (u64 reserved_size);
PtrArray*  ptr_array_new_full   (u64 reserved_size, void (*element_free_func)(void *));
void       ptr_array_add        (PtrArray *array, void * item);
void       ptr_array_set_length (PtrArray *array, u64 length);
void       ptr_array_set        (PtrArray *array, u64 index, void * item);
void       ptr_array_free       (PtrArray *array, bool free_seg);

#endif // PTR_ARRAY_H
