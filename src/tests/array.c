#include <lip/types.h>
#include <lip/array.h>
#include <lip/allocator.h>
#include "greatest.h"

TEST short_array()
{
	lip_array(short) short_array = lip_array_new(lip_default_allocator);
	ASSERT_EQ(0, lip_array_len(short_array));

	for(unsigned int i = 0; i < 50; ++i)
	{
		ASSERT_EQ(i, lip_array_len(short_array));
		lip_array_push(short_array, i * 2 + 3);
		ASSERT_EQ((short)(i * 2 + 3), short_array[i]);
	}
	ASSERT_EQ(50, lip_array_len(short_array));

	for(int i = 0; i < 50; ++i)
	{
		ASSERT_EQ(i * 2 + 3, short_array[i]);
	}

	lip_array_resize(short_array, 5);
	ASSERT_EQ(5,lip_array_len(short_array));
	for(int i = 0; i < 5; ++i)
	{
		ASSERT_EQ(i * 2 + 3, short_array[i]);
	}

	lip_array_delete(short_array);
	PASS();
}

TEST char_array()
{
	lip_array(char) char_array = lip_array_new(lip_default_allocator);
	ASSERT_EQ(0, lip_array_len(char_array));

	for(unsigned int i = 0; i < 50; ++i)
	{
		ASSERT_EQ(i, lip_array_len(char_array));
		lip_array_push(char_array, i * 2 + 3);
		ASSERT_EQ((char)(i * 2 + 3), char_array[i]);
	}
	ASSERT_EQ(50, lip_array_len(char_array));

	for(int i = 0; i < 50; ++i)
	{
		ASSERT_EQ(i * 2 + 3, char_array[i]);
	}

	lip_array_resize(char_array, 5);
	ASSERT_EQ(5,lip_array_len(char_array));
	for(int i = 0; i < 5; ++i)
	{
		ASSERT_EQ(i * 2 + 3, char_array[i]);
	}

	lip_array_delete(char_array);
	PASS();
}

SUITE(array)
{
	RUN_TEST(char_array);
	RUN_TEST(short_array);
}
