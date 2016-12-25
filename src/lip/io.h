#ifndef LIP_IO_H
#define LIP_IO_H

#include <stdarg.h>
#include <stdio.h>
#include "common.h"

struct lip_in_s
{
	size_t (*read)(void* buff, size_t size, lip_in_t* input);
};

struct lip_out_s
{
	size_t (*write)(const void* buff, size_t size, lip_out_t* output);
};

struct lip_sstream_s
{
	lip_in_t vtable;
	lip_string_ref_t str;
	size_t pos;
};

struct lip_ofstream_s
{
	lip_out_t vtable;
	FILE* file;
};

struct lip_ifstream_s
{
	lip_in_t vtable;
	FILE* file;
};

lip_in_t*
lip_stdin(void);

lip_out_t*
lip_stdout(void);

lip_out_t*
lip_stderr(void);

LIP_MAYBE_UNUSED static inline size_t
lip_read(void* buff, size_t size, lip_in_t* input)
{
	return input->read(buff, size, input);
}

LIP_MAYBE_UNUSED static inline size_t
lip_write(const void* buff, size_t size, lip_out_t* output)
{
	return output->write(buff, size, output);
}

#if defined(__GNUC__) || defined(__GNUG__) || defined(__clang__)
__attribute__((format(printf, 2, 3)))
#endif
size_t
lip_printf(lip_out_t* output, const char* format, ...);

size_t
lip_vprintf(lip_out_t* output, const char* format, va_list args);

lip_in_t*
lip_make_sstream(lip_string_ref_t str, struct lip_sstream_s* sstream);

lip_out_t*
lip_make_ofstream(FILE* file, struct lip_ofstream_s* ofstream);

lip_in_t*
lip_make_ifstream(FILE* file, struct lip_ifstream_s* ifstream);

#endif
