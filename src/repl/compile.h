#ifndef LIP_REPL_COMPILE_H
#define LIP_REPL_COMPILE_H

#include "common.h"

struct repl_compile_opts_s
{
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

#endif
