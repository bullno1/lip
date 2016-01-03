#ifndef LIP_TYPES_H
#define LIP_TYPES_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "fuck_msvc.h"

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

typedef struct lip_string_t
{
	uint32_t length;
	char ptr[];
} lip_string_t;

#define lip_array(T) T*

typedef struct lip_allocator_t lip_allocator_t;

typedef size_t (*lip_read_fn_t)(void* ptr, size_t size, void* context);
typedef size_t (*lip_write_fn_t)(const void* ptr, size_t size, void* context);

#endif
