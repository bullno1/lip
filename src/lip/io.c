#include <lip/io.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <lip/memory.h>
#include <lip/array.h>
#include "vendor/format/format.h"

static struct lip_ofstream_s lip_stdout_ofstream;
static struct lip_ofstream_s lip_stderr_ofstream;
static struct lip_ifstream_s lip_stdin_ifstream;


struct lip_native_fs_s
{
	lip_fs_t vtable;
	lip_allocator_t* allocator;
};

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
lip_isstream_read(void* buff, size_t size, lip_in_t* vtable)
{
	struct lip_isstream_s* sstream =
		LIP_CONTAINER_OF(vtable, struct lip_isstream_s, vtable);

	size_t bytes_read = LIP_MIN(sstream->str.length - sstream->pos, size);
	memcpy(buff, sstream->str.ptr + sstream->pos, bytes_read);
	sstream->pos += bytes_read;
	return bytes_read;
}

static size_t
lip_osstream_write(const void* buff, size_t size, lip_out_t* vtable)
{
	struct lip_osstream_s* osstream =
		LIP_CONTAINER_OF(vtable, struct lip_osstream_s, vtable);

	lip_array(char)* buffer = osstream->buffer;
	size_t len = lip_array_len(*buffer);
	lip_array__prepare_push(*buffer);
	lip_array_resize(*buffer, len + size);
	memcpy(*buffer + len, buff, size);
	return size;
}

static lip_in_t*
lip_native_fs_begin_read(lip_fs_t* vtable, lip_string_ref_t path)
{
	FILE* file = fopen(path.ptr, "rb");
	if(file == NULL) { return NULL; }

	struct lip_native_fs_s* fs =
		LIP_CONTAINER_OF(vtable, struct lip_native_fs_s, vtable);
	struct lip_ifstream_s* ifstream = lip_new(fs->allocator, struct lip_ifstream_s);
	return lip_make_ifstream(file, ifstream);
}

static void
lip_native_fs_close(lip_fs_t* vtable, lip_in_t* file)
{
	struct lip_native_fs_s* fs =
		LIP_CONTAINER_OF(vtable, struct lip_native_fs_s, vtable);
	struct lip_ifstream_s* ifstream =
		LIP_CONTAINER_OF(file, struct lip_ifstream_s, vtable);
	fclose(ifstream->file);
	lip_free(fs->allocator, ifstream);
}

static lip_string_ref_t
lip_native_fs_last_error(lip_fs_t* vtable)
{
	(void)vtable;
	return lip_string_ref(strerror(errno));
}

lip_fs_t*
lip_create_native_fs(lip_allocator_t* allocator)
{
	struct lip_native_fs_s* fs = lip_new(allocator, struct lip_native_fs_s);
	*fs = (struct lip_native_fs_s){
		.allocator = allocator,
		.vtable = {
			.begin_read = lip_native_fs_begin_read,
			.end_read = lip_native_fs_close,
			.last_error = lip_native_fs_last_error
		}
	};
	return &fs->vtable;
}

LIP_API void
lip_destroy_native_fs(lip_fs_t* vtable)
{
	struct lip_native_fs_s* fs =
		LIP_CONTAINER_OF(vtable, struct lip_native_fs_s, vtable);
	lip_free(fs->allocator, fs);
}

lip_in_t*
lip_make_isstream(lip_string_ref_t str, struct lip_isstream_s* sstream)
{
	sstream->str = str;
	sstream->pos = 0;
	sstream->vtable.read = lip_isstream_read;

	return &sstream->vtable;
}

lip_out_t*
lip_make_osstream(lip_array(char)* buffer, struct lip_osstream_s* sstream)
{
	sstream->buffer = buffer;
	sstream->vtable.write = lip_osstream_write;

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
