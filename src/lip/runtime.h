#ifndef LIP_RUNTIME_H
#define LIP_RUNTIME_H

#include <stdio.h>
#include "types.h"
#include "value.h"
#include "function.h"
#include "vm.h"

typedef struct lip_runtime_t lip_runtime_t;
typedef struct lip_module_t lip_module_t;
typedef struct lip_linker_t lip_linker_t;

typedef struct lip_runtime_config_t {
	lip_write_fn_t error_fn;
	void* error_ctx;

	lip_vm_config_t vm_config;
	lip_allocator_t* allocator;
} lip_runtime_config_t;

typedef struct lip_native_module_entry_t
{
	const char* name;
	lip_native_function_t function;
	int arity;
} lip_native_module_entry_t;

lip_runtime_t* lip_runtime_new(lip_runtime_config_t* cfg);
void lip_runtime_delete(lip_runtime_t* runtime);

lip_module_t* lip_runtime_load_filen(
	lip_runtime_t* runtime, const char* filename
);
lip_module_t* lip_runtime_load_fileh(
	lip_runtime_t* runtime, const char* filename, FILE* file
);
lip_module_t* lip_runtime_load_stream(
	lip_runtime_t* runtime,
	const char* filename,
	lip_read_fn_t read_fn,
	void* stream
);

lip_module_t* lip_runtime_compile_filen(
	lip_runtime_t* runtime, const char* filename
);
lip_module_t* lip_runtime_compile_fileh(
	lip_runtime_t* runtime, const char* filename, FILE* file
);
lip_module_t* lip_runtime_compile_stream(
	lip_runtime_t* runtime,
	const char* filename,
	lip_read_fn_t read_fn,
	void* stream
);

void lip_runtime_load_module(lip_runtime_t* runtime, lip_module_t* module);
void lip_runtime_unload_module(lip_runtime_t* runtime, lip_module_t* module);
void lip_runtime_free_module(lip_runtime_t* runtime, lip_module_t* module);

typedef bool (*lip_runtime_exec_handler_t)(
	lip_exec_status_t status, lip_value_t* result
);

void lip_runtime_exec_filen(
	lip_runtime_t* runtime,
	lip_runtime_exec_handler_t handler,
	const char* filename
);
void lip_runtime_exec_fileh(
	lip_runtime_t* runtime,
	lip_runtime_exec_handler_t handler,
	const char* filename,
	FILE* file
);
void lip_runtime_exec_stream(
	lip_runtime_t* runtime,
	lip_runtime_exec_handler_t handler,
	const char* filename,
	lip_read_fn_t read_fn,
	void* stream
);

lip_module_t* lip_runtime_register_native_module(
	lip_runtime_t* runtime,
	unsigned int num_entries,
	lip_native_module_entry_t* entries
);

lip_module_t* lip_runtime_create_native_module(
	lip_runtime_t* runtime,
	unsigned int num_entries,
	lip_native_module_entry_t* entries
);

lip_exec_status_t lip_runtime_exec_symbol(
	lip_runtime_t* runtime,
	const char* symbol,
	lip_value_t* result
);

lip_vm_t* lip_runtime_get_vm(lip_runtime_t* runtime);
lip_linker_t* lip_runtime_get_linker(lip_runtime_t* runtime);

#endif
