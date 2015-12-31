#ifndef LIP_ARRAY_H
#define LIP_ARRAY_H

#include <stddef.h>

typedef struct lip_allocator_t lip_allocator_t;

//TODO: convert all use of array to lip_array(type) instead of type*
//TODO: two kinds of resize: one exact and one double
#define lip_array_push(ARRAY, ITEM) \
	(lip_array_resize((ARRAY), lip_array_len(ARRAY) + 1), \
	 (ARRAY)[lip_array_len(ARRAY) - 1] = ITEM)
#define lip_array_resize(ARRAY, NEW_LENGTH) \
	((ARRAY) = lip_array__resize((ARRAY), NEW_LENGTH, sizeof(*(ARRAY))))
#define lip_array_begin(ARRAY) (ARRAY)
#define lip_array_end(ARRAY) ((ARRAY) + lip_array_len(ARRAY))
#define lip_array_foreach(TYPE, VAR, ARRAY) \
	for(TYPE* VAR = lip_array_begin(ARRAY); VAR != lip_array_end(ARRAY); ++(VAR))
#define lip_array_quick_remove(ARRAY, INDEX) \
	((ARRAY)[(INDEX)] = (ARRAY)[lip_array_len(ARRAY) - 1], \
	 lip_array_resize((ARRAY), lip_array_len(ARRAY) - 1))

void* lip_array_new(lip_allocator_t* allocator);
void lip_array_delete(void* array);
void lip_array_clear(void* array);
size_t lip_array_len(void* array);

// private
void* lip_array__resize(void* array, size_t new_length, size_t item_size);

#endif
