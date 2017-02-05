#include <lip/lip.h>
#include <lip/memory.h>
#include <lip/io.h>
#include <lip/print.h>
#include <lip/vm.h>
#include <lip/opcode.h>
#include "munit.h"
#include "test_helpers.h"

#define SOURCE_NAME __FILE__ ":"  STRINGIFY(__LINE__)
#define STRINGIFY(X) STRINGIFY2(X)
#define STRINGIFY2(X) #X

#define lip_assert_result(code, expected_status, assert_fn, expected_result) \
	do { \
		test_hook_t hook = { .printed = false, .vtable = { .step = step } }; \
		lip_set_vm_hook(vm, &hook.vtable); \
		struct lip_isstream_s sstream; \
		lip_in_t* input = lip_make_isstream(lip_string_ref(code), &sstream); \
		lip_script_t* script = lip_load_script(ctx, lip_string_ref(SOURCE_NAME), input); \
		munit_assert_not_null(script); \
		lip_print_closure(100, 0, lip_null, (lip_closure_t*)script); \
		lip_value_t result; \
		lip_reset_vm(vm); \
		lip_assert_enum(lip_exec_status_t, expected_status, ==, lip_exec_script(vm, script, &result)); \
		assert_fn(expected_result, result); \
	} while(0)

#define lip_assert_num_result(code, result_value) \
	lip_assert_result(code, LIP_EXEC_OK, lip_assert_num, result_value)

#define lip_assert_str_result(code, result_value) \
	lip_assert_result(code, LIP_EXEC_OK, lip_assert_str, result_value)

#define lip_assert_symbol_result(code, result_value) \
	lip_assert_result(code, LIP_EXEC_OK, lip_assert_symbol, result_value)

#define lip_assert_nil_result(code) \
	lip_assert_result(code, LIP_EXEC_OK, lip_assert_nil, placeholder)

#define lip_assert_boolean_result(code, result_value) \
	lip_assert_result(code, LIP_EXEC_OK, lip_assert_boolean, result_value)

#define lip_assert_error_msg(code, msg) \
	lip_assert_result(code, LIP_EXEC_ERROR, lip_assert_str, msg)

#define lip_assert_syntax_error(code, error_msg, start_line, start_col, end_line, end_col) \
	do { \
		struct lip_isstream_s sstream; \
		lip_in_t* input = lip_make_isstream(lip_string_ref(code), &sstream); \
		lip_script_t* script = lip_load_script(ctx, lip_string_ref(SOURCE_NAME), input); \
		munit_assert_null(script); \
		const lip_context_error_t* error = lip_get_error(ctx); \
		lip_assert_string_ref_equal(lip_string_ref("Syntax error"), error->message); \
		lip_assert_string_ref_equal(lip_string_ref(error_msg), error->records[0].message); \
		munit_assert_uint(1, ==, error->num_records); \
		lip_loc_range_t expected_location = { \
			.start = { .line = start_line, .column = start_col }, \
			.end = { .line = end_line, .column = end_col } \
		}; \
		lip_assert_loc_range_equal(expected_location, error->records[0].location); \
	} while(0)

typedef struct lip_fixture_s lip_fixture_t;
typedef struct test_hook_s test_hook_t;

struct test_hook_s
{
	lip_vm_hook_t vtable;
	bool printed;
};

struct lip_fixture_s
{
	lip_runtime_t* runtime;
	lip_context_t* context;
	lip_vm_t* vm;
};

static void
step(lip_vm_hook_t* vtable, const lip_vm_t* vm)
{
	test_hook_t* hook = LIP_CONTAINER_OF(vtable, test_hook_t, vtable);
	if(!hook->printed)
	{
		hook->printed = true;
		lip_print_closure(10, 0, lip_stderr(), vm->fp->closure);
	}
}

static void*
setup(const MunitParameter params[], void* data)
{
	(void)params;
	(void)data;

	lip_runtime_config_t cfg;
	lip_reset_runtime_config(&cfg);
	lip_fixture_t* fixture = lip_new(lip_default_allocator, lip_fixture_t);
	fixture->runtime = lip_create_runtime(&cfg);
	fixture->context = lip_create_context(fixture->runtime, NULL);
	fixture->vm = lip_create_vm(fixture->context, NULL);
	return fixture;
}

static void
teardown(void* fixture_)
{
	lip_fixture_t* fixture = fixture_;
	lip_destroy_vm(fixture->context, fixture->vm);
	lip_destroy_context(fixture->runtime, fixture->context);
	lip_destroy_runtime(fixture->runtime);
	lip_free(lip_default_allocator, fixture);
}

