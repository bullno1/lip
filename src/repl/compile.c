#include "compile.h"
#include <lip/io.h>

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
	return opts->output_filename;
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

	lip_script_t* script = lip_load_script(common->context, source_name, input);
	if(!script)
	{
		lip_print_error(lip_stderr(), common->context);
		return EXIT_FAILURE;
	}

	bool result = lip_dump_script(
		common->context, script, lip_string_ref(opts->output_filename), NULL
	);
	if(!result) { lip_print_error(lip_stderr(), common->context); }

	return result ? EXIT_SUCCESS : EXIT_FAILURE;
}
