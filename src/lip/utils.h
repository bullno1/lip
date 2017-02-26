#ifndef LIP_UTILS_H
#define LIP_UTILS_H

#include <lip/common.h>
#include <lip/io.h>
#include <stdarg.h>

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

LIP_MAYBE_UNUSED static inline lip_string_ref_t
lip_string_ref_from_string(lip_string_t* str)
{
	return (lip_string_ref_t){
		.length = str->length,
		.ptr = str->ptr
	};
}

LIP_MAYBE_UNUSED static inline size_t
lip_sprintf(lip_array(char)* buf, const char* fmt, ...)
{
	va_list args;
	struct lip_osstream_s osstream;
	lip_out_t* out = lip_make_osstream(buf, &osstream);
	va_start(args, fmt);
	size_t result = lip_vprintf(out, fmt, args);
	va_end(args);
	return result;
}

#endif