static MunitResult
basic_forms(const MunitParameter params[], void* fixture_)
{
	(void)params;

	lip_fixture_t* fixture = fixture_;
	lip_context_t* ctx = fixture->context;
	lip_load_builtins(ctx);
	lip_vm_t* vm = fixture->vm;

	lip_assert_num_result("2", 2.0);
	lip_assert_num_result("((fn (x y) (x y)) (fn (x) x) 3.5)", 3.5);
	lip_assert_num_result(
		"(let ((x 1)"
		"      (y 2.5))"
		"    y)",
		2.5
	);
	lip_assert_num_result(
		"(let ((x 1.6)"
		"      (y 2.5))"
		"    (let ((x 2.3)) x))",
		2.3
	);
	lip_assert_num_result(
		"(let ((x 1.6)"
		"      (y 2.5))"
		"    (let ((x 2.3)) y))",
		2.5
	);
	lip_assert_num_result(
		"(let ((x 1.6)"
		"      (y 2.5))"
		"    (let ((test-capture (fn () x)))"
		"      (let ((x 4)) (test-capture))))",
		1.6
	);
	lip_assert_num_result(
		"(letrec ((test-capture (let ((y x)) (fn () y)))"
		"         (x 2.5))"
		"    (test-capture))",
		2.5
	);
	lip_assert_num_result(
		"(let ((x (fn (x) (fn () x))))"
		"   ((x 4.2)))",
		4.2
	);
	lip_assert_num_result(
		"(letrec ((y x)"
		"         (x 2.5))"
		"    y)",
		2.5
	);
	lip_assert_num_result("(if true 2 3)", 2.0);
	lip_assert_num_result("(if false 2 3)", 3.0);
	lip_assert_num_result("(if nil 2 3)", 3.0);
	lip_assert_num_result("(if 0 2 3)", 2.0);
	lip_assert_num_result(
		"(do"
		" (let ((x 2)) x)"
		" (let ((y 3) (x 1)) x))",
		1.0
	);
	lip_assert_num_result(
		"(let ((x 2))"
		" (let ((y 3)"
		"       (x 1))"
		"  nil)"
		" (let ((z 8)) x))",
		2.0
	);
	lip_assert_num_result(
		"(let ((x 2))"
		" (let ((y 3)"
		"       (x 1))"
		"  nil)"
		" (let ((z 6) (h x) (w 4)) h))",
		2.0
	);
	lip_assert_num_result("(if (if true 2) 1 2)", 1.0);
	lip_assert_num_result("(if (if false 2) 1 2)", 2.0);
	lip_assert_num_result("(((letrec ((x 1.5)) (fn () (fn () x)))))", 1.5);
	lip_assert_num_result("(let ((x (fn () (identity 3.5)))) (x))", 3.5);

	lip_assert_nil_result("(do)");
	lip_assert_nil_result("");

	// Integers around 23 bits limit
	lip_assert_num_result(STRINGIFY(LIP_LDI_MIN), LIP_LDI_MIN);
	lip_assert_num_result(STRINGIFY(LIP_LDI_MAX), LIP_LDI_MAX);
	char buff[256];

	snprintf(buff, sizeof(buff), "%d", LIP_LDI_MIN - 1);
	lip_assert_num_result(buff, LIP_LDI_MIN - 1);
	snprintf(buff, sizeof(buff), "%d", LIP_LDI_MAX + 1);
	lip_assert_num_result(buff, LIP_LDI_MAX + 1);

	snprintf(buff, sizeof(buff), "%d", LIP_LDI_MIN + 1);
	lip_assert_num_result(buff, LIP_LDI_MIN + 1);
	snprintf(buff, sizeof(buff), "%d", LIP_LDI_MAX - 1);
	lip_assert_num_result(buff, LIP_LDI_MAX - 1);

	lip_assert_num_result("(let ((x 1) (y (fn () (if false x 2)))) (y))", 2);
	lip_assert_num_result("(let ((x 1) (y (fn () (if true x 2)))) (y))", 1);
	lip_assert_nil_result("(let ((x 1) (y (fn () (if false x)))) (y))");
	lip_assert_num_result("(let ((x 1) (y (fn () (do 3 x)))) (y))", 1);
	lip_assert_num_result("(let ((x 1) (y (fn () (let ((x 5) (z x)) z)))) (y))", 5);
	lip_assert_num_result("(let ((x 1) (y (fn () (let ((x2 5) (z x)) x)))) (y))", 1);
	lip_assert_num_result("(let ((x 1) (y (fn () (let ((z x) (x 5)) z)))) (y))", 1);

	lip_assert_num_result("(let ((x 1) (y (fn () (letrec ((x 5) (z x)) z)))) (y))", 5);
	lip_assert_num_result("(let ((x 1) (y (fn () (letrec ((z x) (x 5)) z)))) (y))", 5);

	return MUNIT_OK;
}

static MunitResult
runtime_error(const MunitParameter params[], void* fixture_)
{
	(void)params;
	lip_fixture_t* fixture = fixture_;
	lip_context_t* ctx = fixture->context;
	lip_vm_t* vm = fixture->vm;

	lip_assert_error_msg("(identity 5)", "Undefined symbol: identity");
	const lip_context_error_t* error = lip_traceback(ctx, vm, lip_make_nil(vm));
	unsigned int bottom = error->num_records - 1;
	lip_assert_string_ref_equal(lip_string_ref(__FILE__), error->records[bottom].filename);
	lip_assert_string_ref_equal(lip_string_ref(__func__), error->records[bottom].message);

	lip_load_builtins(ctx);
	lip_assert_error_msg("(identity)", "Bad number of arguments (exactly 1 expected, got 0)");
	error = lip_traceback(ctx, vm, lip_make_nil(vm));
	bottom = error->num_records - 1;
	lip_assert_string_ref_equal(lip_string_ref(__FILE__), error->records[bottom].filename);
	lip_assert_string_ref_equal(lip_string_ref(__func__), error->records[bottom].message);
	lip_assert_string_ref_equal(lip_string_ref("identity"), error->records[0].message);

	return MUNIT_OK;
}

