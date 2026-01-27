#ifndef ARRAY_H
#define ARRAY_H

#include "types.h"
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
typedef struct {
    void *data;
    u64 len;
} Array;


#define array_index(a,t,i)      (((t*) (void *) (a)->data) [(i)])
/*Array*  array_new        (void);*/
Array*  array_sized_new  (u64 reserved_size, u64 element_size);
Array*  array_new_full   (u64 reserved_size, u64 element_size, void (*element_free_func)(void *));
void       array_set_length (Array *array, u64 length);
void       array_set        (Array *array, u64 index, void * item);
void       array_free       (Array *array, bool free_seg);
void       array_push       (Array *array, void * const element );
void       array_push_many  (Array *array, const void *start, u64 n);
i32        array_copy_n     (Array *src, void *dest, u64 n);
i32        array_pop_first  (Array *array, void *ret_val);
i32        array_pop_last   (Array *array, void *ret_val);

#endif // ARRAY_H
