#include <stdio.h>
#include <lip/lexer.h>
#include <lip/parser.h>
#include <lip/allocator.h>
#include "utils.h"

int main(int argc, char* argv[])
{
	void* content = NULL;
	size_t len = 0;
	if(!read_file("data/test.lip", &content, &len)) { return EXIT_FAILURE; }

	lip_lexer_t lexer;
	lip_lexer_init(&lexer, (char*)content, len);

	lip_parser_t parser;
	lip_parser_init(&parser, &lexer, lip_default_allocator);

	lip_parse_status_t parse_status;
	lip_parse_result_t parse_result;
	while(
		parse_status = lip_parser_next_sexp(&parser, &parse_result),
		parse_status == LIP_PARSE_OK
	)
	{
		lip_sexp_print(&parse_result.sexp, 0);
		lip_sexp_cleanup(&parse_result.sexp);
		printf("\n");
	}

	printf("\n");
	lip_parser_print_status(parse_status, &parse_result);
	printf("\n");

	free(content);
	return EXIT_SUCCESS;
}
