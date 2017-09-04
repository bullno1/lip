#include <lip/std/io.h>
#include <lip/core/memory.h>
#include <errno.h>

static struct lip_ofstream_s lip_stdout_ofstream;
static struct lip_ofstream_s lip_stderr_ofstream;
static struct lip_ifstream_s lip_stdin_ifstream;

struct lip_std_fs_s
{
	lip_fs_t vtable;
	lip_allocator_t* allocator;
};

static size_t
lip_ifstream_read(void* buff, size_t size, lip_in_t* vtable)
{
	struct lip_ifstream_s* ifstream =
		LIP_CONTAINER_OF(vtable, struct lip_ifstream_s, vtable);
	return fread(buff, 1, size, ifstream->file);
}

static size_t
lip_ofstream_write(const void* buff, size_t size, lip_out_t* vtable)
{
	struct lip_ofstream_s* ofstream =
		LIP_CONTAINER_OF(vtable, struct lip_ofstream_s, vtable);
	return fwrite(buff, 1, size, ofstream->file);
}

static lip_in_t*
lip_std_fs_begin_read(lip_fs_t* vtable, lip_string_ref_t path)
{
	FILE* file = fopen(path.ptr, "rb");
	if(file == NULL) { return NULL; }

	struct lip_std_fs_s* fs =
		LIP_CONTAINER_OF(vtable, struct lip_std_fs_s, vtable);
	struct lip_ifstream_s* ifstream = lip_new(fs->allocator, struct lip_ifstream_s);
	return lip_make_ifstream(file, ifstream);
}

static lip_out_t*
lip_std_fs_begin_write(lip_fs_t* vtable, lip_string_ref_t path)
{
	FILE* file = fopen(path.ptr, "wb");
	if(file == NULL) { return NULL; }

	struct lip_std_fs_s* fs =
		LIP_CONTAINER_OF(vtable, struct lip_std_fs_s, vtable);
	struct lip_ofstream_s* ofstream = lip_new(fs->allocator, struct lip_ofstream_s);
	return lip_make_ofstream(file, ofstream);
}

static void
lip_std_fs_end_read(lip_fs_t* vtable, lip_in_t* file)
{
	struct lip_std_fs_s* fs =
		LIP_CONTAINER_OF(vtable, struct lip_std_fs_s, vtable);
	struct lip_ifstream_s* ifstream =
		LIP_CONTAINER_OF(file, struct lip_ifstream_s, vtable);
	fclose(ifstream->file);
	lip_free(fs->allocator, ifstream);
}

static void
lip_std_fs_end_write(lip_fs_t* vtable, lip_out_t* file)
{
	struct lip_std_fs_s* fs =
		LIP_CONTAINER_OF(vtable, struct lip_std_fs_s, vtable);
	struct lip_ofstream_s* ofstream =
		LIP_CONTAINER_OF(file, struct lip_ofstream_s, vtable);
	fclose(ofstream->file);
	lip_free(fs->allocator, ofstream);
}

static lip_string_ref_t
lip_std_fs_last_error(lip_fs_t* vtable)
{
	(void)vtable;
	return lip_string_ref(strerror(errno));
}

lip_fs_t*
lip_create_std_fs(lip_allocator_t* allocator)
{
	struct lip_std_fs_s* fs = lip_new(allocator, struct lip_std_fs_s);
	*fs = (struct lip_std_fs_s){
		.allocator = allocator,
		.vtable = {
			.begin_read = lip_std_fs_begin_read,
			.end_read = lip_std_fs_end_read,
			.begin_write = lip_std_fs_begin_write,
			.end_write = lip_std_fs_end_write,
			.last_error = lip_std_fs_last_error
		}
	};
	return &fs->vtable;
}

void
lip_destroy_std_fs(lip_fs_t* vtable)
{
	struct lip_std_fs_s* fs =
		LIP_CONTAINER_OF(vtable, struct lip_std_fs_s, vtable);
	lip_free(fs->allocator, fs);
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
