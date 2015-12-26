#ifndef LIP_TYPES_H
#define LIP_TYPES_H

#include <stddef.h>

typedef struct lip_loc_t
{
	unsigned int line;
	unsigned int column;
} lip_loc_t;

typedef struct lip_string_ref_t
{
	size_t length;
	const char* ptr;
} lip_string_ref_t;

#define lip_array(T) T*

#endif
