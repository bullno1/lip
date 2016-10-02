#include <lip/asm.h>
#include <lip/memory.h>
#include <lip/array.h>
#include <lip/ex/vm.h>
#include <lip/ex/asm.h>
#include <lip/utils.h>
#include <lip/ex/temp_allocator.h>
#include "munit.h"
#include "test_helpers.h"

static void*
setup(const MunitParameter params[], void* data)
{
	(void)params;
	(void)data;

	return lip_asm_create(lip_default_allocator);
}

static void
teardown(void* data)
{
	lip_asm_destroy(data);
}

static MunitResult
empty(const MunitParameter params[], void* fixture)
{
	(void)params;

	lip_allocator_t* temp_allocator = lip_temp_allocator_create(lip_default_allocator);
	lip_asm_t* lasm = fixture;
	lip_asm_begin(lasm, lip_string_ref(__func__));
	lip_function_t* function = lip_asm_end(lasm, temp_allocator);

	munit_assert_size(0, ==, function->num_functions);
	munit_assert_size(0, ==, function->num_instructions);
	munit_assert_size(0, ==, function->num_locals);
	lip_temp_allocator_t* allocator =
		LIP_CONTAINER_OF(temp_allocator, lip_temp_allocator_t, vtable);
	munit_assert_ptr_equal(allocator->mem, function);

	lip_free(temp_allocator, function);
	lip_temp_allocator_destroy(temp_allocator);

	return MUNIT_OK;
}

static MunitResult
dedupe(const MunitParameter params[], void* fixture)
{
	(void)params;

	lip_asm_t* lasm = fixture;
	lip_asm_begin(lasm, lip_string_ref(__func__));

	lip_asm_index_t import_foo = lip_asm_alloc_import(lasm, lip_string_ref("foo"));
	lip_asm_index_t import_bar = lip_asm_alloc_import(lasm, lip_string_ref("bar"));
	lip_asm_index_t import_foo2 = lip_asm_alloc_import(lasm, lip_string_ref("foo"));

	lip_asm_index_t const_foo = lip_asm_alloc_string_constant(lasm, lip_string_ref("foo"));
	lip_asm_index_t const_wat = lip_asm_alloc_string_constant(lasm, lip_string_ref("wat"));
	lip_asm_index_t const_wat2 = lip_asm_alloc_string_constant(lasm, lip_string_ref("wat"));

	lip_asm_index_t const_one = lip_asm_alloc_numeric_constant(lasm, 1.0);
	lip_asm_index_t const_two = lip_asm_alloc_numeric_constant(lasm, 2.2);
	lip_asm_index_t const_one2 = lip_asm_alloc_numeric_constant(lasm, 1.0);

	munit_assert_uint32(0, ==, import_foo);
	munit_assert_uint32(1, ==, import_bar);
	munit_assert_uint32(0, ==, import_foo2);

	munit_assert_uint32(0, ==, const_foo);
	munit_assert_uint32(1, ==, const_wat);
	munit_assert_uint32(1, ==, const_wat2);

	munit_assert_uint32(2, ==, const_one);
	munit_assert_uint32(3, ==, const_two);
	munit_assert_uint32(2, ==, const_one2);

	lip_function_t* function = lip_asm_end(lasm, lip_default_allocator);

	lip_function_layout_t function_layout;
	lip_function_layout(function, &function_layout);

	{
		lip_string_t* foo  = lip_function_resource(
			function, function_layout.imports[import_foo].name
		);
		lip_string_t* bar  = lip_function_resource(
			function, function_layout.imports[import_bar].name
		);
		lip_assert_mem_equal("foo", strlen("foo"), foo->ptr, foo->length);
		lip_assert_mem_equal("bar", strlen("bar"), bar->ptr, bar->length);
	}
	{
		lip_string_t* foo = lip_function_resource(
			function, function_layout.constants[const_foo].data.index
		);
		lip_string_t* wat = lip_function_resource(
			function, function_layout.constants[const_wat].data.index
		);
		lip_assert_mem_equal("foo", strlen("foo"), foo->ptr, foo->length);
		lip_assert_mem_equal("wat", strlen("bar"), wat->ptr, wat->length);
	}
	munit_assert_double(1.0, ==, function_layout.constants[const_one].data.number);
	munit_assert_double(2.2, ==, function_layout.constants[const_two].data.number);

	lip_free(lip_default_allocator, function);

	return MUNIT_OK;
}

