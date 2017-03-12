#ifdef __linux__
#define _XOPEN_SOURCE
#endif

#ifndef _WIN32
#define LIP_HAS_LINENOISE
#endif

#include "run.h"
#include <stdlib.h>
#include <lip/lip.h>
#include <lip/io.h>
#include <lip/memory.h>
#include <lip/print.h>

#ifdef LIP_HAS_LINENOISE
#	include <linenoise.h>
#	include <encodings/utf8.h>
#endif

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

#include "common.h"

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

void
repl_init_run_opts(cargo_t cargo, struct repl_run_opts_s* opts)
{
	memset(opts, 0, sizeof(*opts));

	cargo_add_group(cargo, 0, "run", "Run mode (default)", NULL);
	cargo_add_option(
		cargo, 0,
		"<run> --interactive -i", "Enter interactive mode after executing script", "b",
		&opts->interactive
	);
	cargo_add_option(
		cargo, 0,
		"<run> --debug -d", "Enable debugger", "b",
		&opts->debug
	);
	cargo_add_option(
		cargo, 0,
		"<run> --debug-mode", "Set debugger mode: step, error (default: step)", "s",
		&opts->debug_mode
	);
	cargo_add_validation(
		cargo, 0,
		"--debug-mode",
		cargo_validate_choices(
			CARGO_VALIDATE_CHOICES_CASE_SENSITIVE,
			CARGO_STRING, 2, "step", "error"
		)
	);
	cargo_set_metavar(cargo, "--debug-mode", "MODE");
	cargo_add_option(
		cargo, 0,
		"<run> --execute -e", "Execute STRING", "s",
		&opts->exec_string
	);
	cargo_set_metavar(cargo, "--execute", "STRING");
}

void
repl_cleanup_run_opts(struct repl_run_opts_s* opts)
{
	if(opts->dbg)
	{
		lip_destroy_debugger(opts->dbg);
	}

	free(opts->debug_mode);
	free(opts->exec_string);
}

bool
repl_run_mode_activated(struct repl_run_opts_s* opts)
{
	return opts->debug || opts->interactive || opts->exec_string;
}

int
repl_run(struct repl_common_s* common, struct repl_run_opts_s* opts)
{
	lip_load_builtins(common->context);
	lip_vm_t* vm = lip_create_vm(common->context, NULL);

	if(opts->debug)
	{
		lip_dbg_config_t cfg;
		lip_reset_dbg_config(&cfg);
		if(opts->debug_mode && strcmp(opts->debug_mode, "error") == 0)
		{
			cfg.hook_step = false;
		}

		opts->dbg = lip_create_debugger(&cfg);
		lip_attach_debugger(opts->dbg, vm);
	}

	if(opts->exec_string)
	{
		lip_string_ref_t cmdline_str_ref = lip_string_ref(opts->exec_string);
		struct lip_isstream_s sstream;

		lip_in_t* input = lip_make_isstream(cmdline_str_ref, &sstream);
		if(!repl_run_script(common->context, vm, lip_string_ref("<cmdline>"), input))
		{
			return EXIT_FAILURE;
		}
	}

	if(opts->exec_string && !(opts->interactive || common->script_filename))
	{
		return EXIT_SUCCESS;
	}

	if(common->script_filename)
	{
		bool status = repl_run_script(
			common->context, vm, lip_string_ref(common->script_filename), NULL
		);
		if(!status) { return EXIT_FAILURE; }
	}

	if(common->script_filename && !opts->interactive) { return EXIT_SUCCESS; }

	if(isatty(fileno(stdin)) || opts->interactive)
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
			.ctx = common->context,
			.vm = vm,
			.vtable = { .read = repl_read, .print = repl_print }
		};
		lip_repl(vm, lip_string_ref("<stdin>"), &repl_context.vtable);
		lip_free(lip_default_allocator, repl_context.line_buff);
		return EXIT_SUCCESS;
	}
	else
	{
		bool status = repl_run_script(
			common->context, vm, lip_string_ref("<stdin>"), lip_stdin()
		);
		return status ? EXIT_SUCCESS : EXIT_FAILURE;
	}
}
