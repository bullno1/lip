#ifndef LIP_ARRAY_H
#define LIP_ARRAY_H

#include <stdlib.h>

typedef struct lip_allocator_t lip_allocator_t;

#define lip_array_push(ARRAY, ITEM) \
	(ARRAY = lip_array_resize(ARRAY, lip_array_len(ARRAY) + 1), \
	 ARRAY[lip_array_len(ARRAY) - 1] = ITEM)
#define lip_array_resize(ARRAY, NEW_LENGTH) \
	lip_array__resize(ARRAY, NEW_LENGTH, sizeof(*ARRAY))
#define lip_array_begin(ARRAY) (ARRAY)
#define lip_array_end(ARRAY) ((ARRAY) + lip_array_len(ARRAY))

void* lip_array_new(lip_allocator_t* allocator);
void lip_array_delete(void* array);
size_t lip_array_len(void* array);

// private
void* lip_array__resize(void* array, size_t new_length, size_t item_size);

#endif
