#ifndef LIP_UTILS_H
#define LIP_UTILS_H

#include <lip/core/common.h>
#include <lip/core/io.h>
#include <stdarg.h>

#if defined(__GNUC__) || defined(__GNUG__) || defined(__clang__)
#	define LIP_LIKELY(cond) __builtin_expect(cond, 1)
#	define LIP_UNLIKELY(cond) __builtin_expect(cond, 0)
#else
#	define LIP_LIKELY(cond) cond
#	define LIP_UNLIKELY(cond) cond
#endif

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

LIP_MAYBE_UNUSED static inline void
lip_set_last_error(
	lip_last_error_t* last_error,
	unsigned int code, lip_loc_range_t location, const void* extra
)
{
	last_error->errorp = &last_error->error;
	last_error->error.code = code;
	last_error->error.location = location;
	last_error->error.extra = extra;
}

LIP_MAYBE_UNUSED static inline void
lip_clear_last_error(lip_last_error_t* last_error)
{
	last_error->errorp = NULL;
}

LIP_MAYBE_UNUSED static inline const lip_error_t*
lip_last_error(lip_last_error_t* last_error)
{
	return last_error->errorp;
}

#endif
