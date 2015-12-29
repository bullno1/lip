#ifndef LIP_UTILS_H
#define LIP_UTILS_H

#include <string.h>
#include "types.h"

static inline lip_string_ref_t lip_string_ref(const char* string)
{
	lip_string_ref_t result = { strlen(string), string };
	return result;
}

#endif
