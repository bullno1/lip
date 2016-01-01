#include <stdio.h>
#include <stdlib.h>
#include <lip/runtime.h>
#include <lip/asm.h>
#include <lip/allocator.h>
#include <lip/stack_allocator.h>
#include <lip/utils.h>
#include <lip/value.h>
#include "flag.h"

static lip_exec_status_t print(lip_vm_t* vm)
{
	lip_value_print(lip_fwrite, stdout, lip_vm_get_arg(vm, 0), 4, 0);
	printf("\n");
	lip_vm_push_nil(vm);
	return LIP_EXEC_OK;
}

void vm_hook(lip_vm_t* vm, void* context)
{
	lip_printf(lip_fwrite, stderr, "stack:\n");
	lip_printf(lip_fwrite, stderr, "================\n");
	bool first = true;
	for(lip_value_t* itr = vm->min_sp; itr < vm->sp; ++itr)
	{
		if(first)
		{
			first = false;
		}
		else
		{
			printf("----------------\n");
		}
		lip_printf(lip_fwrite, stderr, "> ");
		lip_value_print(lip_fwrite, stderr, itr, 5, 0);
		lip_printf(lip_fwrite, stderr, "\n");
	}
	printf("================\n");
	lip_asm_print(lip_fwrite, stderr, *(vm->ctx.pc));
	printf("\n");
}

bool exec_handler(void* context, lip_exec_status_t status, lip_value_t* result)
{
	lip_value_print(lip_fwrite, stderr, result, 5, 0);
	lip_printf(lip_fwrite, stderr, "\n");
	return true;
}

int main(int argc, const char** argv)
{
	bool no_repl = false;
	bool show_help = false;
	bool dump_ast = false;
	bool dump_code = false;
	bool hook = false;
	int heap_size = 1024 * 1024;

	flagset_t* fs = flagset_new();
	flagset_int(fs, &heap_size, "heap-size", "Set vm's heap size");
	flagset_bool(fs, &no_repl, "no-repl", "Suppress the Read-Eval-Print-Loop");
	flagset_bool(fs, &dump_ast, "dump-ast", "Dump ast to stderr after parsing");
	flagset_bool(fs, &dump_code, "dump-code", "Dump bytecode to stderr after compilation");
	flagset_bool(fs, &hook, "hook", "Install a vm hook to trace execution");
	flagset_bool(fs, &show_help, "help", "Print this message");

	flag_error fs_err = flagset_parse(fs, argc, argv);
	switch(fs_err)
	{
		case FLAG_ERROR_PARSING:
			fprintf(stderr, "invalid value for --%s\n", fs->error.flag->name);
			break;
		case FLAG_ERROR_ARG_MISSING:
			fprintf(stderr, "missing value for --%s\n", fs->error.flag->name);
			break;
		case FLAG_ERROR_UNDEFINED_FLAG:
			fprintf(stderr, "undefined flag %s\n", fs->error.arg);
			break;
		case FLAG_OK:
			break;
	}

	if(show_help || fs_err != FLAG_OK)
	{
		flagset_write_usage(fs, stderr, "lip");
		flagset_free(fs);
		return fs_err == FLAG_OK ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	flagset_free(fs);

	lip_allocator_t* vm_allocator =
		lip_stack_allocator_new(lip_default_allocator, heap_size);

	lip_runtime_config_t config = {
		.error_fn = lip_fwrite,
		.error_ctx = stderr,
		.dump_ast = dump_ast,
		.dump_code = dump_code,
		.vm_config = {
			.allocator = vm_allocator,
			.operand_stack_length = 256,
			.environment_length = 256,
			.call_stack_length = 256
		},
		.allocator = lip_default_allocator
	};

	lip_native_module_entry_t builtins[] = {
		{"print", print, 1}
	};

	lip_runtime_t* runtime = lip_runtime_new(&config);

	if(hook)
	{
		lip_runtime_get_vm(runtime)->hook = vm_hook;
	}

	lip_runtime_register_native_module(
		runtime, sizeof(builtins) / sizeof(lip_native_module_entry_t), builtins
	);
	lip_runtime_exec_fileh(
		runtime,
		exec_handler,
		NULL,
		"<stdin>",
		stdin
	);
	lip_runtime_delete(runtime);

	lip_stack_allocator_delete(vm_allocator);

	return EXIT_SUCCESS;
}
