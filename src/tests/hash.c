#include <lip/allocator.h>
#include <lip/types.h>
#include <lip/hash.h>
#include <lip/utils.h>
#include "greatest.h"

KHASH_MAP_INIT_INT(int2int, int)

TEST integer_key()
{
	khash_t(int2int)* h = kh_init(int2int, lip_default_allocator);

	int ret;
	for(int i = 0; i < 100; ++i)
	{
		khiter_t iter = kh_put(int2int, h, i, &ret);
		kh_value(h, iter) = i * 2;
	}
	ASSERT_EQ(100, kh_size(h));

	for(int i = 0; i < 100; ++i)
	{
		khiter_t iter = kh_get(int2int, h, i);
		ASSERT_EQ(i * 2, kh_value(h, iter));
	}

	kh_destroy(int2int, h);

	PASS();
}

KHASH_INIT(string,
	lip_string_ref_t, char, false, lip_string_ref_hash, lip_string_ref_equal)

TEST string_key()
{
	khash_t(string)* h = kh_init(string, lip_default_allocator);

	int ret;

	kh_put(string, h, lip_string_ref("first"), &ret);
	kh_put(string, h, lip_string_ref("second"), &ret);
	kh_put(string, h, lip_string_ref("third"), &ret);
	ASSERT_EQ(3, kh_size(h));

	khiter_t iter;
	iter = kh_get(string, h, lip_string_ref("first"));
	ASSERT(kh_exist(h, iter));
	iter = kh_get(string, h, lip_string_ref("second"));
	ASSERT(kh_exist(h, iter));
	iter = kh_get(string, h, lip_string_ref("third"));
	ASSERT(kh_exist(h, iter));
	iter = kh_get(string, h, lip_string_ref("fourth"));
	ASSERT_FALSE(kh_exist(h, iter));
	iter = kh_get(string, h, lip_string_ref("third"));
	kh_del(string, h, iter);
	ASSERT_EQ(2, kh_size(h));
	iter = kh_get(string, h, lip_string_ref("third"));
	ASSERT_FALSE(kh_exist(h, iter));

	kh_destroy(string, h);

	PASS();
}

SUITE(hash)
{
	RUN_TEST(integer_key);
	RUN_TEST(string_key);
}
