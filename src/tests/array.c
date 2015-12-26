#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <lip/array.h>
#include <lip/allocator.h>

int main(int argc, char* argv[])
{
	char* char_array = lip_array_new(lip_default_allocator);
	assert(lip_array_len(char_array) == 0);

	for(int i = 0; i < 50; ++i)
	{
		assert(lip_array_len(char_array) == i);
		lip_array_push(char_array, i * 2 + 3);
		assert(char_array[i] == i * 2 + 3);
	}
	assert(lip_array_len(char_array) == 50);

	for(int i = 0; i < 50; ++i)
	{
		assert(char_array[i] == i * 2 + 3);
	}

	lip_array_resize(char_array, 5);
	assert(lip_array_len(char_array) == 5);
	for(int i = 0; i < 5; ++i)
	{
		assert(char_array[i] == i * 2 + 3);
	}

	lip_array_delete(char_array);

	short* short_array = lip_array_new(lip_default_allocator);
	assert(lip_array_len(short_array) == 0);

	for(int i = 0; i < 50; ++i)
	{
		assert(lip_array_len(short_array) == i);
		lip_array_push(short_array, i * 2 + 3);
		assert(short_array[i] == i * 2 + 3);
	}
	assert(lip_array_len(short_array) == 50);

	for(int i = 0; i < 50; ++i)
	{
		assert(short_array[i] == i * 2 + 3);
	}

	lip_array_resize(short_array, 5);
	assert(lip_array_len(short_array) == 5);
	for(int i = 0; i < 5; ++i)
	{
		assert(short_array[i] == i * 2 + 3);
	}

	lip_array_delete(short_array);

	return EXIT_SUCCESS;
}
