#include <lip/arena_allocator.h>
#include <lip/memory.h>
#include <lip/array.h>
#include "munit.h"
#include "test_helpers.h"

typedef struct tracking_allocator_s tracking_allocator_t;
typedef struct fixture_s fixture_t;

struct tracking_allocator_s
{
	lip_allocator_t vtable;
	int count;
};

struct fixture_s
{
	tracking_allocator_t base_allocator;
};

static void*
tracked_realloc(lip_allocator_t* self, void* old, size_t size)
{
	tracking_allocator_t* allocator = (tracking_allocator_t*)self;
	if(old == NULL) { ++(allocator->count); }

	void* mem = realloc(old, size);

	munit_logf(
		MUNIT_LOG_INFO,
		"tracked_realloc: Allocated %zu bytes at %p", size, mem
	);

	return mem;
}

static void
tracked_free(lip_allocator_t* self, void* mem)
{
	tracking_allocator_t* allocator = (tracking_allocator_t*)self;
	if(mem) { --(allocator->count); }

	free(mem);

	munit_logf(
		MUNIT_LOG_INFO,
		"tracked_free: Freed %p", mem
	);
}

static void*
setup(const MunitParameter params[], void* data)
{
	(void)params;
	(void)data;

	fixture_t* fixture = malloc(sizeof(fixture_t));
	fixture->base_allocator.vtable.realloc = tracked_realloc;
	fixture->base_allocator.vtable.free = tracked_free;
	fixture->base_allocator.count = 0;

	return fixture;
}

static void
teardown(void* data)
{
	fixture_t* fixture = data;
	munit_assert_int(0, ==, fixture->base_allocator.count);
	free(fixture);
}

static MunitResult
no_leak(const MunitParameter params[], void* fixture_)
{
	(void)params;
	fixture_t* fixture = fixture_;
	lip_allocator_t* allocator = lip_arena_allocator_create(
		&fixture->base_allocator.vtable,
		128,
		false
	);

	lip_array(void*) pointers = lip_array_create(
		lip_default_allocator,
		void*,
		0
	);
	int num_rounds = munit_rand_int_range(0, 50);
	for(int i = 0; i < num_rounds; ++i)
	{
		lip_arena_allocator_reset(allocator);
		lip_array_clear(pointers);

		int num_small_allocs = munit_rand_int_range(0, 30);
		int num_large_allocs = munit_rand_int_range(0, 20);
		munit_logf(
			MUNIT_LOG_INFO,
			"Allocating %d small blocks and %d large blocks",
			num_small_allocs, num_large_allocs
		);

		for(int j = 0; j < num_small_allocs; ++j)
		{
			size_t small_alloc_size = munit_rand_int_range(1, 64);
			void* mem = lip_malloc(allocator, small_alloc_size);
			munit_assert_ptr_not_null(mem);

			lip_array_foreach(void*, ptr, pointers)
			{
				munit_assert_ptr_not_equal(*ptr, mem);
			}
			lip_array_push(pointers, mem);

			memset(mem, i, small_alloc_size);
		}

		for(int j = 0; j < num_large_allocs; ++j)
		{
			size_t large_alloc_size = munit_rand_int_range(128 + 1, 128 * 2);
			void* mem = lip_malloc(allocator, large_alloc_size);
			munit_assert_ptr_not_null(mem);

			lip_array_foreach(void*, ptr, pointers)
			{
				munit_assert_ptr_not_equal(*ptr, mem);
			}
			lip_array_push(pointers, mem);

			memset(mem, i, large_alloc_size);
		}
	}

	lip_array_destroy(pointers);
	lip_arena_allocator_destroy(allocator);

	return MUNIT_OK;
}

static MunitResult
reallocate(const MunitParameter params[], void* fixture_)
{
	(void)params;
	fixture_t* fixture = fixture_;
	lip_allocator_t* allocator = lip_arena_allocator_create(
		&fixture->base_allocator.vtable,
		128,
		true
	);
	lip_array(int) array = lip_array_create(allocator, int, 2);
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

	lip_arena_allocator_destroy(allocator);

	return MUNIT_OK;
}

static MunitResult
min_chunk_size(const MunitParameter params[], void* fixture_)
{
	(void)params;
	(void)fixture_;

	fixture_t* fixture = fixture_;
	lip_allocator_t* allocator = lip_arena_allocator_create(
		&fixture->base_allocator.vtable,
		4,
		false
	);

	munit_assert_ptr_not_null(lip_malloc(allocator, 2));
	munit_assert_ptr_not_null(lip_malloc(allocator, 16));

	lip_arena_allocator_destroy(allocator);

	return MUNIT_OK;
}

static MunitTest tests[] = {
	{
		.name = "/no_leak",
		.test = no_leak,
		.setup = setup,
		.tear_down = teardown
	},
	{
		.name = "/reallocate",
		.test = reallocate,
		.setup = setup,
		.tear_down = teardown
	},
	{
		.name = "/min_chunk_size",
		.test = min_chunk_size,
		.setup = setup,
		.tear_down = teardown
	},
	{ .test = NULL }
};

MunitSuite arena_allocator = {
	.prefix = "/arena_allocator",
	.tests = tests
};