static MunitResult
normal(const MunitParameter params[], void* fixture)
{
	(void)params;

	lip_asm_t* lasm = fixture;
	lip_asm_begin(lasm, lip_string_ref(__func__));

	lip_asm_index_t num_locals = munit_rand_uint32() % 20;
	for(lip_asm_index_t i = 0; i < num_locals; ++i)
	{
		munit_assert_uint(i + 1, ==, lip_asm_new_local(lasm));
	}

	lip_asm_index_t num_functions = munit_rand_uint32() % 20;
	lip_asm_t* lasm2 = lip_asm_create(lip_default_allocator);
	lip_loc_range_t dummy_loc;
	lip_array(lip_function_t*) nested_functions =
		lip_array_create(lip_default_allocator, lip_function_t*, num_functions);
	memset(&dummy_loc, 0, sizeof(lip_loc_range_t));
	for(lip_asm_index_t i = 0; i < num_functions; ++i)
	{
		lip_asm_begin(lasm2, lip_string_ref("nested"));
		lip_asm_add(lasm2, LIP_OP_NOP, i, dummy_loc);
		lip_function_t* nested_function = lip_asm_end(lasm2, lip_default_allocator);

		lip_function_layout_t function_layout;
		lip_function_layout(nested_function, &function_layout);
		lip_opcode_t opcode;
		lip_operand_t operand;
		lip_disasm(function_layout.instructions[0], &opcode, &operand);
		lip_assert_enum(lip_opcode_t, LIP_OP_NOP, ==, opcode);

		lip_array_push(nested_functions, nested_function);
		munit_assert_uint(i, ==, lip_asm_new_function(lasm, nested_function));
	}
	lip_asm_destroy(lasm2);

	lip_asm_index_t num_instructions = munit_rand_uint32() % 2000;
	for(lip_asm_index_t i = 0; i < num_instructions; ++i)
	{
		lip_asm_add(lasm, LIP_OP_NOP, i, (lip_loc_range_t) {
			.start = { .line = i, .column = i},
			.end = { .line = i, .column = i + 1}
		});
	}

	lip_function_t* function = lip_asm_end(lasm, lip_default_allocator);
	lip_array_foreach(lip_function_t*, function, nested_functions)
	{
		lip_free(lip_default_allocator, *function);
	}
	lip_array_destroy(nested_functions);

	munit_assert_size(num_locals, ==, function->num_locals);
	munit_assert_size(0, ==, function->num_constants);
	munit_assert_size(0, ==, function->num_imports);
	munit_assert_size(num_functions, ==, function->num_functions);
	munit_assert_size(num_instructions, ==, function->num_instructions);

	lip_function_layout_t function_layout;
	lip_function_layout(function, &function_layout);

	lip_assert_mem_equal(
		__func__, strlen(__func__),
		function_layout.source_name->ptr, function_layout.source_name->length);

	for(lip_asm_index_t i = 0; i < num_functions; ++i)
	{
		lip_function_t* nested_function =
			lip_function_resource(function, function_layout.function_offsets[i]);
		munit_assert_ptr(function, <, nested_function);
		munit_assert_ptr((char*)nested_function + nested_function->size, <=, (char*)function + function->size);
		lip_assert_alignment(nested_function, lip_function_t_alignment);

		lip_function_layout_t nested_layout;
		lip_function_layout(nested_function, &nested_layout);
		lip_assert_mem_equal(
			"nested", strlen("nested"),
			nested_layout.source_name->ptr, nested_layout.source_name->length
		);

		munit_assert_size(1, ==, nested_function->num_instructions);
		munit_assert_size(0, ==, nested_function->num_locals);
		munit_assert_size(0, ==, nested_function->num_functions);

		lip_opcode_t opcode;
		lip_operand_t operand;
		lip_disasm(nested_layout.instructions[0], &opcode, &operand);
		lip_assert_typed_alignment(nested_layout.instructions, lip_instruction_t);

		lip_assert_enum(lip_opcode_t, LIP_OP_NOP, ==, opcode);
		munit_assert_int32(i, ==, operand);
	}

	munit_assert_ptr(function, <, function_layout.instructions);
	munit_assert_ptr(function_layout.instructions + function->num_instructions, <=, (char*)function + function->size);
	lip_assert_typed_alignment(function_layout.instructions, lip_instruction_t);

	munit_assert_ptr(function, <, function_layout.locations);
	munit_assert_ptr(function_layout.locations + function->num_instructions, <=, (char*)function + function->size);
	lip_assert_typed_alignment(function_layout.locations, lip_loc_range_t);

	for(lip_asm_index_t i = 0; i < num_instructions; ++i)
	{
		lip_opcode_t opcode;
		lip_operand_t operand;
		lip_disasm(function_layout.instructions[i], &opcode, &operand);

		munit_assert_uint(i, ==, operand);
		lip_assert_enum(lip_opcode_t, LIP_OP_NOP, ==, opcode);

		lip_loc_range_t location = {
			.start = { .line = i, .column = i},
			.end = { .line = i, .column = i + 1}
		};
		lip_assert_loc_range_equal(location, function_layout.locations[i]);
	}

	lip_free(lip_default_allocator, function);

	return MUNIT_OK;
}

