#ifndef LIP_TEST_MODULE_H
#define LIP_TEST_MODULE_H

#include <lip/allocator.h>
#include <lip/module.h>
#include <lip/bundler.h>
#include <lip/value.h>
#include <lip/vm.h>
#include <lip/utils.h>
#include "asm.h"

static lip_exec_status_t print(lip_vm_t* vm)
{
	lip_value_print(lip_vm_get_arg(vm, 0), 0);
	printf("\n");
	lip_vm_push_value(vm, lip_vm_get_arg(vm, 0));
	return LIP_EXEC_OK;
}

static lip_module_t* bundle_functions(
	lip_bundler_t* bundler, lip_function_t* function
)
{
	lip_bundler_begin(bundler);
	lip_bundler_add_lip_function(bundler, lip_string_ref("main"), function);
	lip_bundler_add_native_function(bundler, lip_string_ref("print"), print, 0);
	return lip_bundler_end(bundler);
}

#endif
