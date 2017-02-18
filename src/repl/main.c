#ifdef __linux__
#define _XOPEN_SOURCE
#endif

#ifndef _WIN32
#define LIP_HAS_LINENOISE
#endif

#include <stdlib.h>
#include <lip/lip.h>
#include <lip/config.h>
#include <lip/memory.h>
#include <lip/io.h>
#include <lip/print.h>
#ifdef LIP_HAS_LINENOISE
#	include <linenoise.h>
#	include <encodings/utf8.h>
#endif
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
#ifdef LIP_HAS_LINENOISE
			line = linenoise("lip> ");
#else
			printf("lip> ");
			char buff[1024];
			line = fgets(buff, sizeof(buff) - 1, stdin);
#endif

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

#ifdef LIP_HAS_LINENOISE
		linenoiseHistoryAdd(line);
#endif

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
#ifdef LIP_HAS_LINENOISE
		free(line);
#endif
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
	int inspect = false;
	int interactive = false;
	int print_depth = 10;
	char* exec_string = NULL;
	char* script_filename = NULL;
	char* bytecode_output_file = NULL;

	cargo_add_option(
		cargo, 0,
		"--version -v", "Show version information", "b",
		&show_version
	);

	cargo_add_mutex_group(cargo, 0, "mode", "Operation mode", NULL);

	cargo_add_group(cargo, 0, "run", "Run mode (default)", NULL);
	cargo_add_option(
		cargo, 0,
		"<run> --interactive -i", "Enter interactive mode after executing script", "b",
		&interactive
	);
	cargo_add_option(
		cargo, 0,
		"<run> --execute -e", "Execute STRING", "s",
		&exec_string
	);
	cargo_set_metavar(cargo, "--execute", "STRING");

	cargo_add_group(cargo, 0, "compile", "Compile mode", NULL);
	cargo_add_option(
		cargo, 0,
		"<!mode, compile> --compile -c", "Write bytecode to OUTPUT", "s",
		&bytecode_output_file
	);
	cargo_set_metavar(cargo, "--compile", "OUTPUT");

	cargo_add_group(cargo, 0, "inspect", "Inspect mode", NULL);
	cargo_add_option(
		cargo, 0,
		"<!mode, inspect> --inspect", "Print informations about script", "b",
		&inspect
	);
	cargo_add_option(
		cargo, 0,
		"<inspect> --depth -d", "Print depth", "i",
		&print_depth
	);

	cargo_add_option(
		cargo, CARGO_OPT_NOT_REQUIRED | CARGO_OPT_STOP,
		"script", "Script file to compile/execute/inspect", "s",
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

	lip_runtime_config_t cfg;
	lip_reset_runtime_config(&cfg);
	runtime = lip_create_runtime(&cfg);
	lip_context_t* ctx = lip_create_context(runtime, lip_default_allocator);

	int num_modes = 0;
	if(bytecode_output_file != NULL) { ++num_modes; }
	if(inspect) { ++num_modes; }
	if(interactive || exec_string) { ++num_modes; }

	if(num_modes > 1)
	{
		lip_printf(
			lip_stderr(),
			"Cannot operate in multiple modes at the same time.\n"
		);
		quit(EXIT_FAILURE);
	}

	// Compile
	if(bytecode_output_file != NULL)
	{
		lip_in_t* input = NULL;
		lip_string_ref_t source_name;
		if(script_filename == NULL)
		{
			input = lip_stdin();
			source_name = lip_string_ref("<stdin>");
		}
		else
		{
			source_name = lip_string_ref(script_filename);
		}

		lip_script_t* script = lip_load_script(ctx, source_name, input);
		if(!script)
		{
			print_error(ctx);
			quit(EXIT_FAILURE);
		}

		bool result = lip_dump_script(
			ctx, script, lip_string_ref(bytecode_output_file), NULL
		);
		if(!result) { print_error(ctx); }
		quit(result ? EXIT_SUCCESS : EXIT_FAILURE);
	}

	// Inspect
	if(inspect)
	{
		lip_in_t* input = NULL;
		lip_string_ref_t source_name;
		if(script_filename == NULL)
		{
			input = lip_stdin();
			source_name = lip_string_ref("<stdin>");
		}
		else
		{
			source_name = lip_string_ref(script_filename);
		}

		lip_script_t* script = lip_load_script(ctx, source_name, input);
		if(!script)
		{
			print_error(ctx);
			quit(EXIT_FAILURE);
		}

		lip_print_closure(print_depth, 0, lip_stdout(), (lip_closure_t*)script);

		quit(EXIT_SUCCESS);
	}

	lip_load_builtins(ctx);

	lip_vm_t* vm = lip_create_vm(ctx, NULL);

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
		bool status = run_script(ctx, vm, lip_string_ref(script_filename), NULL);
		if(!status)	{ quit(EXIT_FAILURE); }
	}

	if(script_filename && !interactive) { quit(EXIT_SUCCESS); }

	if(isatty(fileno(stdin)) || interactive)
	{
#ifdef LIP_HAS_LINENOISE
		linenoiseSetMultiLine(1);
		linenoiseSetEncodingFunctions(
			linenoiseUtf8PrevCharLen,
			linenoiseUtf8NextCharLen,
			linenoiseUtf8ReadCode
		);
#endif

		struct repl_context_s repl_context = {
			.read_pos = 1,
			.ctx = ctx,
			.vm = vm,
			.vtable = { .read = repl_read, .print = repl_print }
		};
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
	free(bytecode_output_file);
	if(runtime) { lip_destroy_runtime(runtime); }

	return exit_code;
}