static MunitResult
builtins(const MunitParameter params[], void* fixture_)
{
	(void)params;

	lip_fixture_t* fixture = fixture_;
	lip_context_t* ctx = fixture->context;
	lip_vm_t* vm = fixture->vm;
	lip_load_builtins(ctx);

	lip_assert_num_result("(identity 5)", 5.0);
	lip_assert_num_result("(identity 4) (identity 5)", 5.0);
	lip_assert_nil_result("(nop 1 2 3 identity \"f\")");
	lip_assert_error_msg("(throw \"custom error\")", "custom error");

	lip_assert_boolean_result("(nil? nil)", true);
	lip_assert_boolean_result("(nil? 3)", false);
	lip_assert_boolean_result("(bool? nil)", false);
	lip_assert_boolean_result("(bool? true)", true);
	lip_assert_boolean_result("(bool? false)", true);
	lip_assert_boolean_result("(number? true)", false);
	lip_assert_boolean_result("(number? 3.0)", true);
	lip_assert_boolean_result("(string? 0)", false);
	lip_assert_boolean_result("(string? \"ff\")", true);
	lip_assert_boolean_result("(symbol? '\"f\")", false);
	lip_assert_boolean_result("(symbol? 'ff)", true);
	lip_assert_boolean_result("(list? '(1 2 3))", true);
	lip_assert_boolean_result("(list? 'ff)", false);
	lip_assert_boolean_result("(fn? '(1 2 3))", false);
	lip_assert_boolean_result("(fn? fn?)", true);

	lip_assert_num_result("(list/head '(1 2 3))", 1);
	lip_assert_num_result("(list/len (list/tail '(1 2 3)))", 2);
	lip_assert_num_result("(list/nth 0 (list/tail '(1 2 3)))", 2);
	lip_assert_num_result("(list/nth 1 (list/tail '(1 2 3)))", 3);
	lip_assert_num_result("(list/len (list/append '(1 2) 3))", 3);
	lip_assert_num_result("(list/nth 0 (list/append '(1 2) 3))", 1);
	lip_assert_num_result("(list/nth 1 (list/append '(1 2) 3))", 2);
	lip_assert_num_result("(list/nth 2 (list/append '(1 2) 3))", 3);
	lip_assert_num_result("(list/len (list/map (fn (x) (* 2 x)) '(1 2)))", 2);
	lip_assert_num_result("(list/nth 0 (list/map (fn (x) (* 2 x)) '(1 2)))", 2);
	lip_assert_num_result("(list/nth 1 (list/map (fn (x) (* 2 x)) '(1 2)))", 4);
	lip_assert_num_result("(list/len (list/foldl (fn (x l) (list/append l x)) '(1 2 3) '()))", 3);
	lip_assert_num_result("(list/nth 0 (list/foldl (fn (x l) (list/append l x)) '(1 2 3) '()))", 1);
	lip_assert_num_result("(list/nth 1 (list/foldl (fn (x l) (list/append l x)) '(1 2 3) '()))", 2);
	lip_assert_num_result("(list/nth 2 (list/foldl (fn (x l) (list/append l x)) '(1 2 3) '()))", 3);
	lip_assert_num_result("(list/len (list/foldr (fn (x l) (list/append l x)) '(1 2 3) '()))", 3);
	lip_assert_num_result("(list/nth 0 (list/foldr (fn (x l) (list/append l x)) '(1 2 3) '()))", 3);
	lip_assert_num_result("(list/nth 1 (list/foldr (fn (x l) (list/append l x)) '(1 2 3) '()))", 2);
	lip_assert_num_result("(list/nth 2 (list/foldr (fn (x l) (list/append l x)) '(1 2 3) '()))", 1);

	return MUNIT_OK;
}

