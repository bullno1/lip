#ifdef __linux__
#define _XOPEN_SOURCE
#endif

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <lip/lip.h>
#include <lip/config.h>
#include <lip/memory.h>
#include <lip/io.h>
#include <lip/print.h>
#include <linenoise.h>
#include <cargo.h>

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

#define quit(code) exit_code = code; goto quit;

typedef struct repl_context_s repl_context_t;
typedef struct args_s args_t;
typedef struct program_state_s program_state_t;

struct repl_context_s
{
	lip_repl_handler_t vtable;
	lip_context_t* ctx;
	lip_vm_t* vm;
	char* line_buff;
	size_t read_pos;
	size_t buff_len;
};

static size_t
repl_read(lip_repl_handler_t* vtable, void* buff, size_t size)
{
	struct repl_context_s* repl = LIP_CONTAINER_OF(vtable, struct repl_context_s, vtable);

	if(repl->read_pos >= repl->buff_len)
	{
		char* line;
		for(;;)
		{
			line = linenoise("lip> ");

			if(line == 0)
			{
				return 0;
			}
			else if(line[0] == '\0')
			{
				free(line);
			}
			else
			{
				break;
			}
		}

		linenoiseHistoryAdd(line);

		size_t line_length = strlen(line);
		if(line_length + 1 > repl->buff_len)
		{
			repl->line_buff = lip_realloc(
				lip_default_allocator, repl->line_buff, line_length + 1
			);
		}
		memcpy(repl->line_buff, line, line_length);
		repl->line_buff[line_length] = '\n';
		repl->read_pos = 0;
		repl->buff_len = line_length + 1;
		free(line);
	}

	size_t num_bytes_to_read = LIP_MIN(repl->buff_len - repl->read_pos, size);
	memcpy(buff, &repl->line_buff[repl->read_pos], num_bytes_to_read);
	repl->read_pos += num_bytes_to_read;
	return num_bytes_to_read;
}

static void
print_error(lip_context_t* ctx)
{
	const lip_context_error_t* err = lip_get_error(ctx);
	lip_printf(
		lip_stderr(),
		"Error: %.*s.\n",
		(int)err->message.length, err->message.ptr
	);
	for(unsigned int i = 0; i < err->num_records; ++i)
	{
		lip_error_record_t* record = &err->records[i];
		lip_printf(
			lip_stderr(),
			"  %.*s:%u:%u - %u:%u: %.*s.\n",
			(int)record->filename.length, record->filename.ptr,
			record->location.start.line,
			record->location.start.column,
			record->location.end.line,
			record->location.end.column,
			(int)record->message.length, record->message.ptr
		);
	}
}

static void
repl_print(lip_repl_handler_t* vtable, lip_exec_status_t status, lip_value_t result)
{
	repl_context_t* repl = LIP_CONTAINER_OF(vtable, repl_context_t, vtable);
	switch(status)
	{
	case LIP_EXEC_OK:
		if(result.type != LIP_VAL_NIL)
		{
			lip_print_value(5, 0, lip_stdout(), result);
		}
		break;
	case LIP_EXEC_ERROR:
		{
			const lip_context_error_t* err = lip_get_error(repl->ctx);
			if(err->num_records == 0)
			{
				lip_traceback(repl->ctx, repl->vm, result);
			}
			print_error(repl->ctx);
		}
		break;
	}
}

static bool
run_script(
	lip_context_t* ctx,
	lip_vm_t* vm,
	lip_string_ref_t filename,
	lip_in_t* input
)
{
	lip_script_t* script = lip_load_script(ctx, filename, input);
	if(!script)
	{
		print_error(ctx);
		return false;
	}

	lip_value_t result;
	if(lip_exec_script(vm, script, &result) != LIP_EXEC_OK)
	{
		lip_traceback(ctx, vm, result);
		print_error(ctx);
		return false;
	}
	else
	{
		return true;
	}
}

