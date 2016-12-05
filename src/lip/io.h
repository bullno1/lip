#ifndef LIP_IO_H
#define LIP_IO_H

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
lip_stdin();

lip_out_t*
lip_stdout();

lip_out_t*
lip_stderr();

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
__attribute__((format(printf, 3, 4)))
#endif
void
lip_printf(lip_allocator_t* allocator, lip_out_t* output, const char* format, ...);

lip_in_t*
lip_make_sstream(lip_string_ref_t str, struct lip_sstream_s* sstream);

lip_out_t*
lip_make_ofstream(FILE* file, struct lip_ofstream_s* ofstream);

lip_in_t*
lip_make_ifstream(FILE* file, struct lip_ifstream_s* ifstream);

#endif
