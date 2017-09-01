#include <stdlib.h>
#include <string.h>
#include <lip/config.h>
#include <lip/lip.h>
#include <lip/io.h>
#include <cargo.h>
#include "common.h"
#include "run.h"
#include "compile.h"
#include "inspect.h"

#define quit(code) exit_code = code; goto quit;

int
main(int argc, char* argv[])
{
	int exit_code = EXIT_FAILURE;
	cargo_t cargo;
	if(cargo_init(&cargo, 0, argv[0]))
	{
		return EXIT_FAILURE;
	}

	int show_version = false;
	struct repl_common_s common = {
		.argc = argc,
		.argv = argv
	};
	struct repl_run_opts_s run_opts;
	struct repl_compile_opts_s compile_opts;
	struct repl_inspect_opts_s inspect_opts;

	lip_reset_runtime_config(&common.cfg);

	cargo_add_option(
		cargo, 0,
		"--version -v", "Show version information", "b",
		&show_version
	);
	cargo_add_mutex_group(cargo, 0, "mode", "Operation mode", NULL);

	repl_init_run_opts(cargo, &run_opts);
	repl_init_compile_opts(cargo, &compile_opts);
	repl_init_inspect_opts(cargo, &inspect_opts);

	cargo_add_option(
		cargo, CARGO_OPT_NOT_REQUIRED | CARGO_OPT_STOP,
		"script", "Script file to compile/execute/inspect", "s",
		&common.script_filename
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

	int num_modes = 0;
	if(repl_run_mode_activated(&run_opts)) { ++num_modes; }
	if(repl_compile_mode_activated(&compile_opts)) { ++num_modes; }
	if(repl_inspect_mode_activated(&inspect_opts)) { ++num_modes; }

	if(num_modes > 1)
	{
		lip_printf(
			lip_stderr(), "Cannot operate in multiple modes at the same time"
		);
		quit(EXIT_FAILURE);
	}

	if(show_version && num_modes == 0) { quit(EXIT_SUCCESS); }

	common.runtime = lip_create_runtime(&common.cfg);
	common.context = lip_create_context(common.runtime, NULL);

	if(repl_compile_mode_activated(&compile_opts))
	{
		quit(repl_compile(&common, &compile_opts));
	}
	else if(repl_inspect_mode_activated(&inspect_opts))
	{
		quit(repl_inspect(&common, &inspect_opts));
	}
	else
	{
		quit(repl_run(&common, &run_opts));
	}

quit:
	free(common.script_filename);
	repl_cleanup_inspect_opts(&inspect_opts);
	repl_cleanup_compile_opts(&compile_opts);
	repl_cleanup_run_opts(&run_opts);
	if(common.runtime)
	{
		lip_destroy_context(common.context);
		lip_destroy_runtime(common.runtime);
	}

	return exit_code;
}
