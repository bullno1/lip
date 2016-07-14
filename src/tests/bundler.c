#include <lip/bundler.h>
#include <lip/memory.h>
#include <lip/array.h>
#include <lip/ex/vm.h>
#include <stdarg.h>
#include <stdio.h>
#include "munit.h"
#include "test_helpers.h"

static void*
setup(const MunitParameter params[], void* data)
{
	(void)params;
	(void)data;

	return lip_bundler_create(lip_default_allocator);
}

static void
teardown(void* data)
{
	lip_bundler_destroy(data);
}

static MunitResult
empty(const MunitParameter params[], void* fixture)
{
	(void)params;

	lip_bundler_t* bundler = fixture;

	lip_bundler_begin(bundler, lip_string_ref("test-module"));
	lip_module_t* module = lip_bundler_end(bundler);

	munit_assert_uint32(0, ==, module->num_public_symbols);
	munit_assert_uint32(0, ==, module->num_private_symbols);
	munit_assert_uint32(0, ==, module->num_imports);
	munit_assert_uint32(0, ==, module->num_constants);
	lip_string_t* module_name = lip_module_get_name(module);
	munit_assert_size(strlen("test-module"), ==, module_name->length);
	munit_assert_memory_equal(module_name->length, "test-module", module_name->ptr);

	lip_free(lip_default_allocator, module);

	return MUNIT_OK;
}

#if defined(__GNUC__) || defined(__GNUG__) || defined(__clang__)
__attribute__((format(printf, 1, 2)))
#endif
static char*
alloc_string(char* format, ...)
{
	va_list args1, args2;
	va_start(args1, format);
	va_copy(args2, args1);
	int length = vsnprintf(NULL, 0, format, args1);
	va_end(args1);
	char* buff = lip_malloc(lip_default_allocator, length + 1);
	vsnprintf(buff, length + 1, format, args2);
	va_end(args2);

	return buff;
}