static MunitResult
prim_ops(const MunitParameter params[], void* fixture_)
{
	(void)params;

	lip_fixture_t* fixture = fixture_;
	lip_context_t* ctx = fixture->context;
	lip_vm_t* vm = fixture->vm;
	lip_load_builtins(ctx);

	lip_assert_num_result("(+)", 0.0);
	lip_assert_num_result("(+ 1 2)", 3.0);
	lip_assert_num_result("(+ -1 2 3.5)", 4.5);
	lip_assert_num_result("(let ((op +)) (op 1 2))", 3.0);

	lip_assert_num_result("(*)", 1.0);
	lip_assert_num_result("(* 1 2)", 2.0);
	lip_assert_num_result("(* 2 2 3)", 12.0);
	lip_assert_num_result("(let ((op *)) (op 1 2))", 2.0);

	lip_assert_error_msg("(-)", "Bad number of arguments (at least 1 expected, got 0)");
	lip_assert_num_result("(- 5)", -5.0);
	lip_assert_num_result("(- 5 2.6)", 2.4);
	lip_assert_error_msg("(- 5 2 3)", "Bad number of arguments (at most 2 expected, got 3)");
	lip_assert_num_result("(let ((op -)) (op 1 2))", -1.0);
	lip_assert_num_result("(let ((op -)) (op 2.5))", -2.5);

	lip_assert_error_msg("(/)", "Bad number of arguments (at least 1 expected, got 0)");
	lip_assert_num_result("(/ 5)", (1.0 / 5.0));
	lip_assert_num_result("(/ 5 2.6)", (5.0 / 2.6));
	lip_assert_error_msg("(/ 5 2 3)", "Bad number of arguments (at most 2 expected, got 3)");
	lip_assert_num_result("(let ((op /)) (op 1 2))", (1.0 / 2.0));
	lip_assert_num_result("(let ((op /)) (op 2.5))", (1 / 2.5));

	lip_assert_error_msg("(!)", "Bad number of arguments (exactly 1 expected, got 0)");
	lip_assert_error_msg("(! 1 2)", "Bad number of arguments (exactly 1 expected, got 2)");
	lip_assert_boolean_result("(! 1)", false);
	lip_assert_boolean_result("(! 0)", false);
	lip_assert_boolean_result("(! nil)", true);
	lip_assert_boolean_result("(! true)", false);
	lip_assert_boolean_result("(! false)", true);
	lip_assert_boolean_result("(let ((not !)) (not not))", false);
	lip_assert_boolean_result("(let ((not !)) (not nil))", true);

	lip_assert_error_msg("(cmp)", "Bad number of arguments (exactly 2 expected, got 0)");
	lip_assert_num_result("(let ((op cmp)) (op 1 2))", -1);
	lip_assert_num_result("(cmp 1 2)", -1);
	lip_assert_num_result("(cmp nil 2)", -1);
	lip_assert_num_result("(cmp nil nil)", 0);

	lip_assert_error_msg("(<)", "Bad number of arguments (exactly 2 expected, got 0)");
	lip_assert_boolean_result("(let ((op <)) (op 1 2))", true);
	lip_assert_boolean_result("(< 1 2)", true);
	lip_assert_boolean_result("(< \"a\" \"ab\")", true);
	lip_assert_boolean_result("(< \"ab\" \"ab\")", false);
	lip_assert_boolean_result("(< \"a\" \"b\")", true);

	lip_assert_error_msg("(<=)", "Bad number of arguments (exactly 2 expected, got 0)");
	lip_assert_boolean_result("(let ((op <=)) (op 1 2))", true);
	lip_assert_boolean_result("(<= 1 2)", true);
	lip_assert_boolean_result("(<= \"a\" \"ab\")", true);

	lip_assert_error_msg("(>)", "Bad number of arguments (exactly 2 expected, got 0)");
	lip_assert_boolean_result("(let ((op >)) (op 1 2))", false);
	lip_assert_boolean_result("(> 1 2)", false);
	lip_assert_boolean_result("(> \"a\" \"ab\")", false);
	lip_assert_boolean_result("(> \"abc\" \"ab\")", true);
	lip_assert_boolean_result("(> \"ab\" \"ab\")", false);
	lip_assert_boolean_result("(> false true)", false);

	lip_assert_error_msg("(>=)", "Bad number of arguments (exactly 2 expected, got 0)");
	lip_assert_boolean_result("(let ((op >=)) (op 1 2))", false);
	lip_assert_boolean_result("(>= 1 2)", false);
	lip_assert_boolean_result("(>= \"a\" \"ab\")", false);
	lip_assert_boolean_result("(>= \"ab\" \"ab\")", true);
	lip_assert_boolean_result("(>= \"abc\" \"ab\")", true);

	lip_assert_error_msg("(==)", "Bad number of arguments (exactly 2 expected, got 0)");
	lip_assert_boolean_result("(let ((op ==)) (op \"a\" \"ab\"))", false);
	lip_assert_boolean_result("(== \"a\" \"ab\")", false);
	lip_assert_boolean_result("(== \"a\" \"a\")", true);
	lip_assert_boolean_result("(== true true)", true);
	lip_assert_boolean_result("(== true false)", false);

	lip_assert_error_msg("(!=)", "Bad number of arguments (exactly 2 expected, got 0)");
	lip_assert_boolean_result("(let ((op !=)) (op \"a\" \"ab\"))", true);
	lip_assert_boolean_result("(!= \"a\" \"ab\")", true);
	lip_assert_boolean_result("(!= \"a\" \"a\")", false);
	lip_assert_boolean_result("(!= true false)", true);
	lip_assert_boolean_result("(!= true true)", false);

	lip_assert_boolean_result("(< '(1) '(2))", true);
	lip_assert_boolean_result("(== '(1 2 3) '(1 2 3))", true);
	lip_assert_boolean_result("(< '(1 2) '(1 2 3))", true);
	lip_assert_boolean_result("(<= '(1 0 0) '(0 2 3))", false);

	lip_assert_boolean_result("(== '(-3 1 2 2.5 8) (list/sort '(2 1 8 -3 2.5)))", true);
	lip_assert_boolean_result("(== '(8 2.5 2 1 -3) (list/sort '(2 1 8 -3 2.5) (fn (a b) (cmp b a))))", true);
	lip_assert_error_msg(
		"(list/sort '(2 1 8 -3 2.5) identity)",
		"Bad number of arguments (exactly 1 expected, got 2)"
	);
	lip_assert_error_msg(
		"(list/sort '(2 1 8 -3 2.5) <)",
		"Comparision function did not return a number"
	);

	return MUNIT_OK;
}

