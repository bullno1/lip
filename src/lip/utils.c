#include "utils.h"
#include <stdarg.h>
#include <stdio.h>

size_t lip_fread(void* ptr, size_t size, void* file)
{
	return fread(ptr, size, 1, (FILE*)file);
}

size_t lip_fwrite(const void* ptr, size_t size, void* file)
{
	return fwrite(ptr, size, 1, (FILE*)file);
}

void lip_printf(
	lip_write_fn_t write_fn, void* ctx, const char* format, ...
)
{
	char buff[LIP_PRINTF_BUFF_SIZE];
	va_list args;
	va_start(args, format);
	int length = vsnprintf(buff, LIP_PRINTF_BUFF_SIZE, format, args);
	if(length > 0)
	{
		write_fn(buff, length, ctx);
	}
	va_end(args);
}
