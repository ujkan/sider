#ifndef ARRAY_H
#define ARRAY_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
typedef struct {
    void *data;
    uint64_t len;
} Array;


#define array_index(a,t,i)      (((t*) (void *) (a)->data) [(i)])
/*Array*  array_new        (void);*/
Array*  array_sized_new  (uint64_t reserved_size, uint64_t element_size);
/*Array*  array_new_full   (uint64_t reserved_size, void (*element_free_func)(void *));*/
void       array_set_length (Array *array, uint64_t length);
void       array_set        (Array *array, uint64_t index, void * item);
void       array_free       (Array *array, bool free_seg);
void       array_push       (Array *array, void * const element );
void       array_push_many  (Array *array, const void *start, uint64_t n);
int        array_copy_n     (Array *src, void *dest, uint64_t n);
int        array_pop_first  (Array *array, void *ret_val);
int        array_pop_last   (Array *array, void *ret_val);

#endif // ARRAY_H