#define lip_assert_asm(LASM, BEFORE, AFTER, LOCATIONS) \
	lip_assert_asm_( \
		LASM, \
		LIP_STATIC_ARRAY_LEN(BEFORE), BEFORE, \
		LIP_STATIC_ARRAY_LEN(AFTER), AFTER, \
		LOCATIONS \
	)

static void
lip_assert_asm_(
	lip_asm_t* lasm,
	size_t num_instr_before, lip_instruction_t* instr_before,
	size_t num_instr_after, lip_instruction_t* instr_after,
	lip_asm_index_t* expected_locations
)
{
	lip_loc_range_t location;
	memset(&location, 0, sizeof(lip_loc_range_t));

	for(size_t i = 0; i < num_instr_before; ++i)
	{
		lip_opcode_t opcode;
		lip_operand_t operand;
		lip_disasm(instr_before[i], &opcode, &operand);
		location.start.line = i;
		lip_asm_add(lasm, opcode, operand, location);
	}

	lip_function_t* function = lip_asm_end(lasm, lip_default_allocator);

	lip_function_layout_t function_layout;
	lip_function_layout(function, &function_layout);

	munit_assert_size(num_instr_after, ==, function->num_instructions);
	for(lip_asm_index_t i = 0; i < function->num_instructions; ++i)
	{
		lip_opcode_t opcode1, opcode2;
		lip_operand_t operand1, operand2;

		lip_disasm(instr_after[i], &opcode1, &operand1);
		lip_disasm(function_layout.instructions[i], &opcode2, &operand2);

		lip_assert_enum(lip_opcode_t, opcode1, ==, opcode2);
		munit_assert_int32(operand1, ==, operand2);
		munit_assert_uint(expected_locations[i], ==, function_layout.locations[i].start.line);
	}

	lip_free(lip_default_allocator, function);
}

static MunitResult
jump(const MunitParameter params[], void* fixture)
{
	(void)params;

	lip_asm_t* lasm = fixture;
	lip_asm_begin(lasm, lip_string_ref(__func__));

	lip_asm_index_t label = lip_asm_new_label(lasm);
	lip_asm_index_t label2 = lip_asm_new_label(lasm);

	lip_instruction_t before[] = {
		lip_asm(LIP_OP_LDI, 7),
		lip_asm(LIP_OP_JOF, label),
		lip_asm(LIP_OP_NOP, 0),
		lip_asm(LIP_OP_NOP, 1),
		lip_asm(LIP_OP_LABEL, label2),
		lip_asm(LIP_OP_NOP, 2),
		lip_asm(LIP_OP_LABEL, label),
		lip_asm(LIP_OP_JMP, label2)
	};

	lip_instruction_t after[] = {
		lip_asm(LIP_OP_LDI, 7),
		lip_asm(LIP_OP_JOF, 5),
		lip_asm(LIP_OP_NOP, 0),
		lip_asm(LIP_OP_NOP, 1),
		lip_asm(LIP_OP_NOP, 2),
		lip_asm(LIP_OP_JMP, 4)
	};

	lip_asm_index_t locations[] = {
		0,
		1,
		2,
		3,
		5,
		7
	};

	lip_assert_asm(lasm, before, after, locations);

	return MUNIT_OK;
}

