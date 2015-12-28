#include <stdio.h>
#include <stdlib.h>
#include <lip/token.h>
#include <lip/lexer.h>
#include <lip/allocator.h>
#include "utils.h"

int main(int argc, char* argv[])
{
	lip_lexer_t lexer;
	FILE* file = fopen("data/test.lip", "rb");
	lip_lexer_init(&lexer, lip_default_allocator, read_file, file);
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

	lip_lexer_cleanup(&lexer);

	return EXIT_SUCCESS;
}
