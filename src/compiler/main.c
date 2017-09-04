#include <stdlib.h>
#include <lip/core.h>
#include <lip/core/memory.h>
#include <lip/core/print.h>
#include <lip/std/runtime.h>
#include <lip/std/io.h>
#define OPTPARSE_IMPLEMENTATION
#define OPTPARSE_API static
#include <optparse/optparse.h>
#include "../cli.h"

#define quit(code) exit_code = code; goto quit;

const struct optparse_long opts[] = {
	{ "help", 'h', OPTPARSE_NONE },
	{ "version", 'v', OPTPARSE_NONE },
	{ "output", 'o', OPTPARSE_REQUIRED },
	{ "inspect", 'i', OPTPARSE_OPTIONAL },
	{ 0 }
};

const char* help[] = {
	NULL, "Print this message",
	NULL, "Show version information",
	"name", "Output bytecode to file `name`",
	"depth", "Inspect script up to depth `depth` (default: 1)",
};

static void
show_usage()
{
	fprintf(stderr, "Usage: lipc [options] [--] <input>\n");
	fprintf(stderr, "Available options:\n");
	show_options(opts, help);
	fprintf(stderr, "\nUse '-' as `input` to read from stdin\n");
}

int
main(int argc, char* argv[])
{
	(void)argc;

	int exit_code = EXIT_SUCCESS;

	const char* input_file = NULL;
	const char* output_file = NULL;
	int print_depth = -1;

	lip_runtime_config_t* config = NULL;
	lip_runtime_t* runtime = NULL;
	lip_context_t* ctx = NULL;
	lip_script_t* script = NULL;

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
				fprintf(stderr, "lipc: %s\n", options.errmsg);
				quit(EXIT_FAILURE);
				break;
			case 'o':
				output_file = options.optarg;
				break;
			case 'i':
				print_depth = options.optarg ? atoi(options.optarg) : 1;
				break;
		}
	}

	input_file = optparse_arg(&options);

	if(!input_file)
	{
		fprintf(stderr, "lipc: No input file given\n");
		show_usage();
		quit(EXIT_FAILURE);
	}

	config = lip_create_std_runtime_config(NULL);
	runtime = lip_create_runtime(config);
	ctx = lip_create_context(runtime, NULL);

	lip_in_t* input;

	if(strcmp(input_file, "-") == 0)
	{
		input_file = "<stdin>";
		input = lip_stdin();
	}
	else
	{
		input = NULL;
	}

	script = lip_load_script(ctx, lip_string_ref(input_file), input);
	if(!script)
	{
		lip_print_error(lip_stderr(), ctx);
		quit(EXIT_FAILURE);
	}

	if(output_file)
	{
		bool result = lip_dump_script(
			ctx, script, lip_string_ref(output_file), NULL
		);
		if(!result)
		{
			lip_print_error(lip_stderr(), ctx);
			quit(EXIT_FAILURE);
		}
	}

	if(print_depth > 0)
	{
		lip_print_script(print_depth, 0, lip_stdout(), script);
	}

quit:
	if(script) { lip_unload_script(ctx, script); }
	if(ctx) { lip_destroy_context(ctx); }
	if(runtime) { lip_destroy_runtime(runtime); }
	if(config) { lip_destroy_std_runtime_config(config); }
	return exit_code;
}
