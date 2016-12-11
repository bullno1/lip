#ifndef LIP_LIP_H
#define LIP_LIP_H

#include "common.h"
#include "vm.h"

typedef struct lip_runtime_s lip_runtime_t;
typedef struct lip_runtime_config_s lip_runtime_config_t;
typedef struct lip_context_s lip_context_t;
typedef struct lip_script_s lip_script_t;
typedef struct lip_repl_handler_s lip_repl_handler_t;
typedef struct lip_context_error_s lip_context_error_t;
typedef struct lip_error_record_s lip_error_record_t;

struct lip_repl_handler_s
{
	lip_in_t*(*input)(lip_repl_handler_t* self);
	void(*prompt)(lip_repl_handler_t* self);
	void(*handle_result)(lip_repl_handler_t* self, bool error, lip_value_t result);
};

struct lip_error_record_s
{
	lip_string_ref_t message;
	lip_loc_range_t location;
};

struct lip_context_error_s
{
	lip_string_ref_t message;

	unsigned int num_records;
	lip_error_record_t* records;
};

lip_runtime_t*
lip_create_runtime(lip_allocator_t* allocator);

void
lip_destroy_runtime(lip_runtime_t* runtime);

lip_context_t*
lip_create_context(lip_runtime_t* runtime, lip_allocator_t* allocator);

void
lip_destroy_context(lip_runtime_t* runtime, lip_context_t* context);

const lip_context_error_t*
lip_get_error(lip_context_t* context);

lip_vm_t*
lip_create_vm(lip_context_t* ctx, lip_vm_config_t* config);

void
lip_destroy_vm(lip_context_t* ctx, lip_vm_t* vm);

bool
lip_lookup_symbol(lip_context_t* ctx, lip_string_ref_t symbol, lip_value_t* result);

lip_script_t*
lip_load_script(lip_context_t* ctx, lip_string_ref_t filename, lip_in_t* input);

void
lip_unload_script(lip_context_t* ctx, lip_script_t* script);

lip_exec_status_t
lip_exec_script(lip_vm_t* vm, lip_script_t* script, lip_value_t* result);

void
lip_repl(lip_vm_t* vm, lip_repl_handler_t* repl_handler);

#endif
