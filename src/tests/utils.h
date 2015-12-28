#ifndef LIP_TEST_UTILS_H
#define LIP_TEST_UTILS_H

#include <stdio.h>
#include <stdlib.h>

static inline size_t read_file(void* ptr, size_t size, void* file)
{
	return fread(ptr, size, 1, (FILE*)file);
}

static inline size_t write_file(const void* ptr, size_t size, void* file)
{
	return fwrite(ptr, size, 1, (FILE*)file);
}

#endif
