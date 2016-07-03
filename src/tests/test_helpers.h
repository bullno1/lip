#ifndef LIP_TEST_HELPERS_H
#define LIP_TEST_HELPERS_H

#include <lip/common.h>
#include <lip/token.h>
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

LIP_MAYBE_UNUSED static inline void
lip_assert_loc_equal(lip_loc_t lhs, lip_loc_t rhs)
{
	munit_assert_uint(lhs.column, ==, rhs.column);
	munit_assert_uint(lhs.line, ==, rhs.line);
}

LIP_MAYBE_UNUSED static inline void
lip_assert_loc_range_equal(lip_loc_range_t lhs, lip_loc_range_t rhs)
{
	lip_assert_loc_equal(lhs.start, rhs.start);
	lip_assert_loc_equal(lhs.end, rhs.end);
}

LIP_MAYBE_UNUSED static inline void
lip_assert_token_equal(lip_token_t lhs, lip_token_t rhs)
{
	lip_assert_enum(lip_token_type_t, lhs.type, ==, rhs.type);
	lip_assert_loc_range_equal(lhs.location, rhs.location);
	lip_assert_string_ref_equal(lhs.lexeme, rhs.lexeme);
}

LIP_MAYBE_UNUSED static inline void
lip_assert_error_equal(lip_error_t lhs, lip_error_t rhs)
{
	munit_assert_uint(lhs.code, ==, rhs.code);
	lip_assert_loc_range_equal(lhs.location, rhs.location);
}

#endif
