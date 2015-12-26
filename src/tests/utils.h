#include <stdio.h>

static inline bool read_file(const char* filename, void** buff, size_t* size)
{
	FILE* file = fopen(filename, "rb");
	if(!file) { return false; }

	fseek(file, 0, SEEK_END);
	*size = ftell(file);
	*buff = malloc(*size);
	fseek(file, 0, SEEK_SET);
	fread(*buff, sizeof(char), *size, file);
	fclose(file);

	return true;
}

