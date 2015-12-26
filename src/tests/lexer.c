#include <stdio.h>
#include <lip/token.h>
#include <lip/lexer.h>
#include "utils.h"

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

	free(content);

	return EXIT_SUCCESS;
}
