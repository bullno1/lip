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

struct lip_isstream_s
{
	lip_in_t vtable;
	lip_string_ref_t str;
	size_t pos;
};

struct lip_osstream_s
{
	lip_out_t vtable;
	lip_array(char)* buffer;
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

LIP_API lip_in_t*
lip_stdin(void);

LIP_API lip_out_t*
lip_stdout(void);

LIP_API lip_out_t*
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

LIP_API LIP_PRINTF_LIKE(2, 3) size_t
lip_printf(lip_out_t* output, const char* format, ...);

LIP_API size_t
lip_vprintf(lip_out_t* output, const char* format, va_list args);

LIP_API lip_in_t*
lip_make_isstream(lip_string_ref_t str, struct lip_isstream_s* sstream);

LIP_API lip_out_t*
lip_make_osstream(lip_array(char)* buffer, struct lip_osstream_s* sstream);

LIP_API lip_out_t*
lip_make_ofstream(FILE* file, struct lip_ofstream_s* ofstream);

LIP_API lip_in_t*
lip_make_ifstream(FILE* file, struct lip_ifstream_s* ifstream);

#endif
