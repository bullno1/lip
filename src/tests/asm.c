#include <lip/asm.h>
#include <lip/memory.h>
#include <lip/ex/vm.h>
#include <lip/ex/asm.h>
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

	lip_asm_t* lasm = fixture;
	lip_asm_begin(lasm);
	lip_function_t* function = lip_asm_end(lasm);

	munit_assert_size(0, ==, function->num_functions);
	munit_assert_size(0, ==, function->num_instructions);
	munit_assert_size(0, ==, function->num_locals);

	lip_free(lip_default_allocator, function);

	return MUNIT_OK;
}

static MunitResult
normal(const MunitParameter params[], void* fixture)
{
	(void)params;

	lip_asm_t* lasm = fixture;
	lip_asm_begin(lasm);

	lip_asm_index_t num_locals = munit_rand_uint32() % 20;
	for(lip_asm_index_t i = 0; i < num_locals; ++i)
	{
		munit_assert_uint(i + 1, ==, lip_asm_new_local(lasm));
	}

	lip_asm_index_t num_functions = munit_rand_uint32() % 20;
	for(lip_asm_index_t i = 0; i < num_functions; ++i)
	{
		munit_assert_uint(i, ==, lip_asm_new_function(lasm, (void*)(uintptr_t)i));
	}

	lip_asm_index_t num_instructions = munit_rand_uint32() % 2000;
	for(lip_asm_index_t i = 0; i < num_instructions; ++i)
	{
		lip_asm_add(lasm, LIP_OP_NOP, i, (lip_loc_range_t) {
			.start = { .line = i, .column = i},
			.end = { .line = i, .column = i + 1}
		});
	}

	lip_function_t* function = lip_asm_end(lasm);

	munit_assert_size(num_locals, ==, function->num_locals);
	munit_assert_size(num_functions, ==, function->num_functions);
	munit_assert_size(num_instructions, ==, function->num_instructions);

	for(lip_asm_index_t i = 0; i < num_functions; ++i)
	{
		munit_assert_ptr((void*)(uintptr_t)i, ==, function->functions[i]);
	}

	for(lip_asm_index_t i = 0; i < num_instructions; ++i)
	{
		lip_opcode_t opcode;
		lip_operand_t operand;
		lip_disasm(function->instructions[i], &opcode, &operand);

		munit_assert_uint(i, ==, operand);
		lip_assert_enum(lip_opcode_t, LIP_OP_NOP, ==, opcode);

		lip_loc_range_t location = {
			.start = { .line = i, .column = i},
			.end = { .line = i, .column = i + 1}
		};
		lip_assert_loc_range_equal(location, function->locations[i]);
	}

	munit_assert_ptr(function, <=, function->functions);
	munit_assert_ptr(function, <=, function->instructions);
	munit_assert_ptr(function, <=, function->locations);

	lip_memblock_info_t layout = lip_function_layout(function);
	void* function_end = (char*)function + layout.num_elements;
	munit_assert_ptr(function->functions + num_functions, <=, function_end);
	munit_assert_ptr(function->instructions + num_instructions, <=, function_end);
	munit_assert_ptr(function->locations + num_instructions, <=, function_end);

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
	lip_asm_index_t* locations
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

	lip_function_t* function = lip_asm_end(lasm);

	munit_assert_size(num_instr_after, ==, function->num_instructions);
	for(lip_asm_index_t i = 0; i < function->num_instructions; ++i)
	{
		lip_opcode_t opcode1, opcode2;
		lip_operand_t operand1, operand2;

		lip_disasm(instr_after[i], &opcode1, &operand1);
		lip_disasm(function->instructions[i], &opcode2, &operand2);

		lip_assert_enum(lip_opcode_t, opcode1, ==, opcode2);
		munit_assert_int32(operand1, ==, operand2);
		munit_assert_uint(locations[i], ==, function->locations[i].start.line);
	}
}

static MunitResult
jump(const MunitParameter params[], void* fixture)
{
	(void)params;

	lip_asm_t* lasm = fixture;
	lip_asm_begin(lasm);

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
	lip_asm_begin(lasm);

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
tail_call(const MunitParameter params[], void* fixture)
{
	(void)params;

	lip_asm_t* lasm = fixture;
	lip_asm_begin(lasm);

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
