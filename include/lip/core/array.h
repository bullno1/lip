#ifndef LIP_CORE_ARRAY_H
#define LIP_CORE_ARRAY_H

/**
 * @defgroup array Array
 * @brief Flexible array.
 *
 * @{
 */

#include "common.h"

/**
 * @brief Create a new array.
 * @param ALLOCATOR The allocator to use.
 * @param TYPE Type of elements.
 * @param CAPACITY Initial capacity.
 */
#define lip_array_create(ALLOCATOR, TYPE, CAPACITY) \
	((TYPE*)(lip_array__create(\
		(ALLOCATOR), \
		sizeof(TYPE), \
		LIP_ALIGN_OF(TYPE), \
		(CAPACITY))))

/// Push an element to the end of the array.
#define lip_array_push(ARRAY, ITEM) \
	((ARRAY) = lip_array__prepare_push((ARRAY)), \
	 ((ARRAY))[lip_array_len((ARRAY)) - 1] = (ITEM))

/// Resize an array.
#define lip_array_resize(ARRAY, NEW_LENGTH) \
	((ARRAY) = lip_array__resize((ARRAY), (NEW_LENGTH)))

/// Get the begin iterator the array.
#define lip_array_begin(ARRAY) (ARRAY)

/// Get the end iterator of an array.
#define lip_array_end(ARRAY) ((ARRAY) + lip_array_len((ARRAY)))

/**
 * @brief Loop through an array.
 * @param TYPE element type.
 * @param VAR iterator variable.
 * @param ARRAY array to loop through.
 */
#define lip_array_foreach(TYPE, VAR, ARRAY) \
	for(TYPE* VAR = lip_array_begin((ARRAY)); VAR != lip_array_end((ARRAY)); ++(VAR))

/// Remove an item from an array by overwriting it with the last item.
#define lip_array_quick_remove(ARRAY, INDEX) \
	((ARRAY)[(INDEX)] = (ARRAY)[lip_array_len((ARRAY)) - 1], \
	 lip_array_resize((ARRAY), lip_array_len((ARRAY)) - 1))

/// Reserve space at the end of the array and return pointer to that slot.
#define lip_array_alloc(ARRAY) \
	((ARRAY) = lip_array__prepare_push((ARRAY)), \
	 &(ARRAY)[lip_array_len((ARRAY)) - 1])

/// Destroy an array.
LIP_CORE_API void
lip_array_destroy(void* array);

/// Remove all items from an array.
LIP_CORE_API void
lip_array_clear(void* array);

/// Get the length of an array.
LIP_CORE_API size_t
lip_array_len(const void* array);

// private
LIP_CORE_API void*
lip_array__create(
	lip_allocator_t* allocator,
	size_t elem_size,
	uint8_t alignment,
	size_t capacity
);

LIP_CORE_API void*
lip_array__prepare_push(void* array);

LIP_CORE_API void*
lip_array__resize(void* array, size_t new_length);

/**
 * @}
 */

#endif
