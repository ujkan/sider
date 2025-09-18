#ifndef PTR_ARRAY_H
#define PTR_ARRAY_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
typedef struct {
    void **data;
    uint64_t len;
} PtrArray;


#define   ptr_array_index(array,index_) ((array)->data)[index_]
PtrArray*  ptr_array_new        (void);
PtrArray*  ptr_array_sized_new  (uint64_t reserved_size);
PtrArray*  ptr_array_new_full   (uint64_t reserved_size, void (*element_free_func)(void *));
void       ptr_array_add        (PtrArray *array, void * item);
void       ptr_array_set_length (PtrArray *array, uint64_t length);
void       ptr_array_set        (PtrArray *array, uint64_t index, void * item);
void       ptr_array_free       (PtrArray *array, bool free_seg);

#endif // PTR_ARRAY_H
