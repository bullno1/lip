#include "io.h"

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

static lip_in_t lip_sstream_vtable = { lip_sstream_read };

lip_in_t*
lip_make_sstream(lip_string_ref_t str, struct lip_sstream_s* sstream)
{
	sstream->str = str;
	sstream->pos = 0;
	sstream->vtable = lip_sstream_vtable;

	return &sstream->vtable;
}
