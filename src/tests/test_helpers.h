#ifndef LIP_TEST_HELPERS_H
#define LIP_TEST_HELPERS_H

#include <lip/common.h>
#include <lip/token.h>
#include <lip/runtime.h>
#include <lip/io.h>
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
		if (MUNIT_UNLIKELY(a_len_ != b_len_ || memcmp(a_, b_, a_len_) != 0)) { \
			munit_errorf( \
				"assertion failed: mem " #a " == " #b " (\"%.*s\" == \"%.*s\")", \
				(int)a_len_, a_, (int)b_len_, b_ \
			); \
		} \
	} while (0)

LIP_MAYBE_UNUSED static inline bool
lip_runtime_exec_string(
	lip_runtime_t* runtime,
	const char* string,
	lip_value_t* result
)
{
	struct lip_sstream_s sstream;

	return lip_runtime_exec(
		runtime,
		lip_make_sstream(lip_string_ref(string), &sstream),
		lip_string_ref(__func__),
		NULL,
		result
	);
}

#endif
