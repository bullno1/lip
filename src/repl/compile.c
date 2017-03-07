#include "compile.h"
#include <errno.h>
#include <string.h>
#include <lip/io.h>
#include <lip/memory.h>
#include <lip/array.h>
#ifdef __linux__
#include <sys/stat.h>
#endif

struct lip_mzfs_s
{
	lip_fs_t vtable;
	lip_allocator_t* allocator;
	mz_zip_archive* archive;
	lip_string_ref_t last_error;
};

void
repl_init_compile_opts(cargo_t cargo, struct repl_compile_opts_s* opts)
{
	memset(opts, 0, sizeof(*opts));

	cargo_add_group(cargo, 0, "compile", "Compile mode", NULL);
	cargo_add_option(
		cargo, 0,
		"<!mode, compile> --compile -c", "Write bytecode to OUTPUT", "s",
		&opts->output_filename
	);
	cargo_add_option(
		cargo, 0,
		"<compile> --standalone -s", "Make compiled output a standalone executable", "b",
		&opts->standalone
	);
	cargo_set_metavar(cargo, "--compile", "OUTPUT");
}

void
repl_cleanup_compile_opts(struct repl_compile_opts_s* opts)
{
	free(opts->output_filename);
}

bool
repl_compile_mode_activated(struct repl_compile_opts_s* opts)
{
	return opts->output_filename || opts->standalone;
}

static int
repl_compile_standalone(
	lip_script_t* script,
	struct repl_common_s* common,
	struct repl_compile_opts_s* opts
)
{
	int exit_code = EXIT_FAILURE;
	lip_array(char) bytecode_buf =
		lip_array_create(lip_default_allocator, char, 0);
	FILE* lip = NULL;
	FILE* out = NULL;
	bool ended_archive = true;
#define quit(code) exit_code = code; goto quit;
#define repl_assert(cond, ...) \
	do { \
		if(!(cond)) { \
			lip_printf(lip_stderr(), __VA_ARGS__); \
			quit(EXIT_FAILURE); \
		} \
	} while(0)

	struct lip_osstream_s osstream;
	bool result = lip_dump_script(
		common->context,
		script,
		lip_string_ref(opts->output_filename),
		lip_make_osstream(&bytecode_buf, &osstream)
	);
	if(!result)
	{
		lip_print_error(lip_stderr(), common->context);
		quit(EXIT_FAILURE);
	}

	repl_assert(
		lip = fopen(common->argv[0], "rb"),
		"Could not open %s: %s\n", common->argv[0], strerror(errno)
	);
	fseek(lip, 0, SEEK_END);
	long loader_size = ftell(lip);
	fseek(lip, 0, SEEK_SET);

	mz_zip_archive archive;
	memset(&archive, 0, sizeof(archive));
	repl_assert(
		mz_zip_writer_init_file(&archive, opts->output_filename, loader_size),
		"Could not create archive\n"
	);
	ended_archive = false;
	repl_assert(
		mz_zip_writer_add_mem(
			&archive, "main.lipc", bytecode_buf, lip_array_len(bytecode_buf),
			MZ_DEFAULT_COMPRESSION
		),
		"Could not create archive\n"
	);
	repl_assert(
		mz_zip_writer_finalize_archive(&archive),
		"Could not finalize archive\n"
	);
	mz_zip_writer_end(&archive);
	ended_archive = true;

	repl_assert(
		out = fopen(opts->output_filename, "r+b"),
		"Could not open %s: %s\n", opts->output_filename, strerror(errno)
	);
	while(!feof(lip) && !ferror(lip))
	{
		char read_buf[4096];
		size_t bytes_read = fread(read_buf, 1, sizeof(read_buf), lip);
		repl_assert(
			fwrite(read_buf, 1, bytes_read, out) == bytes_read,
			"IO error\n"
		);
	}

#ifdef __linux__
	repl_assert(
		chmod(opts->output_filename, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH ) == 0,
		"Could not chmod %s: %s\n", opts->output_filename, strerror(errno)
	);
	quit(EXIT_SUCCESS);
#endif

quit:
	if(!ended_archive) { mz_zip_writer_end(&archive); }
	lip_array_destroy(bytecode_buf);
	if(lip) { fclose(lip); }
	if(out) { fclose(out); }
	return exit_code;
}

