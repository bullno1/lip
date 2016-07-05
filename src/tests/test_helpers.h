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

#endif