static MunitResult
syntax_error(const MunitParameter params[], void* fixture_)
{
	(void)params;

	lip_fixture_t* fixture = fixture_;
	lip_context_t* ctx = fixture->context;

	lip_assert_syntax_error(" 1a  ", "Malformed number", 1, 2, 1, 3);
	lip_assert_syntax_error("  (56  ", "Unterminated list", 1, 3, 1, 3);
	lip_assert_syntax_error(
		"  (let ((x\n"
		"         (fn (x)\n"
		"          (if x true))))\n"
		"   () (x x))  ",
		"Empty list is invalid",
		4, 4, 4, 5
	);
	lip_assert_syntax_error(
		"  (let (x\n"
		"         (fn (x)\n"
		"          (if x true)))\n"
		"   () (x x))  ",
		"a binding must have the form: (<symbol> <expr>)",
		1, 9, 1, 9
	);
	lip_assert_syntax_error(
		"  (let x)  ",
		"'let' must have the form: (let (<bindings...>) <exp...>)",
		1, 3, 1, 9
	);

	return MUNIT_OK;
}

static MunitResult
utils(const MunitParameter params[], void* fixture_)
{
	(void)params;

	lip_fixture_t* fixture = fixture_;
	lip_vm_t* vm = fixture->vm;

	lip_assert_num(4.21, lip_make_number(vm, 4.21));
	lip_assert_nil(placeholder, lip_make_nil(vm));

	lip_value_t str1 = lip_make_string(vm, "1 - abc %s gh %d", "def", 3);
	lip_value_t str2 = lip_make_string_copy(vm, lip_string_ref("2 - abc wat gh 69"));
	lip_assert_str("1 - abc def gh 3", str1);
	lip_assert_str("2 - abc wat gh 69", str2);

	return MUNIT_OK;
}

static MunitResult
strings(const MunitParameter params[], void* fixture_)
{
	(void)params;

	lip_fixture_t* fixture = fixture_;
	lip_context_t* ctx = fixture->context;
	lip_vm_t* vm = fixture->vm;
	lip_load_builtins(ctx);

	lip_assert_str_result("\"abc\"", "abc");
	lip_assert_str_result("\"ab c \\n \" ", "ab c \n ");
	lip_assert_str_result("\"ab c \\\" \" ", "ab c \" ");
	lip_assert_str_result(
		"(identity \"start \\r \\n\\t\\\\\\a \\b\\f\\v stop\")",
		"start \r \n\t\\\a \b\f\v stop"
	);

	lip_assert_str_result("\"\\x23yay\"", "\x23yay");
	lip_assert_str_result("\"\\x6Fzz\"", "\x6fzz");
	lip_assert_str_result("\"\\xe\"", "\xe");
	lip_assert_str_result("\"\\xfg z\"", "\xfg z");

	lip_assert_str_result("\"\\060yay\"", "\60yay");
	lip_assert_str_result("\"\\178z\"", "\178z");
	lip_assert_str_result("\"\\17 z\"", "\17 z");
	lip_assert_str_result("\"\\15\"", "\15");

	lip_assert_syntax_error(
		"\" \\xhh\"",
		"Invalid hex escape sequence",
		1, 3, 1, 4
	);
	lip_assert_syntax_error(
		"\"  \\777 \"",
		"Invalid octal escape sequence",
		1, 4, 1, 7
	);
	lip_assert_syntax_error(
		"\" \\9 \"",
		"Invalid octal escape sequence",
		1, 3, 1, 4
	);
	lip_assert_syntax_error(
		"\" \\9\"",
		"Invalid octal escape sequence",
		1, 3, 1, 4
	);

	return MUNIT_OK;
}

static MunitResult
arity(const MunitParameter params[], void* fixture_)
{
	(void)params;

	lip_fixture_t* fixture = fixture_;
	lip_context_t* ctx = fixture->context;
	lip_vm_t* vm = fixture->vm;
	lip_load_builtins(ctx);

	lip_assert_error_msg(
		"((fn (x) x) 1 2)",
		"Bad number of arguments (exactly 1 expected, got 2)"
	);

	lip_assert_error_msg(
		"((fn (x &y) y))",
		"Bad number of arguments (at least 1 expected, got 0)"
	);

	lip_assert_num_result(
		"((fn (x &y) (list/len y)) 1 2 3)",
		2
	);

	lip_assert_num_result(
		"((fn (x &y) (list/nth 1 y)) 1 2 3)",
		3
	);

	lip_assert_num_result(
		"((fn (x &y) (list/nth 0 y)) 2 4 6)",
		4
	);

	lip_assert_syntax_error(
		"((fn (x y &y) y))",
		"Duplicated argument name",
		1, 11, 1, 12
	);

	lip_assert_syntax_error(
		"((fn (x &y z) y))",
		"Only last argument can be prefixed with '&'",
		1, 9, 1, 10
	);

	return MUNIT_OK;
}

