#ifndef LIP_TEST_HELPERS_H
#define LIP_TEST_HELPERS_H

#include <lip/common.h>
#include <lip/io.h>
#include <lip/asm.h>
#include <lip/parser.h>
#include <lip/lexer.h>
#include "munit.h"

#define lip_assert_enum(ENUM_TYPE, EXPECTED, OP, ACTUAL) \
	do { \
		unsigned int expected = (EXPECTED); \
		unsigned int actual = (ACTUAL); \
		if(!(expected OP actual)) { \
			munit_errorf( \
				"assert failed: " #EXPECTED " " #OP " " #ACTUAL " (%s " #OP " %s)", \
				ENUM_TYPE##_to_str(expected), ENUM_TYPE##_to_str(actual) \
			); \
		} \
	} while (0)

#define lip_assert_string_ref_equal(EXPECTED, ACTUAL) \
	do { \
		lip_string_ref_t expected = (EXPECTED); \
		lip_string_ref_t actual = (ACTUAL); \
		if(!lip_string_ref_equal(expected, actual)) { \
			munit_errorf( \
				"assert failed: " #EXPECTED " == " #ACTUAL " (\"%.*s\" == \"%.*s\")", \
				(int)expected.length, expected.ptr, \
				(int)actual.length, actual.ptr \
			); \
		} \
	} while (0)

#define lip_assert_string_equal(EXPECTED, ACTUAL) \
	do { \
		lip_string_ref_t expected = (EXPECTED); \
		lip_string_t* actual = (ACTUAL); \
		lip_string_ref_t actual_ref = { \
			.length = actual->length, .ptr = actual->ptr \
		}; \
		if(!lip_string_ref_equal(expected, actual_ref)) { \
			munit_errorf( \
				"assert failed: " #EXPECTED " == " #ACTUAL " (\"%.*s\" == \"%.*s\")", \
				(int)expected.length, expected.ptr, \
				(int)actual_ref.length, actual_ref.ptr \
			); \
		} \
	} while (0)

#define lip_assert_loc_equal(EXPECTED, ACTUAL) \
	do { \
		lip_loc_t loc_expected = (EXPECTED); \
		lip_loc_t loc_actual = (ACTUAL); \
		munit_assert_uint(loc_expected.column, ==, loc_actual.column); \
		munit_assert_uint(loc_expected.line, ==, loc_actual.line); \
	} while (0)

#define lip_assert_loc_range_equal(EXPECTED, ACTUAL) \
	do { \
		lip_loc_range_t loc_range_expected = (EXPECTED); \
		lip_loc_range_t loc_range_actual = (ACTUAL); \
		lip_assert_loc_equal(loc_range_expected.start, loc_range_actual.start); \
		lip_assert_loc_equal(loc_range_expected.end, loc_range_actual.end); \
	} while(0)

#define lip_assert_error_equal(EXPECTED, ACTUAL) \
	do { \
		lip_error_t error_expected = (EXPECTED); \
		lip_error_t error_actual = (ACTUAL); \
		munit_assert_uint(error_expected.code, ==, error_actual.code); \
		lip_assert_loc_range_equal(error_expected.location, error_actual.location); \
	} while(0)

#define lip_assert_typed_alignment(POINTER, TYPE) \
	lip_assert_alignment(POINTER, LIP_ALIGN_OF(TYPE))

#define lip_assert_alignment(POINTER, ALIGNMENT) \
	do { \
		void* ptr = (POINTER); \
		uint32_t alignment = (ALIGNMENT); \
		uint32_t rem = (uintptr_t)ptr % alignment; \
		if(!(rem == 0)) { \
			munit_errorf( \
				"assert failed: " #POINTER " (%p) is not aligned to " #ALIGNMENT " (%d bytes), remainder: %d", \
				ptr, alignment, rem \
			); \
		} \
	} while(0)

#define lip_assert_mem_equal(a, a_len, b, b_len) \
	do { \
		const char* a_ = a; \
		size_t a_len_ = a_len; \
		const char* b_ = b; \
		size_t b_len_ = b_len; \
		munit_assert_size(a_len_, ==, b_len_); \
		munit_assert_memory_equal(a_len_, a_, b_); \
	} while (0)

#define LIP_CONSTRUCTOR_AND_DESTRUCTOR(T) \
	LIP_MAYBE_UNUSED static inline T##_t* \
	T##_create(lip_allocator_t* allocator) { \
		T##_t* instance = lip_new(allocator, T##_t); \
		T##_init(instance, allocator); \
		return instance; \
	} \
	LIP_MAYBE_UNUSED static inline void \
	T##_destroy(T##_t* instance) { \
		T##_cleanup(instance); \
		lip_free(instance->allocator, instance); \
	}

#define lip_assert_num(expected, actual) \
	do { \
		lip_assert_enum(lip_value_type_t, LIP_VAL_NUMBER, ==, actual.type); \
		munit_assert_double_equal(expected, actual.data.number, 4); \
	} while(0)

#define lip_assert_str(expected, actual) \
	do { \
		lip_assert_enum(lip_value_type_t, LIP_VAL_STRING, ==, actual.type); \
		lip_string_t* returned_str = actual.data.reference; \
		munit_assert_string_equal(expected, returned_str->ptr); \
	} while(0)

#define lip_assert_symbol(expected, actual) \
	do { \
		lip_assert_enum(lip_value_type_t, LIP_VAL_SYMBOL, ==, actual.type); \
		lip_string_t* returned_str = actual.data.reference; \
		munit_assert_string_equal(expected, returned_str->ptr); \
	} while(0)

#define lip_assert_boolean(expected, actual) \
	do { \
		lip_assert_enum(lip_value_type_t, LIP_VAL_BOOLEAN, ==, actual.type); \
		munit_assert_int(expected, ==, actual.data.boolean); \
	} while(0)

#define lip_assert_nil(placeholder, actual) \
	lip_assert_enum(lip_value_type_t, LIP_VAL_NIL, ==, actual.type); \

static size_t
lip_null_write(const void* buff, size_t size, lip_out_t* vtable)
{
	(void)buff;
	(void)vtable;

	return size;
}

static lip_out_t null_ = { .write = lip_null_write };

static lip_out_t* const lip_null = &null_;

LIP_CONSTRUCTOR_AND_DESTRUCTOR(lip_lexer)
LIP_CONSTRUCTOR_AND_DESTRUCTOR(lip_parser)
LIP_CONSTRUCTOR_AND_DESTRUCTOR(lip_asm)

#endif
