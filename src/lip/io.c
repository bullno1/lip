#include "io.h"
#include <stdarg.h>
#include <stdio.h>
#include "memory.h"

struct lip_ofstream_s lip_stdout_ofstream;
struct lip_ofstream_s lip_stderr_ofstream;

static size_t
lip_ofstream_write(const void* buff, size_t size, lip_out_t* vtable)
{
	struct lip_ofstream_s* ofstream =
		LIP_CONTAINER_OF(vtable, struct lip_ofstream_s, vtable);
	return fwrite(buff, size, 1, ofstream->file);
}

static size_t
lip_sstream_read(void* buff, size_t size, lip_in_t* vtable)
{
	struct lip_sstream_s* sstream =
		LIP_CONTAINER_OF(vtable, struct lip_sstream_s, vtable);

	size_t bytes_read = LIP_MIN(sstream->str.length - sstream->pos, size);
	memcpy(buff, sstream->str.ptr + sstream->pos, bytes_read);
	sstream->pos += bytes_read;
	return bytes_read;
}

lip_in_t*
lip_make_sstream(lip_string_ref_t str, struct lip_sstream_s* sstream)
{
	sstream->str = str;
	sstream->pos = 0;
	sstream->vtable.read = lip_sstream_read;

	return &sstream->vtable;
}

void
lip_printf(lip_allocator_t* allocator, lip_out_t* output, const char* format, ...)
{
	char print_buff[256];

	va_list args1, args2;
	va_start(args1, format);
	va_copy(args2, args1);

	int needed_size = vsnprintf(print_buff, sizeof(print_buff), format, args1);
	char* str;
	if((size_t)needed_size < sizeof(print_buff))
	{
		str = print_buff;
	}
	else
	{
		str = lip_malloc(allocator, needed_size);
		vsnprintf(str, needed_size, format, args2);
	}

	va_end(args2);
	va_end(args1);

	lip_write(str, needed_size, output);

	if((size_t)needed_size >= sizeof(print_buff))
	{
		lip_free(allocator, str);
	}
}

lip_out_t*
lip_make_ofstream(FILE* file, struct lip_ofstream_s* ofstream)
{
	ofstream->file = file;
	ofstream->vtable.write = lip_ofstream_write;

	return &ofstream->vtable;
}

lip_out_t*
lip_stdout()
{
	static lip_out_t* out = NULL;
	if(!out)
	{
		out = lip_make_ofstream(stdout, &lip_stdout_ofstream);
	}

	return out;
}

lip_out_t*
lip_stderr()
{
	static lip_out_t* out = NULL;
	if(!out)
	{
		out = lip_make_ofstream(stderr, &lip_stdout_ofstream);
	}

	return out;
}