static MunitResult
quotation(const MunitParameter params[], void* fixture_)
{
	(void)params;

	lip_fixture_t* fixture = fixture_;
	lip_context_t* ctx = fixture->context;
	lip_vm_t* vm = fixture->vm;
	lip_load_builtins(ctx);

	lip_assert_num_result("'3", 3);
	lip_assert_num_result("`3", 3);
	lip_assert_num_result("`,3", 3);

	lip_assert_str_result("'\"haha\"", "haha");
	lip_assert_str_result("`\"haha\"", "haha");

	lip_assert_symbol_result("'haha", "haha");
	lip_assert_symbol_result("`haha", "haha");

	lip_assert_boolean_result(
		"(let ((x 3) (y (list 3 5)) (l '(3 haha (\"hehe\" ,3)))) (list? l))",
		true
	);
	lip_assert_num_result(
		"(let ((x 3) (y (list 3 5)) (l '(3 haha (\"hehe\" ,3)))) (list/len l))",
		3
	);
	lip_assert_symbol_result(
		"(let ((x 3) (y (list 3 5)) (l '(2 haha (\"hehe\" ,3)))) (list/nth 1 l))",
		"haha"
	);
	lip_assert_str_result(
		"(let ((x 3) (y (list 3 5)) (l '(2 haha (\"hehe\" ,3)))) (list/nth 0 (list/nth 2 l)))",
		"hehe"
	);
	lip_assert_symbol_result(
		"(let ((x 3) (y (list 3 5)) (l '(2 haha (\"hehe\" ,3)))) (list/nth 0 (list/nth 1 (list/nth 2 l))))",
		"unquote"
	);
	lip_assert_num_result(
		"(let ((x 3) (y (list 3 5)) (l '(2 haha (\"hehe\" ,4.2)))) (list/nth 1 (list/nth 1 (list/nth 2 l))))",
		4.2
	);

	lip_assert_boolean_result(
		"(let ((x 3) (y (list 3 5)) (l `(3 haha (\"hehe\" ,3)))) (list? l))",
		true
	);
	lip_assert_num_result(
		"(let ((x 3) (y (list 3 5)) (l `(3 haha (\"hehe\" ,3)))) (list/len l))",
		3
	);
	lip_assert_symbol_result(
		"(let ((x 3) (y (list 3 5)) (l `(2 haha (\"hehe\" ,3)))) (list/nth 1 l))",
		"haha"
	);
	lip_assert_str_result(
		"(let ((x 3) (y (list 3 5)) (l `(2 haha (\"hehe\" ,3)))) (list/nth 0 (list/nth 2 l)))",
		"hehe"
	);
	lip_assert_num_result(
		"(let ((x 3) (y (list 3 5)) (l `(2 haha (\"hehe\" ,8.7)))) (list/nth 1 (list/nth 2 l)))",
		8.7
	);
	lip_assert_num_result(
		"(let ((x 3.4) (y (list 3 5)) (l `(2 haha (\"hehe\" ,x)))) (list/nth 1 (list/nth 2 l)))",
		3.4
	);
	lip_assert_num_result(
		"(let ((x 3.4) (y (list 3 5)) (l `(2 haha (\"hehe\" ,y)))) (list/nth 1 (list/nth 1 (list/nth 2 l))))",
		5
	);
	lip_assert_num_result(
		"(let ((x 3.4) (y (list 3 5)) (l `(2 haha (\"hehe\" ,@y)))) (list/nth 1 (list/nth 2 l)))",
		3
	);
	lip_assert_num_result(
		"(let ((x 3.4) (y (list 3 5)) (l `(2 haha (\"hehe\" ,@y)))) (list/nth 2 (list/nth 2 l)))",
		5
	);
	lip_assert_num_result(
		"(let ((x 3.4) (y (list 3 5)) (l `(2 haha (\"hehe\" ,@y)))) (list/len (list/nth 2 l)))",
		3
	);

	lip_assert_syntax_error(
		" (quote 3 4)",
		"'quote' must have the form: (quote <sexp>)",
		1, 2,
		1, 12
	);
	lip_assert_syntax_error(
		" (quasiquote 3 4)",
		"'quasiquote' must have the form: (quasiquote <sexp>)",
		1, 2,
		1, 17
	);

	lip_assert_syntax_error(
		" (wat ,3 4)",
		"Cannot unquote outside of quasiquote",
		1, 7,
		1, 8
	);
	lip_assert_syntax_error(
		" (wat ,@3 4)",
		"Cannot unquote-splicing outside of quasiquoted list",
		1, 7,
		1, 9
	);
	lip_assert_syntax_error(
		"`,@3",
		"Cannot unquote-splicing outside of quasiquoted list",
		1, 2,
		1, 4
	);
	lip_assert_syntax_error(
		"`( ,@34)",
		"The expression passed to unquote-splicing must evaluate to a list",
		1, 6,
		1, 7
	);
	lip_assert_syntax_error(
		"(quasiquote (unquote 1 1))",
		"'unquote' must have the form: (unquote <sexp>)",
		1, 13,
		1, 25
	);
	lip_assert_syntax_error(
		"(quasiquote ((unquote-splicing)))",
		"'unquote-splicing' must have the form: (unquote-splicing <sexp>)",
		1, 14,
		1, 31
	);

	return MUNIT_OK;
}

