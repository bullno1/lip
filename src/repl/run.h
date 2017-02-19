#ifndef LIP_REPL_RUN_H
#define LIP_REPL_RUN_H

#include "common.h"

struct repl_run_opts_s
{
	int interactive;
	char* exec_string;
};

void
repl_init_run_opts(cargo_t cargo, struct repl_run_opts_s* opts);

void
repl_cleanup_run_opts(struct repl_run_opts_s* opts);

bool
repl_run_mode_activated(struct repl_run_opts_s* opts);

int
repl_run(struct repl_common_s* common, struct repl_run_opts_s* opts);

#endif
