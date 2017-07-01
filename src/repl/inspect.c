#include "inspect.h"
#include <lip/io.h>
#include <lip/print.h>

void
repl_init_inspect_opts(cargo_t cargo, struct repl_inspect_opts_s* opts)
{
	*opts = (struct repl_inspect_opts_s){
		.print_depth = 10
	};

	cargo_add_group(cargo, 0, "inspect", "Inspect mode", NULL);
	cargo_add_option(
		cargo, 0,
		"<!mode, inspect> --inspect", "Print informations about script", "b",
		&opts->inspect
	);
	cargo_add_option(
		cargo, 0,
		"<inspect> --depth", "Print depth", "i",
		&opts->print_depth
	);

	cargo_set_metavar(cargo, "--compile", "OUTPUT");
}

void
repl_cleanup_inspect_opts(struct repl_inspect_opts_s* opts)
{
	(void)opts;
}

bool
repl_inspect_mode_activated(struct repl_inspect_opts_s* opts)
{
	return opts->inspect;
}

int
repl_inspect(struct repl_common_s* common, struct repl_inspect_opts_s* opts)
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

	lip_print_script(opts->print_depth, 0, lip_stdout(), script);

	return EXIT_SUCCESS;
}
