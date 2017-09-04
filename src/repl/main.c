#include <stdlib.h>
#include <lip/core.h>
#include <lip/core/memory.h>
#include <lip/core/print.h>
#include <lip/std/runtime.h>
#include <lip/std/io.h>
#include <lip/std/lib.h>
#include <lip/dbg.h>
#include <linenoise/linenoise.h>
#include <linenoise/encodings/utf8.h>
#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

#define OPTPARSE_IMPLEMENTATION
#define OPTPARSE_API static
#include <optparse/optparse.h>
#include "../cli.h"

#define quit(code) exit_code = code; goto quit;

typedef struct repl_context_s repl_context_t;

struct repl_context_s
{
	lip_repl_handler_t vtable;
	lip_context_t* ctx;
	lip_vm_t* vm;
	char* line_buff;
	size_t read_pos;
	size_t buff_len;
};

const struct optparse_long opts[] = {
	{ "help", 'h', OPTPARSE_NONE },
	{ "version", 'v', OPTPARSE_NONE },
	{ "interactive", 'i', OPTPARSE_NONE },
	{ "debug", 'd', OPTPARSE_OPTIONAL },
	{ "execute", 'e', OPTPARSE_REQUIRED },
	{ 0 }
};

const char* help[] = {
	NULL, "Print this message",
	NULL, "Show version information",
	NULL, "Enter interactive mode after executing `script`",
	"off|step|error", "Enable debugger (default: 'step')",
	"string", "Execute `string`",
};

static void
show_usage()
{
	fprintf(stderr, "Usage: lip [options] [--] [script]\n");
	fprintf(stderr, "Available options:\n");
	show_options(opts, help);
}

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
			repl->line_buff = realloc(repl->line_buff, line_length + 1);
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
		lip_print_error(lip_stderr(), repl->ctx);
		break;
	}
}

bool
repl_run_script(
	lip_context_t* ctx,
	lip_vm_t* vm,
	lip_string_ref_t filename,
	lip_in_t* input
)
{
	lip_script_t* script = lip_load_script(ctx, filename, input);
	if(!script)
	{
		lip_print_error(lip_stderr(), ctx);
		return false;
	}

	lip_value_t result;
	bool success = lip_exec_script(vm, script, &result) == LIP_EXEC_OK;
	if(!success) { lip_print_error(lip_stderr(), ctx); }
	lip_unload_script(ctx, script);

	return success;
}

int
main(int argc, char* argv[])
{
	(void)argc;
	int exit_code = EXIT_SUCCESS;

	const char* debug_mode = "off";
	bool interactive = false;
	const char* exec_string = NULL;
	const char* script_filename = NULL;

	lip_runtime_config_t* config = NULL;
	lip_runtime_t* runtime = NULL;
	lip_context_t* ctx = NULL;
	lip_vm_t* vm = NULL;
	lip_dbg_t* dbg = NULL;

    int option;
    struct optparse options;
    optparse_init(&options, argv);

    while((option = optparse_long(&options, opts, NULL)) != -1)
	{
		switch(option)
		{
			case 'h':
				show_usage();
				quit(EXIT_SUCCESS);
				break;
			case 'v':
				printf("lip %s\n", LIP_VERSION);
				quit(EXIT_SUCCESS);
				break;
			case '?':
				fprintf(stderr, "lip: %s\n", options.errmsg);
				quit(EXIT_FAILURE);
				break;
			case 'i':
				interactive = true;
				break;
			case 'd':
				debug_mode = options.optarg ? options.optarg : "step";
				break;
			case 'e':
				exec_string = options.optarg;
				break;
		}
	}

	script_filename = optparse_arg(&options);

	config = lip_create_std_runtime_config(NULL);
	runtime = lip_create_runtime(config);
	ctx = lip_create_context(runtime, NULL);
	vm = lip_create_vm(ctx, NULL);
	lip_load_stdlib(ctx);

	lip_dbg_config_t dbg_conf = {
		.allocator = config->allocator,
		.fs = config->fs,
		.port = 8081
	};

	if(strcmp(debug_mode, "step") == 0)
	{
		dbg_conf.hook_step = true;
	}
	else if(strcmp(debug_mode, "error") == 0)
	{
		dbg_conf.hook_step = false;
	}
	else if(strcmp(debug_mode, "off"))
	{
		fprintf(stderr, "lip: Invalid debug mode: '%s'\n", debug_mode);
		show_usage();
		quit(EXIT_FAILURE);
	}

	if(strcmp(debug_mode, "off"))
	{
		dbg = lip_create_debugger(&dbg_conf);
		lip_attach_debugger(dbg, vm);
	}

	if(exec_string)
	{
		lip_string_ref_t cmdline_str_ref = lip_string_ref(exec_string);
		struct lip_isstream_s sstream;

		lip_in_t* input = lip_make_isstream(cmdline_str_ref, &sstream);
		if(!repl_run_script(ctx, vm, lip_string_ref("<cmdline>"), input))
		{
			quit(EXIT_FAILURE);
		}
	}

	if(exec_string && !(interactive || script_filename))
	{
		quit(EXIT_SUCCESS);
	}

	if(script_filename)
	{
		if(!repl_run_script(
			ctx, vm, lip_string_ref(script_filename), NULL
		))
		{
			quit(EXIT_FAILURE);
		}
	}

	if(script_filename && !interactive)
	{
		quit(EXIT_SUCCESS);
	}

	if(isatty(fileno(stdin)) || interactive)
	{
		linenoiseSetMultiLine(1);
		linenoiseSetEncodingFunctions(
			linenoiseUtf8PrevCharLen,
			linenoiseUtf8NextCharLen,
			linenoiseUtf8ReadCode
		);

		struct repl_context_s repl_context = {
			.read_pos = 1,
			.ctx = ctx,
			.vm = vm,
			.vtable = { .read = repl_read, .print = repl_print }
		};
		lip_repl(vm, lip_string_ref("<stdin>"), &repl_context.vtable);
		free(repl_context.line_buff);
	}
	else
	{
		bool status = repl_run_script(
			ctx, vm, lip_string_ref("<stdin>"), lip_stdin()
		);
		exit_code = status ? EXIT_SUCCESS : EXIT_FAILURE;
	}

quit:
	if(config) { lip_destroy_std_runtime_config(config); }
	if(vm) { lip_destroy_vm(ctx, vm); }
	if(ctx) { lip_destroy_context(ctx); }
	if(runtime) { lip_destroy_runtime(runtime); }
	if(dbg) { lip_destroy_debugger(dbg); }
	return exit_code;
}
