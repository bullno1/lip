#ifndef LIP_UTILS_H
#define LIP_UTILS_H

#include <string.h>
#include <stddef.h>
#include "types.h"

#define LIP_PRINTF_BUFF_SIZE 2048

static inline lip_string_ref_t lip_string_ref(const char* string)
{
	lip_string_ref_t result = { strlen(string), string };
	return result;
}

struct lip__str_align_helper { char c; struct{ uint32_t len; char ptr[2];} s; };
static const uint32_t lip_string_t_align_size =
	offsetof(struct lip__str_align_helper, s);

static inline size_t lip_string_align(uint32_t len)
{
	size_t entry_size = sizeof(lip_string_t) + len;
	return (entry_size + lip_string_t_align_size - 1) / lip_string_t_align_size * lip_string_t_align_size;
}

static inline bool lip_string_ref_equal(
	lip_string_ref_t lhs,
	lip_string_ref_t rhs
)
{
	return lhs.length == rhs.length && memcmp(lhs.ptr, rhs.ptr, lhs.length) == 0;
}

size_t lip_fread(void* ptr, size_t size, void* file);

size_t lip_fwrite(const void* ptr, size_t size, void* file);

void lip_printf(
	lip_write_fn_t write_fn, void* ctx, const char* format, ...
)
#ifdef __GNUC__
__attribute__((format(printf, 3, 4)))
#endif
;

#endif
