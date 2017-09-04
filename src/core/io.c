#include <lip/core/io.h>
#include <stdarg.h>
#include <lip/core/array.h>
#include "vendor/format/format.h"

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
	*buffer = lip_array__prepare_push(*buffer);
	lip_array_resize(*buffer, len + size);
	memcpy(*buffer + len, buff, size);
	return size;
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