int
main(int argc, char* argv[])
{
	lip_runtime_t* runtime = NULL;
	int exit_code = EXIT_FAILURE;
	cargo_t cargo;
	if(cargo_init(&cargo, 0, argv[0]))
	{
		return EXIT_FAILURE;
	}

	int show_version = false;
	int interactive = false;
	char* exec_string = NULL;
	char* script_filename = NULL;
	cargo_add_option(
		cargo, 0,
		"--version -v", "Show version information", "b",
		&show_version
	);
	cargo_add_option(
		cargo, 0,
		"--interactive -i", "Enter interactive mode after executing script", "b",
		&interactive
	);
	cargo_add_option(
		cargo, 0,
		"--execute -e", "Execute STRING", "s",
		&exec_string
	);
	cargo_set_metavar(cargo, "--execute", "STRING");
	cargo_add_option(
		cargo, CARGO_OPT_NOT_REQUIRED | CARGO_OPT_STOP,
		"script", "Script to execute", "s",
		&script_filename
	);
	cargo_set_metavar(cargo, "script", "script");

	cargo_parse_result_t parse_result = cargo_parse(cargo, 0, 1, argc, argv);
	cargo_destroy(&cargo);
	switch(parse_result)
	{
		case CARGO_PARSE_OK:
			break;
		case CARGO_PARSE_SHOW_HELP:
			quit(EXIT_SUCCESS);
		default:
			quit(EXIT_FAILURE);
	}

	if(show_version)
	{
		lip_printf(
			lip_stdout(),
			"lip %s [threading-api:%s]\n"
			"\n"
			"Compiler: %s\n"
			"Linker: %s\n"
			"Compile flags: %s\n"
			"Link flags: %s\n"
			"\n",
			LIP_VERSION, LIP_THREADING_API,
			LIP_COMPILER, LIP_LINKER,
			LIP_COMPILE_FLAGS, LIP_LINK_FLAGS
		);
	}

	if(show_version && !(interactive || exec_string)) { quit(EXIT_SUCCESS); }

	runtime = lip_create_runtime(lip_default_allocator);
	lip_context_t* ctx = lip_create_context(runtime, lip_default_allocator);
	lip_load_builtins(ctx);

	lip_vm_config_t vm_config = {
		.os_len = 256,
		.cs_len = 256,
		.env_len = 256
	};
	lip_vm_t* vm = lip_create_vm(ctx, &vm_config);

	if(exec_string)
	{
		lip_string_ref_t cmdline_str_ref = lip_string_ref(exec_string);
		struct lip_isstream_s sstream;

		lip_in_t* input = lip_make_isstream(cmdline_str_ref, &sstream);

		if(!run_script(ctx, vm, lip_string_ref("<cmdline>"), input))
		{
			quit(EXIT_FAILURE);
		}
	}

	if(exec_string && !(interactive || script_filename)) { quit(EXIT_SUCCESS); }

	if(script_filename)
	{
		FILE* file = fopen(script_filename, "rb");
		if(!file)
		{
			lip_printf(
				lip_stderr(), "lip: cannot open %s: %s\n",
				script_filename, strerror(errno)
			);
			quit(EXIT_FAILURE);
		}

		struct lip_ifstream_s ifstream;
		lip_in_t* input = lip_make_ifstream(file, &ifstream);

		bool status = run_script(ctx, vm, lip_string_ref(script_filename), input);
		fclose(file);

		if(!status)	{ quit(EXIT_FAILURE); }
	}

	if(script_filename && !interactive) { quit(EXIT_SUCCESS); }

	if(isatty(fileno(stdin)) || interactive)
	{
		struct repl_context_s repl_context = {
			.read_pos = 1,
			.ctx = ctx,
			.vm = vm,
			.vtable = { .read = repl_read, .print = repl_print }
		};
		linenoiseInstallWindowChangeHandler();
		lip_repl(vm, lip_string_ref("<stdin>"), &repl_context.vtable);
		lip_free(lip_default_allocator, repl_context.line_buff);
		quit(EXIT_SUCCESS);
	}
	else
	{
		bool status = run_script(ctx, vm, lip_string_ref("<stdin>"), lip_stdin());
		quit(status ? EXIT_SUCCESS : EXIT_FAILURE);
	}

quit:
	free(exec_string);
	free(script_filename);
	if(runtime) { lip_destroy_runtime(runtime); }

	return exit_code;
}
