#include <lip/array.h>
#include <lip/memory.h>
#include <emmintrin.h>
#include "munit.h"

static MunitResult
retention(const MunitParameter params[], void* fixture)
{
	(void)params;
	(void)fixture;

	lip_array(int) array = lip_array_create(lip_default_allocator, int, 2);
	munit_assert_size(0, ==, lip_array_len(array));

	for(int i = 0; i < 10000; ++i)
	{
		munit_assert_size(i, ==, lip_array_len(array));
		lip_array_push(array, i);
	}

	munit_assert_size(10000, ==, lip_array_len(array));

	for(int i = 0; i < 10000; ++i)
	{
		munit_assert_int(array[i], ==, i);
	}

	lip_array_destroy(array);

	return MUNIT_OK;
}

static MunitResult
alignment_sse(const MunitParameter params[], void* fixture)
{
	(void)params;
	(void)fixture;

	lip_array(__m128i) array = lip_array_create(lip_default_allocator, __m128i, 1);
	munit_assert_true((uintptr_t)array % LIP_ALIGN_OF(__m128i) == 0);

	for(int i = 0; i < 10000; ++i)
	{
		lip_array_push(array, _mm_set_epi32(i, i + 1, i + 2, i + 3));
		munit_assert_true((uintptr_t)array % LIP_ALIGN_OF(__m128i) == 0);
	}

	for(int i = 0; i < 10000; ++i)
	{
		__m128i expected = _mm_set_epi32(i, i + 1, i + 2, i + 3);
		munit_assert_memory_equal(sizeof(expected), &expected, &array[i]);
	}

	lip_array_destroy(array);

	return MUNIT_OK;
}

static MunitResult
alignment_long_double(const MunitParameter params[], void* fixture)
{
	(void)params;
	(void)fixture;

	lip_array(long double) array = lip_array_create(lip_default_allocator, long double, 1);
	munit_assert_true((uintptr_t)array % LIP_ALIGN_OF(long double) == 0);

	for(int i = 0; i < 10000; ++i)
	{
		lip_array_push(array, i);
		munit_assert_true((uintptr_t)array % LIP_ALIGN_OF(long double) == 0);
	}

	lip_array_destroy(array);

	return MUNIT_OK;
}

static MunitResult
quick_remove(const MunitParameter params[], void* fixture)
{
	(void)params;
	(void)fixture;

	uint32_t num_elements = munit_rand_uint32() % 1000;
	uint32_t deleted_element = munit_rand_uint32() % num_elements;

	lip_array(uint32_t) array = lip_array_create(lip_default_allocator, uint32_t, 0);

	for(uint32_t i = 0; i < num_elements; ++i)
	{
		lip_array_push(array, i);
	}

	munit_assert_size(num_elements, ==, lip_array_len(array));
	lip_array_quick_remove(array, deleted_element);
	munit_assert_size(num_elements - 1, ==, lip_array_len(array));

	for(uint32_t i = 0; i < num_elements; ++i)
	{
		bool present = false;

		for(uint32_t j = 0; j < lip_array_len(array); ++j)
		{
			if(array[j] == i)
			{
				present = true;
				break;
			}
		}

		if(i == deleted_element)
		{
			munit_assert_false(present);
		}
		else
		{
			munit_assert_true(present);
		}
	}

	lip_array_destroy(array);

	return MUNIT_OK;
}

static MunitTest tests[] = {
	{
		.name = "/retention",
		.test = retention
	},
	{
		.name = "/alignment/sse",
		.test = alignment_sse
	},
	{
		.name = "/alignment/long_double",
		.test = alignment_long_double
	},
	{
		.name = "/quick_remove",
		.test = quick_remove,
	},
	{ .test = NULL }
};

MunitSuite array = {
	.prefix = "/array",
	.tests = tests
};
