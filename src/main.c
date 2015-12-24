#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "lip_token.h"
#include "lip_lexer.h"

bool read_file(const char* filename, void** buff, size_t* size)
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

int main(int argc, char* argv[])
{
	void* content = NULL;
	size_t len = 0;
	if(!read_file("data/test.lip", &content, &len)) { return EXIT_FAILURE; }

	lip_lexer_t lexer;
	lip_lexer_init(&lexer, (char*)content, len);
	lip_token_t token;
	while(lip_lexer_next_token(&lexer, &token))
	{
		lip_token_print(&token);
		printf("\n");
	}

	free(content);
	return EXIT_SUCCESS;
}
