#ifndef LIP_TEST_MODULE_H
#define LIP_TEST_MODULE_H

#include <lip/allocator.h>
#include <lip/module.h>
#include <lip/bundler.h>
#include "asm.h"

static lip_exec_status_t print(lip_vm_t* vm)
{
	return LIP_EXEC_OK;
}

static const char* main_sym = "main";
static const char* print_sym = "print";

static lip_module_t* bundle_functions(
	lip_bundler_t* bundler, lip_function_t* function
)
{
	lip_bundler_begin(bundler);

	lip_string_ref_t main_ref = { strlen(main_sym), main_sym };
	lip_bundler_add_lip_function(bundler, main_ref, function);

	lip_string_ref_t print_ref = { strlen(print_sym), print_sym };
	lip_bundler_add_native_function(bundler, print_ref, 0, print);

	return lip_bundler_end(bundler);
}

#endif
