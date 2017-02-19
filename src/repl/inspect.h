#ifndef LIP_REPL_INSPECT_H
#define LIP_REPL_INSPECT_H

#include "common.h"

struct repl_inspect_opts_s
{
	int inspect;
	int print_depth;
};

void
repl_init_inspect_opts(cargo_t cargo, struct repl_inspect_opts_s* opts);

void
repl_cleanup_inspect_opts(struct repl_inspect_opts_s* opts);

bool
repl_inspect_mode_activated(struct repl_inspect_opts_s* opts);

int
repl_inspect(struct repl_common_s* common, struct repl_inspect_opts_s* opts);

#endif