static MunitResult
userdata(const MunitParameter params[], void* fixture_)
{
	(void)params;

	lip_fixture_t* fixture = fixture_;
	lip_vm_t* vm = fixture->vm;

	lip_vm_t* vm2 = lip_create_vm(fixture->context, NULL);
	lip_context_t* ctx2 = lip_create_context(fixture->runtime, NULL);
	lip_vm_t* vm3 = lip_create_vm(ctx2,  NULL);;

	char userdata_key1, userdata_key2, userdata_vm1, userdata_vm2,
		 userdata_ctx1, userdata_ctx2, userdata_rt1, userdata_rt2;
	munit_assert_null(lip_get_userdata(vm, LIP_SCOPE_VM, &userdata_key1));
	munit_assert_null(lip_get_userdata(vm2, LIP_SCOPE_VM, &userdata_key1));
	munit_assert_null(lip_get_userdata(vm3, LIP_SCOPE_VM, &userdata_key1));
	munit_assert_null(lip_get_userdata(vm, LIP_SCOPE_CONTEXT, &userdata_key1));
	munit_assert_null(lip_get_userdata(vm2, LIP_SCOPE_CONTEXT, &userdata_key1));
	munit_assert_null(lip_get_userdata(vm3, LIP_SCOPE_CONTEXT, &userdata_key1));
	munit_assert_null(lip_get_userdata(vm, LIP_SCOPE_RUNTIME, &userdata_key1));
	munit_assert_null(lip_get_userdata(vm2, LIP_SCOPE_RUNTIME, &userdata_key1));
	munit_assert_null(lip_get_userdata(vm3, LIP_SCOPE_RUNTIME, &userdata_key1));
	munit_assert_null(lip_get_userdata(vm, LIP_SCOPE_VM, &userdata_key2));
	munit_assert_null(lip_get_userdata(vm2, LIP_SCOPE_VM, &userdata_key2));
	munit_assert_null(lip_get_userdata(vm3, LIP_SCOPE_VM, &userdata_key2));
	munit_assert_null(lip_get_userdata(vm, LIP_SCOPE_CONTEXT, &userdata_key2));
	munit_assert_null(lip_get_userdata(vm2, LIP_SCOPE_CONTEXT, &userdata_key2));
	munit_assert_null(lip_get_userdata(vm3, LIP_SCOPE_CONTEXT, &userdata_key2));
	munit_assert_null(lip_get_userdata(vm, LIP_SCOPE_RUNTIME, &userdata_key2));
	munit_assert_null(lip_get_userdata(vm2, LIP_SCOPE_RUNTIME, &userdata_key2));
	munit_assert_null(lip_get_userdata(vm3, LIP_SCOPE_RUNTIME, &userdata_key2));

	munit_assert_null(lip_set_userdata(vm, LIP_SCOPE_VM, &userdata_key1, &userdata_vm1));
	munit_assert_ptr(&userdata_vm1, ==, lip_get_userdata(vm, LIP_SCOPE_VM, &userdata_key1));
	munit_assert_ptr(
		&userdata_vm1,
		==,
		lip_set_userdata(vm, LIP_SCOPE_VM, &userdata_key1, &userdata_vm2)
	);
	munit_assert_ptr(&userdata_vm2, ==, lip_get_userdata(vm, LIP_SCOPE_VM, &userdata_key1));
	munit_assert_ptr(
		&userdata_vm2,
		==,
		lip_set_userdata(vm, LIP_SCOPE_VM, &userdata_key1, &userdata_vm1)
	);
	munit_assert_ptr(&userdata_vm1, ==, lip_get_userdata(vm, LIP_SCOPE_VM, &userdata_key1));
	munit_assert_null(lip_get_userdata(vm, LIP_SCOPE_CONTEXT, &userdata_key1));
	munit_assert_null(lip_get_userdata(vm, LIP_SCOPE_RUNTIME, &userdata_key1));
	munit_assert_null(lip_get_userdata(vm, LIP_SCOPE_CONTEXT, &userdata_key2));
	munit_assert_null(lip_get_userdata(vm, LIP_SCOPE_RUNTIME, &userdata_key2));

	munit_assert_null(lip_set_userdata(vm, LIP_SCOPE_CONTEXT, &userdata_key1, &userdata_ctx1));
	munit_assert_ptr(
		&userdata_ctx1,
		==,
		lip_set_userdata(vm, LIP_SCOPE_CONTEXT, &userdata_key1, &userdata_ctx2)
	);
	munit_assert_ptr(&userdata_ctx2, ==, lip_get_userdata(vm, LIP_SCOPE_CONTEXT, &userdata_key1));
	munit_assert_ptr(
		&userdata_ctx2,
		==,
		lip_set_userdata(vm, LIP_SCOPE_CONTEXT, &userdata_key1, &userdata_ctx1)
	);
	munit_assert_ptr(&userdata_ctx1, ==, lip_get_userdata(vm, LIP_SCOPE_CONTEXT, &userdata_key1));
	munit_assert_null(lip_get_userdata(vm, LIP_SCOPE_RUNTIME, &userdata_key1));
	munit_assert_null(lip_get_userdata(vm, LIP_SCOPE_RUNTIME, &userdata_key2));

	munit_assert_null(lip_set_userdata(vm, LIP_SCOPE_RUNTIME, &userdata_key1, &userdata_rt1));
	munit_assert_ptr(
		&userdata_rt1,
		==,
		lip_set_userdata(vm, LIP_SCOPE_RUNTIME, &userdata_key1, &userdata_rt2)
	);
	munit_assert_ptr(&userdata_rt2, ==, lip_get_userdata(vm, LIP_SCOPE_RUNTIME, &userdata_key1));
	munit_assert_ptr(
		&userdata_rt2,
		==,
		lip_set_userdata(vm, LIP_SCOPE_RUNTIME, &userdata_key1, &userdata_rt1)
	);
	munit_assert_ptr(&userdata_rt1, ==, lip_get_userdata(vm, LIP_SCOPE_RUNTIME, &userdata_key1));

	munit_assert_null(
		lip_set_userdata(vm2, LIP_SCOPE_VM, &userdata_key2, &userdata_vm2)
	);
	munit_assert_ptr(
		&userdata_vm2,
		==,
		lip_set_userdata(vm2, LIP_SCOPE_VM, &userdata_key2, &userdata_vm2)
	);
	munit_assert_null(lip_get_userdata(vm2, LIP_SCOPE_VM, &userdata_key1));
	munit_assert_null(lip_set_userdata(vm2, LIP_SCOPE_VM, &userdata_key1, &userdata_vm2));
	munit_assert_null(lip_get_userdata(vm3, LIP_SCOPE_VM, &userdata_key1));
	munit_assert_null(lip_get_userdata(vm3, LIP_SCOPE_VM, &userdata_key2));

	munit_assert_null(
		lip_set_userdata(vm2, LIP_SCOPE_CONTEXT, &userdata_key2, &userdata_ctx2)
	);
	munit_assert_ptr(
		&userdata_ctx1,
		==,
		lip_get_userdata(vm2, LIP_SCOPE_CONTEXT, &userdata_key1)
	);
	munit_assert_ptr(
		&userdata_ctx1,
		==,
		lip_get_userdata(vm, LIP_SCOPE_CONTEXT, &userdata_key1)
	);
	munit_assert_ptr(
		&userdata_ctx2,
		==,
		lip_get_userdata(vm2, LIP_SCOPE_CONTEXT, &userdata_key2)
	);
	munit_assert_ptr(
		&userdata_ctx2,
		==,
		lip_get_userdata(vm, LIP_SCOPE_CONTEXT, &userdata_key2)
	);
	munit_assert_null(
		lip_get_userdata(vm3, LIP_SCOPE_CONTEXT, &userdata_key1)
	);

	munit_assert_ptr(
		&userdata_rt1,
		==,
		lip_get_userdata(vm2, LIP_SCOPE_RUNTIME, &userdata_key1)
	);
	munit_assert_ptr(
		&userdata_rt1,
		==,
		lip_get_userdata(vm3, LIP_SCOPE_RUNTIME, &userdata_key1)
	);

	return MUNIT_OK;
}