static MunitResult
normal(const MunitParameter params[], void* fixture)
{
	(void)params;

	lip_bundler_t* bundler = fixture;

	lip_bundler_begin(bundler, lip_string_ref("test-module"));

	lip_asm_t* lasm = lip_asm_create(lip_default_allocator);
	lip_asm_begin(lasm);
	lip_function_t* function = lip_asm_end(lasm);
	lip_asm_destroy(lasm);

	lip_array(char*) strings = lip_array_create(lip_default_allocator, char*, 0);

	uint32_t num_public_symbols = munit_rand_uint32() % 1000;
	uint32_t num_private_symbols = munit_rand_uint32() % 1000;
	uint32_t num_imports = munit_rand_uint32() % 1000;
	uint32_t num_constants = munit_rand_uint32() % 50;

	for(uint32_t i = 0; i < num_public_symbols; ++i)
	{
		char* name = alloc_string("public-symbol-%d", i);
		lip_array_push(strings, name);
		switch(i % 3)
		{
			case 0:
				lip_bundler_add_number(bundler, true, lip_string_ref(name), i);
				break;
			case 1:
				{
					char* string = alloc_string("string-%d", i);
					lip_array_push(strings, string);
					lip_bundler_add_string(
						bundler,
						true,
						lip_string_ref(name),
						lip_string_ref(string)
					);
				}
				break;
			case 2:
				lip_bundler_add_function(bundler, true, lip_string_ref(name), function);
				break;
		}
	}

	for(uint32_t i = 0; i < num_private_symbols; ++i)
	{
		char* name = alloc_string("private-symbol-%d", i);
		lip_array_push(strings, name);
		switch(i % 3)
		{
			case 0:
				lip_bundler_add_number(bundler, false, lip_string_ref(name), i);
				break;
			case 1:
				{
					char* string = alloc_string("string-%d", i);
					lip_array_push(strings, string);
					lip_bundler_add_string(
						bundler,
						false,
						lip_string_ref(name),
						lip_string_ref(string)
					);
				}
				break;
			case 2:
				lip_bundler_add_function(bundler, false, lip_string_ref(name), function);
				break;
		}
	}

	for(uint32_t i = 0; i < num_imports; ++i)
	{
		char* import = alloc_string("import-%d", i);
		lip_array_push(strings, import);
		lip_bundler_alloc_import(bundler, lip_string_ref(import));
	}

	for(uint32_t i = 0; i < num_constants; ++i)
	{
		switch(i % 2)
		{
			case 0:
				{
					char* string_const = alloc_string("import-%d", i);
					lip_array_push(strings, string_const);
					lip_bundler_alloc_string_constant(bundler, lip_string_ref(string_const));
				}
				break;
			case 1:
				lip_bundler_alloc_numeric_constant(bundler, i);
				break;
		}
	}

	lip_module_t* module = lip_bundler_end(bundler);

	lip_array_foreach(char*, string, strings)
	{
		lip_free(lip_default_allocator, *string);
	}
	lip_array_destroy(strings);

	munit_assert_uint32(num_public_symbols, ==, module->num_public_symbols);
	munit_assert_uint32(num_private_symbols, ==, module->num_private_symbols);
	munit_assert_uint32(num_imports, ==, module->num_imports);
	munit_assert_uint32(num_constants, ==, module->num_constants);

	lip_kv_t* public_symbols = lip_module_get_public_symbols(module);
	lip_assert_typed_alignment(public_symbols, lip_kv_t);
	for(uint32_t i = 0; i < num_public_symbols; ++i)
	{
		char expected_name[1024];
		snprintf(expected_name, sizeof(expected_name), "public-symbol-%d", i);

		lip_kv_t* symbol = &public_symbols[i];
		lip_string_t* symbol_name = lip_module_get_resource(module, symbol->key);
		lip_assert_alignment(symbol_name, lip_string_t_alignment);

		munit_assert_memory_equal(symbol_name->length, expected_name, symbol_name->ptr);

		switch(i % 3)
		{
			case 0:
				lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, symbol->value.type);
				munit_assert_double(i, ==, symbol->value.data.number);
				break;
			case 1:
				{
					char expected_value[1024];
					snprintf(expected_value, sizeof(expected_value), "string-%d", i);

					lip_assert_enum(lip_value_type_t, LIP_VAL_STRING, ==, symbol->value.type);
					lip_string_t* symbol_value = lip_module_get_resource(module, symbol->value.data.index);

					lip_assert_alignment(symbol_value, lip_string_t_alignment);
					munit_assert_memory_equal(symbol_value->length, expected_value, symbol_value->ptr);
				}
				break;
			case 2:
				{
					lip_assert_enum(lip_value_type_t, LIP_VAL_FUNCTION, ==, symbol->value.type);
					lip_function_t* symbol_value = lip_module_get_resource(module, symbol->value.data.index);
					lip_assert_alignment(symbol_value, lip_function_t_alignment);
					munit_assert_ptr(function, !=, symbol_value);
					munit_assert_size(function->size, ==, symbol_value->size);
					munit_assert_memory_equal(function->size, function, symbol_value);
				}
				break;
		}
	}

	lip_kv_t* private_symbols = lip_module_get_private_symbols(module);
	lip_assert_typed_alignment(private_symbols, lip_kv_t);
	for(uint32_t i = 0; i < num_private_symbols; ++i)
	{
		char expected_name[1024];
		snprintf(expected_name, sizeof(expected_name), "private-symbol-%d", i);

		lip_kv_t* symbol = &private_symbols[i];
		lip_string_t* symbol_name = lip_module_get_resource(module, symbol->key);
		lip_assert_alignment(symbol_name, lip_string_t_alignment);

		munit_assert_memory_equal(symbol_name->length, expected_name, symbol_name->ptr);

		switch(i % 3)
		{
			case 0:
				lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, symbol->value.type);
				munit_assert_double(i, ==, symbol->value.data.number);
				break;
			case 1:
				{
					char expected_value[1024];
					snprintf(expected_value, sizeof(expected_value), "string-%d", i);

					lip_assert_enum(lip_value_type_t, LIP_VAL_STRING, ==, symbol->value.type);
					lip_string_t* symbol_value = lip_module_get_resource(module, symbol->value.data.index);

					lip_assert_alignment(symbol_value, lip_string_t_alignment);
					munit_assert_memory_equal(symbol_value->length, expected_value, symbol_value->ptr);
				}
				break;
			case 2:
				{
					lip_assert_enum(lip_value_type_t, LIP_VAL_FUNCTION, ==, symbol->value.type);
					lip_function_t* symbol_value = lip_module_get_resource(module, symbol->value.data.index);
					lip_assert_alignment(symbol_value, lip_function_t_alignment);
					munit_assert_ptr(function, !=, symbol_value);
					munit_assert_size(function->size, ==, symbol_value->size);
					munit_assert_memory_equal(function->size, function, symbol_value);
				}
				break;
		}
	}

	lip_free(lip_default_allocator, function);
	lip_free(lip_default_allocator, module);

	return MUNIT_OK;
}

