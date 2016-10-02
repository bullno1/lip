#ifndef LIP_RUNTIME_EX_H
#define LIP_RUNTIME_EX_H

#include "../runtime.h"
#include "parser.h"
#include "compiler.h"
#include "universe.h"

struct lip_runtime_s
{
	lip_allocator_t* allocator;
	lip_runtime_config_t config;
	lip_parser_t parser;
	lip_compiler_t compiler;
	lip_universe_t universe;
	lip_last_error_t last_error;
	lip_vm_t* default_vm;
};

void
lip_runtime_init(
	lip_runtime_t* runtime,
	lip_allocator_t* allocator,
	lip_runtime_config_t* config
);

void
lip_runtime_cleanup(lip_runtime_t* runtime);

#endif
