#ifndef LIP_REPL_COMPILE_H
#define LIP_REPL_COMPILE_H

#include "common.h"
#define MINIZ_HEADER_FILE_ONLY
#include <miniz.c>

struct repl_compile_opts_s
{
	int standalone;
	char* output_filename;
};

void
repl_init_compile_opts(cargo_t cargo, struct repl_compile_opts_s* opts);

void
repl_cleanup_compile_opts(struct repl_compile_opts_s* opts);

bool
repl_compile_mode_activated(struct repl_compile_opts_s* opts);

int
repl_compile(struct repl_common_s* common, struct repl_compile_opts_s* opts);

int
repl_compiled_script_entry(mz_zip_archive *archive, int argc, char* argv[]);

#endif