static MunitResult
dedupe(const MunitParameter params[], void* fixture)
{
	(void)params;

	lip_bundler_t* bundler = fixture;

	lip_bundler_begin(bundler, lip_string_ref("test-module"));

	{
		lip_asm_index_t a = lip_bundler_alloc_import(bundler, lip_string_ref("a"));
		lip_asm_index_t b = lip_bundler_alloc_import(bundler, lip_string_ref("b"));
		munit_assert_uint32(a, ==, lip_bundler_alloc_import(bundler, lip_string_ref("a")));
		lip_asm_index_t c = lip_bundler_alloc_import(bundler, lip_string_ref("c"));
		munit_assert_uint32(c, ==, lip_bundler_alloc_import(bundler, lip_string_ref("c")));

		munit_assert_uint32(0, ==, a);
		munit_assert_uint32(1, ==, b);
		munit_assert_uint32(2, ==, c);
	}

	{
		lip_asm_index_t a = lip_bundler_alloc_numeric_constant(bundler, 1.5);
		lip_asm_index_t b = lip_bundler_alloc_numeric_constant(bundler, 1.8);
		munit_assert_uint32(b, ==, lip_bundler_alloc_numeric_constant(bundler, 1.8));
		munit_assert_uint32(a, ==, lip_bundler_alloc_numeric_constant(bundler, 1.5));

		munit_assert_uint32(0, ==, a);
		munit_assert_uint32(1, ==, b);
	}

	{
		lip_asm_index_t a = lip_bundler_alloc_string_constant(bundler, lip_string_ref("a"));
		munit_assert_uint32(a, ==, lip_bundler_alloc_string_constant(bundler, lip_string_ref("a")));
		lip_asm_index_t b = lip_bundler_alloc_string_constant(bundler, lip_string_ref("b"));
		munit_assert_uint32(a, ==, lip_bundler_alloc_string_constant(bundler, lip_string_ref("a")));
		munit_assert_uint32(b, ==, lip_bundler_alloc_string_constant(bundler, lip_string_ref("b")));

		munit_assert_uint32(2, ==, a);
		munit_assert_uint32(3, ==, b);
	}

	lip_module_t* module = lip_bundler_end(bundler);

	munit_assert_uint32(3, ==, module->num_imports);
	munit_assert_uint32(4, ==, module->num_constants);

	lip_free(lip_default_allocator, module);

	return MUNIT_OK;
}

static MunitTest tests[] = {
	{
		.name = "/empty",
		.test = empty,
		.setup = setup,
		.tear_down = teardown
	},
	{
		.name = "/normal",
		.test = normal,
		.setup = setup,
		.tear_down = teardown
	},
	{
		.name = "/dedupe",
		.test = dedupe,
		.setup = setup,
		.tear_down = teardown
	},
	{ .test = NULL }
};

MunitSuite bundler = {
	.prefix = "/bundler",
	.tests = tests
};
