#include <lip/array.h>
#include <lip/asm.h>
#include <lip/bind.h>
#include <lip/common.h>
#include <lip/compiler.h>
#include <lip/config.h>
#include <lip/extra.h>
#include <lip/io.h>
#include <lip/lexer.h>
#include <lip/lip.h>
#include <lip/memory.h>
#include <lip/opcode.h>
#include <lip/parser.h>
#include <lip/pp.h>
#include <lip/print.h>
#include <lip/sexp.h>
#include <lip/vm.h>
#include <lip/pp.h>
#include <lip/extra.h>

#define _Bool bool
#include "munit.h"

namespace cpp {

lip_exec_status_t cpp_identity(lip_vm_t* vm, lip_value_t* result)
{
	lip_bind_args((any, x));
	lip_return(x);
}

}

extern "C" void test_cpp()
{
	lip_runtime_config_t cfg;
	lip_reset_runtime_config(&cfg);
	lip_runtime_t* runtime = lip_create_runtime(&cfg);
	lip_context_t* ctx = lip_create_context(runtime, NULL);
	lip_vm_t* vm = lip_create_vm(ctx, NULL);
	lip_value_t fn = lip_make_function(vm, cpp::cpp_identity, 0, NULL);
	lip_value_t result;
	lip_exec_status_t status = lip_call(vm, &result, fn, 1, lip_make_number(vm, 42.5));
	munit_assert_uint(LIP_EXEC_OK, ==, status);
	munit_assert_uint(LIP_VAL_NUMBER, ==, result.type);
	munit_assert_double_equal(42.5, result.data.number, 2);

	lip_destroy_runtime(runtime);
}