static MunitTest tests[] = {
	{
		.name = "/basic_forms",
		.test = basic_forms,
		.setup = setup,
		.tear_down = teardown
	},
	{
		.name = "/syntax_error",
		.test = syntax_error,
		.setup = setup,
		.tear_down = teardown
	},
	{
		.name = "/runtime_error",
		.test = runtime_error,
		.setup = setup,
		.tear_down = teardown
	},
	{
		.name = "/builtins",
		.test = builtins,
		.setup = setup,
		.tear_down = teardown
	},
	{
		.name = "/prim_ops",
		.test = prim_ops,
		.setup = setup,
		.tear_down = teardown
	},
	{
		.name = "/utils",
		.test = utils,
		.setup = setup,
		.tear_down = teardown
	},
	{
		.name = "/strings",
		.test = strings,
		.setup = setup,
		.tear_down = teardown
	},
	{
		.name = "/arity",
		.test = arity,
		.setup = setup,
		.tear_down = teardown
	},
	{
		.name = "/quotation",
		.test = quotation,
		.setup = setup,
		.tear_down = teardown
	},
	{
		.name = "/userdata",
		.test = userdata,
		.setup = setup,
		.tear_down = teardown
	},
	{ .test = NULL }
};

MunitSuite runtime = {
	.prefix = "/runtime",
	.tests = tests
};
