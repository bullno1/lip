#include "lip_lexer.h"
#include "lip_token.h"
#include <ctype.h>

void lip_lexer_init(lip_lexer_t* lexer, const char* buff, size_t len)
{
	lexer->buff = buff;
	lexer->end = buff + len;
	lexer->location.line = 1;
	lexer->location.column = 1;
}

static bool lip_lexer_make_token(
	lip_lexer_t* lexer,
	lip_token_t* token,
	lip_token_type_t type
)
{
	token->type = type;
	token->length = lexer->buff - token->lexeme;
	token->end = lexer->location;
	--token->end.column;

	return true;
}

static bool lip_lexer_peek_char(lip_lexer_t* lexer, char* ch)
{
	bool eos = lexer->buff == lexer->end;
	*ch = eos ? 0 : *lexer->buff;
	return !eos;
}

static void lip_lexer_consume_char(lip_lexer_t* lexer)
{
	++lexer->buff;
	++lexer->location.column;
}

static bool lip_lexer_is_separator(char ch)
{
	return isspace(ch) || ch == ')' || ch == '(' || ch == ';';
}

bool lip_lexer_next_token(lip_lexer_t* lexer, lip_token_t* token)
{
	if(lexer->buff > lexer->end) { return false; }

	char ch;
	while(lip_lexer_peek_char(lexer, &ch))
	{
		token->start = lexer->location;
		token->lexeme = lexer->buff;
		lip_lexer_consume_char(lexer);

		switch(ch)
		{
			case ' ':
			case '\t':
				continue;
			case '\r':
				++lexer->location.line;
				lexer->location.column = 1;

				if(lip_lexer_peek_char(lexer, &ch) && ch == '\n')
				{
					lip_lexer_consume_char(lexer);
				}
				continue;
			case '\n':
				++lexer->location.line;
				lexer->location.column = 1;
				continue;
			case '(':
				return lip_lexer_make_token(lexer, token, LIP_TOKEN_LPAREN);
			case ')':
				return lip_lexer_make_token(lexer, token, LIP_TOKEN_RPAREN);
			case ';':
				while(lip_lexer_peek_char(lexer, &ch))
				{
					if(ch == '\r' || ch == '\n')
					{
						break;
					}
					else
					{
						lip_lexer_consume_char(lexer);
					}
				}
				continue;
			case '"':
				token->start = lexer->location;
				token->lexeme = lexer->buff;
				while(lip_lexer_peek_char(lexer, &ch))
				{
					if(ch == '"')
					{
						lip_lexer_make_token(lexer, token, LIP_TOKEN_STRING);
						lip_lexer_consume_char(lexer);
						return true;
					}
					else
					{
						lip_lexer_consume_char(lexer);
					}
				}
				return lip_lexer_make_token(lexer, token, LIP_TOKEN_ERROR);
			default:
				if(isdigit(ch))
				{
					while(lip_lexer_peek_char(lexer, &ch))
					{
						if(ch == '.' || isdigit(ch))
						{
							lip_lexer_consume_char(lexer);
							continue;
						}
						else if(!lip_lexer_is_separator(ch))
						{
							lip_lexer_consume_char(lexer);
							return lip_lexer_make_token(
								lexer,
								token,
								LIP_TOKEN_ERROR
							);
						}
						else
						{
							break;
						}
					}

					return lip_lexer_make_token(lexer, token, LIP_TOKEN_NUMBER);
				}
				else
				{
					while(lip_lexer_peek_char(lexer, &ch))
					{
						if(!lip_lexer_is_separator(ch))
						{
							lip_lexer_consume_char(lexer);
						}
						else
						{
							break;
						}
					}

					return lip_lexer_make_token(lexer, token, LIP_TOKEN_SYMBOL);
				}
		}
	}

	lip_lexer_consume_char(lexer);
	return lip_lexer_make_token(lexer, token, LIP_TOKEN_EOS);
}
