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

extern "C" void test_cpp()
{
	lip_runtime_t* runtime = lip_create_runtime(lip_default_allocator);
	lip_destroy_runtime(runtime);
}