static MunitResult
short_circuit(const MunitParameter params[], void* fixture)
{
	(void)params;

	lip_asm_t* lasm = fixture;
	lip_asm_begin(lasm, lip_string_ref(__func__));

	lip_asm_index_t label = lip_asm_new_label(lasm);

	lip_instruction_t before[] = {
		lip_asm(LIP_OP_JOF, label),
		lip_asm(LIP_OP_JMP, label),
		lip_asm(LIP_OP_NOP, 0),
		lip_asm(LIP_OP_LABEL, label),
		lip_asm(LIP_OP_RET, 0),
	};

	lip_instruction_t after[] = {
		lip_asm(LIP_OP_JOF, 3),
		lip_asm(LIP_OP_RET, 0),
		lip_asm(LIP_OP_NOP, 0),
		lip_asm(LIP_OP_RET, 0)
	};

	lip_asm_index_t locations[] = {
		0,
		1,
		2,
		4
	};

	lip_assert_asm(lasm, before, after, locations);

	return MUNIT_OK;
}

static MunitResult
nil_pop(const MunitParameter params[], void* fixture)
{
	(void)params;

	lip_asm_t* lasm = fixture;
	{
		lip_asm_begin(lasm, lip_string_ref(__func__));

		lip_instruction_t before[] = {
			lip_asm(LIP_OP_NIL, 0),
			lip_asm(LIP_OP_POP, 1)
		};

		lip_instruction_t after[] = {
			lip_asm(LIP_OP_NIL, 0),
			lip_asm(LIP_OP_POP, 1)
		};

		lip_asm_index_t locations[] = {
			0,
			1
		};

		lip_assert_asm(lasm, before, after, locations);
	}

	{
		lip_asm_begin(lasm, lip_string_ref(__func__));

		lip_instruction_t before[] = {
			lip_asm(LIP_OP_NIL, 0),
			lip_asm(LIP_OP_POP, 1),
			lip_asm(LIP_OP_NOP, 0)
		};

		lip_instruction_t after[] = {
			lip_asm(LIP_OP_NOP, 0)
		};

		lip_asm_index_t locations[] = {
			2
		};

		lip_assert_asm(lasm, before, after, locations);
	}

	return MUNIT_OK;
}

static MunitResult
tail_call(const MunitParameter params[], void* fixture)
{
	(void)params;

	lip_asm_t* lasm = fixture;
	lip_asm_begin(lasm, lip_string_ref(__func__));

	lip_asm_index_t label = lip_asm_new_label(lasm);

	lip_instruction_t before[] = {
		lip_asm(LIP_OP_CALL, 1),
		lip_asm(LIP_OP_JMP, label),
		lip_asm(LIP_OP_CALL, 2),
		lip_asm(LIP_OP_RET, 0),
		lip_asm(LIP_OP_CALL, 3),
		lip_asm(LIP_OP_LABEL, label),
		lip_asm(LIP_OP_RET, 0)
	};

	lip_instruction_t after[] = {
		lip_asm(LIP_OP_TAIL, 1),
		lip_asm(LIP_OP_TAIL, 2),
		lip_asm(LIP_OP_TAIL, 3),
		lip_asm(LIP_OP_RET, 0)
	};

	lip_asm_index_t locations[] = {
		0,
		2,
		4,
		6
	};

	lip_assert_asm(lasm, before, after, locations);

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
		.name = "/dedupe",
		.test = dedupe,
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
		.name = "/jump",
		.test = jump,
		.setup = setup,
		.tear_down = teardown
	},
	{
		.name = "/short_circuit",
		.test = short_circuit,
		.setup = setup,
		.tear_down = teardown
	},
	{
		.name = "/nil_pop",
		.test = nil_pop,
		.setup = setup,
		.tear_down = teardown
	},
	{
		.name = "/tail_call",
		.test = tail_call,
		.setup = setup,
		.tear_down = teardown
	},
	{ .test = NULL }
};

MunitSuite assembler = {
	.prefix = "/asm",
	.tests = tests
};
