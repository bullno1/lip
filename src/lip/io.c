#include <lip/io.h>
#include <stdarg.h>
#include <stdio.h>
#include <lip/memory.h>
#include "vendor/format/format.h"

static struct lip_ofstream_s lip_stdout_ofstream;
static struct lip_ofstream_s lip_stderr_ofstream;
static struct lip_ifstream_s lip_stdin_ifstream;


static size_t
lip_ofstream_write(const void* buff, size_t size, lip_out_t* vtable)
{
	struct lip_ofstream_s* ofstream =
		LIP_CONTAINER_OF(vtable, struct lip_ofstream_s, vtable);
	return fwrite(buff, size, 1, ofstream->file);
}

static size_t
lip_ifstream_read(void* buff, size_t size, lip_in_t* vtable)
{
	struct lip_ifstream_s* ifstream =
		LIP_CONTAINER_OF(vtable, struct lip_ifstream_s, vtable);
	return fread(buff, size, 1, ifstream->file);
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

size_t
lip_printf(lip_out_t* output, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	size_t ret = lip_vprintf(output, format, args);
	va_end(args);
	return ret;
}

static void*
lip_print_cons(void* out, const char* str, size_t size)
{
	return lip_write(str, size, out) > 0 ? out : NULL;
}

size_t
lip_vprintf(lip_out_t* output, const char* format, va_list va)
{
	int ret = lip_format(lip_print_cons, output, format, va);
	return ret >= 0 ? ret : 0;
}

lip_out_t*
lip_make_ofstream(FILE* file, struct lip_ofstream_s* ofstream)
{
	ofstream->file = file;
	ofstream->vtable.write = lip_ofstream_write;

	return &ofstream->vtable;
}

lip_in_t*
lip_make_ifstream(FILE* file, struct lip_ifstream_s* ifstream)
{
	ifstream->file = file;
	ifstream->vtable.read = lip_ifstream_read;

	return &ifstream->vtable;
}

lip_in_t*
lip_stdin(void)
{
	static lip_in_t* in = NULL;
	if(!in)
	{
		in = lip_make_ifstream(stdin, &lip_stdin_ifstream);
	}

	return in;
}

lip_out_t*
lip_stdout(void)
{
	static lip_out_t* out = NULL;
	if(!out)
	{
		out = lip_make_ofstream(stdout, &lip_stdout_ofstream);
	}

	return out;
}

lip_out_t*
lip_stderr(void)
{
	static lip_out_t* out = NULL;
	if(!out)
	{
		out = lip_make_ofstream(stderr, &lip_stderr_ofstream);
	}

	return out;
}
