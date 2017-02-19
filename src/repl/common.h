#ifndef LIP_REPL_COMMON_H
#define LIP_REPL_COMMON_H

#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <cargo.h>
#include <lip/lip.h>

struct repl_common_s
{
	lip_runtime_config_t cfg;
	lip_runtime_t* runtime;
	lip_context_t* context;
	char* script_filename;
};

bool
repl_run_script(
	lip_context_t* ctx,
	lip_vm_t* vm,
	lip_string_ref_t filename,
	lip_in_t* input
);

#endif
