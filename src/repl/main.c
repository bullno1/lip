#ifdef __linux__
#define _XOPEN_SOURCE
#endif

#include <stdlib.h>
#include <stdio.h>
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

typedef struct repl_context_s repl_context_t;
typedef struct program_state_s program_state_t;

struct repl_context_s
{
	lip_repl_handler_t vtable;
	lip_context_t* ctx;
	char* line_buff;
	size_t read_pos;
	size_t buff_len;
};

struct program_state_s
{
	lip_runtime_t* runtime;
	lip_context_t* context;
	lip_vm_config_t vm_config;
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
		lip_stdout(),
		"Error: %.*s.\n",
		(int)err->message.length, err->message.ptr
	);
	for(unsigned int i = 0; i < err->num_records; ++i)
	{
		lip_error_record_t* record = &err->records[i];
		lip_printf(
			lip_stdout(),
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
		print_error(repl->ctx);
		break;
	}
}

static int
repl(program_state_t* prog)
{
	lip_vm_t* vm = lip_create_vm(prog->context, &prog->vm_config);
	struct repl_context_s repl_context = {
		.read_pos = 1,
		.ctx = prog->context,
		.vtable = { .read = repl_read, .print = repl_print }
	};
	linenoiseInstallWindowChangeHandler();
	lip_repl(vm, lip_string_ref("<stdin>"), &repl_context.vtable);
	lip_free(lip_default_allocator, repl_context.line_buff);

	return EXIT_SUCCESS;
}

static int
run_script(program_state_t* prog)
{
	lip_script_t* script = lip_load_script(
		prog->context, lip_string_ref("<stdin>"), lip_stdin()
	);
	if(!script)
	{
		print_error(prog->context);
		return EXIT_FAILURE;
	}

	lip_vm_t* vm = lip_create_vm(prog->context, &prog->vm_config);
	lip_value_t result;
	lip_exec_status_t status = lip_exec_script(vm, script, &result);

	return status == LIP_EXEC_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
main(int argc, char* argv[])
{
	cargo_t cargo;
	if(cargo_init(&cargo, 0, argv[0]))
	{
		return EXIT_FAILURE;
	}

	program_state_t program_state = {
		.vm_config = {
			.os_len = 256,
			.cs_len = 256,
			.env_len = 256
		}
	};

	bool show_version;
	cargo_add_option(
		cargo, 0, "--version -v", "Show version information", "b", &show_version
	);

	cargo_parse_result_t parse_result = cargo_parse(cargo, 0, 0, argc, argv);
	cargo_destroy(&cargo);
	switch(parse_result)
	{
		case CARGO_PARSE_OK:
			break;
		case CARGO_PARSE_SHOW_HELP:
			return EXIT_SUCCESS;
		default:
			return EXIT_FAILURE;
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

	program_state.runtime = lip_create_runtime(lip_default_allocator);
	program_state.context = lip_create_context(program_state.runtime, lip_default_allocator);
	lip_load_builtins(program_state.context);

	int exit_code;
	if(isatty(fileno(stdin)))
	{
		exit_code = repl(&program_state);
	}
	else
	{
		exit_code = run_script(&program_state);
	}

	lip_destroy_runtime(program_state.runtime);

	return exit_code;
}