int
repl_compile(struct repl_common_s* common, struct repl_compile_opts_s* opts)
{
	lip_in_t* input = NULL;
	lip_string_ref_t source_name;
	if(common->script_filename == NULL)
	{
		input = lip_stdin();
		source_name = lip_string_ref("<stdin>");
	}
	else
	{
		source_name = lip_string_ref(common->script_filename);
	}

	lip_script_t* script = lip_load_script(common->context, source_name, input, false);
	if(!script)
	{
		lip_print_error(lip_stderr(), common->context);
		return EXIT_FAILURE;
	}

	if(!opts->standalone)
	{
		bool result = lip_dump_script(
			common->context, script, lip_string_ref(opts->output_filename), NULL
		);
		if(!result) { lip_print_error(lip_stderr(), common->context); }
		return result ? EXIT_SUCCESS : EXIT_FAILURE;
	}
	else
	{
		return repl_compile_standalone(script, common, opts);
	}
}

static lip_in_t*
lip_mzfs_begin_read(lip_fs_t* vtable, lip_string_ref_t path)
{
	struct lip_mzfs_s* fs =
		LIP_CONTAINER_OF(vtable, struct lip_mzfs_s, vtable);

	mz_uint flags = MZ_ZIP_FLAG_CASE_SENSITIVE;
	int file_index =
		mz_zip_reader_locate_file(fs->archive, path.ptr, NULL, flags);

	if(file_index < 0)
	{
		fs->last_error = lip_string_ref("File not found");
		return NULL;
	}

	mz_zip_archive_file_stat stat;
	if(!mz_zip_reader_file_stat(fs->archive, file_index, &stat))
	{
		fs->last_error = lip_string_ref("Corrupted archive");
		return NULL;
	}

	void* mem = lip_malloc(fs->allocator, stat.m_uncomp_size);
	char readbuf[4096];
	mz_bool status = mz_zip_reader_extract_to_mem_no_alloc(
		fs->archive, file_index, mem, stat.m_uncomp_size, 0, readbuf, sizeof(readbuf)
	);
	if(!status)
	{
		fs->last_error = lip_string_ref("Corrupted archive");
		lip_free(fs->allocator, mem);
		return NULL;
	}

	struct lip_isstream_s* isstream = lip_new(fs->allocator, struct lip_isstream_s);
	lip_string_ref_t mem_ref = { .length = stat.m_uncomp_size, .ptr = mem };
	return lip_make_isstream(mem_ref, isstream);
}

static void
lip_mzfs_end_read(lip_fs_t* vtable, lip_in_t* file)
{
	struct lip_mzfs_s* fs =
		LIP_CONTAINER_OF(vtable, struct lip_mzfs_s, vtable);
	struct lip_isstream_s* isstream =
		LIP_CONTAINER_OF(file, struct lip_isstream_s, vtable);
	lip_free(fs->allocator, (void*)isstream->str.ptr);
	lip_free(fs->allocator, isstream);
}

static lip_out_t*
lip_mzfs_begin_write(lip_fs_t* vtable, lip_string_ref_t path)
{
	(void)path;
	struct lip_mzfs_s* fs =
		LIP_CONTAINER_OF(vtable, struct lip_mzfs_s, vtable);
	fs->last_error = lip_string_ref("Writing is not supported");
	return NULL;
}

static void
lip_mzfs_end_write(lip_fs_t* vtable, lip_out_t* file)
{
	(void)vtable;
	(void)file;
}

static lip_string_ref_t
lip_mzfs_last_error(lip_fs_t* vtable)
{
	struct lip_mzfs_s* fs =
		LIP_CONTAINER_OF(vtable, struct lip_mzfs_s, vtable);
	return fs->last_error;
}

static void
lip_init_mzfs(
	struct lip_mzfs_s* mzfs,
	lip_allocator_t* allocator,
	mz_zip_archive* archive
)
{
	*mzfs = (struct lip_mzfs_s){
		.allocator = allocator,
		.archive = archive,
		.vtable = {
			.begin_read = lip_mzfs_begin_read,
			.end_read = lip_mzfs_end_read,
			.begin_write = lip_mzfs_begin_write,
			.end_write = lip_mzfs_end_write,
			.last_error = lip_mzfs_last_error
		}
	};
}

int
repl_compiled_script_entry(mz_zip_archive* archive, int argc, char* argv[])
{
	(void)argc;
	(void)argv;

	struct lip_mzfs_s mzfs;
	lip_init_mzfs(&mzfs, lip_default_allocator, archive);

	lip_runtime_config_t cfg;
	lip_reset_runtime_config(&cfg);
	cfg.fs = &mzfs.vtable;
	lip_runtime_t* runtime = lip_create_runtime(&cfg);
	lip_context_t* context = lip_create_context(runtime, NULL);
	lip_vm_t* vm = lip_create_vm(context, NULL);
	lip_load_builtins(context);

	bool status = repl_run_script(context, vm, lip_string_ref("main.lipc"), NULL);
	lip_destroy_runtime(runtime);
	return status ? EXIT_SUCCESS : EXIT_FAILURE;
}
