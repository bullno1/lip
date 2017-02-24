#ifndef LIP_UTILS_H
#define LIP_UTILS_H

#include <lip/common.h>

LIP_MAYBE_UNUSED static inline lip_string_ref_t
lip_copy_string_ref(lip_allocator_t* allocator, lip_string_ref_t str)
{
	char* copy = lip_malloc(allocator, str.length + 1);
	memcpy(copy, str.ptr, str.length);
	copy[str.length] = '\0';
	return (lip_string_ref_t){
		.length = str.length,
		.ptr = copy
	};
}

#endif
