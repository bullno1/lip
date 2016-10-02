#ifndef LIP_RUNTIME_H
#define LIP_RUNTIME_H

#include "common.h"
#include "vm.h"

#define LIP_ERROR_STAGE(F) \
	F(LIP_STAGE_LEXER) \
	F(LIP_STAGE_PARSER) \
	F(LIP_STAGE_COMPILER) \
	F(LIP_STAGE_EXEC)

LIP_ENUM(lip_error_stage_t, LIP_ERROR_STAGE)

typedef struct lip_runtime_s lip_runtime_t;
typedef struct lip_runtime_config_s lip_runtime_config_t;
typedef bool(*lip_repl_handler_t)(
	void* context,
	lip_runtime_t* runtime,
	lip_vm_t* vm,
	bool status,
	lip_value_t* result
);

struct lip_runtime_config_s
{
	lip_vm_config_t vm_config;
};

lip_runtime_t*
lip_runtime_create(lip_allocator_t* allocator, lip_runtime_config_t* config);

void
lip_runtime_destroy(lip_runtime_t* runtime);

const lip_error_t*
lip_runtime_last_error(lip_runtime_t* runtime);

bool
lip_runtime_exec(
	lip_runtime_t* runtime,
	lip_in_t* stream,
	lip_string_ref_t name,
	lip_vm_t* vm,
	lip_value_t* result
);

void
lip_runtime_repl(
	lip_runtime_t* runtime,
	lip_in_t* stream,
	lip_string_ref_t name,
	lip_vm_t* vm,
	lip_repl_handler_t handler,
	void* context
);

lip_vm_t*
lip_runtime_alloc_vm(lip_runtime_t* runtime, lip_vm_config_t* config);

void
lip_runtime_free_vm(lip_runtime_t* runtime, lip_vm_t* vm);

lip_vm_t*
lip_runtime_default_vm(lip_runtime_t* runtime);

#endif
