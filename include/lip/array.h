#ifndef LIP_ARRAY_H
#define LIP_ARRAY_H

#include "common.h"

#define lip_array_create(ALLOCATOR, TYPE, CAPACITY) \
	((TYPE*)(lip_array__create(\
		(ALLOCATOR), \
		sizeof(TYPE), \
		LIP_ALIGN_OF(TYPE), \
		(CAPACITY))))
#define lip_array_push(ARRAY, ITEM) \
	((ARRAY) = lip_array__prepare_push((ARRAY)), \
	 ((ARRAY))[lip_array_len((ARRAY)) - 1] = (ITEM))
#define lip_array_resize(ARRAY, NEW_LENGTH) \
	((ARRAY) = lip_array__resize((ARRAY), (NEW_LENGTH)))
#define lip_array_begin(ARRAY) (ARRAY)
#define lip_array_end(ARRAY) ((ARRAY) + lip_array_len((ARRAY)))
#define lip_array_foreach(TYPE, VAR, ARRAY) \
	for(TYPE* VAR = lip_array_begin((ARRAY)); VAR != lip_array_end((ARRAY)); ++(VAR))
#define lip_array_quick_remove(ARRAY, INDEX) \
	((ARRAY)[(INDEX)] = (ARRAY)[lip_array_len((ARRAY)) - 1], \
	 lip_array_resize((ARRAY), lip_array_len((ARRAY)) - 1))
#define lip_array_alloc(ARRAY) \
	((ARRAY) = lip_array__prepare_push((ARRAY)), \
	 &(ARRAY)[lip_array_len((ARRAY)) - 1])

LIP_API void
lip_array_destroy(void* array);

LIP_API void
lip_array_clear(void* array);

LIP_API size_t
lip_array_len(const void* array);

// private
LIP_API void*
lip_array__create(
	lip_allocator_t* allocator,
	size_t elem_size,
	uint8_t alignment,
	size_t capacity
);

LIP_API void*
lip_array__prepare_push(void* array);

LIP_API void*
lip_array__resize(void* array, size_t new_length);

#endif
