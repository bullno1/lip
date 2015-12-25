#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "token.h"
#include "lexer.h"
#include "allocator.h"
#include "parser.h"

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
	lip_lex_status_t lex_status;

	while(
		lex_status = lip_lexer_next_token(&lexer, &token),
		lex_status == LIP_LEX_OK
	)
	{
		lip_token_print(&token);
		printf("\n");
	}

	printf("\n");
	lip_lexer_print_status(lex_status, &token);
	printf("\n");

	printf("\n");
	lip_parser_t parser;
	lip_lexer_init(&lexer, (char*)content, len);
	lip_parser_init(&parser, &lexer, &lip_default_allocator);
	lip_parse_status_t parse_status;
	lip_parse_result_t parse_result;
	while(
		parse_status = lip_parser_next_sexp(&parser, &parse_result),
		parse_status == LIP_PARSE_OK
	)
	{
		lip_sexp_print(&parse_result.sexp, 0);
		lip_sexp_free(&parse_result.sexp, &lip_default_allocator);
		printf("\n");
	}

	printf("\n");
	lip_parser_print_status(parse_status, &parse_result);
	printf("\n");

	free(content);
	return EXIT_SUCCESS;
}
