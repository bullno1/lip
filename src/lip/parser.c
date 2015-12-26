#include "parser.h"
#include <stdio.h>
#include "lexer.h"
#include "token.h"
#include "array.h"

static lip_parse_status_t lip_parser_switch_error(
	lip_parser_t* parser,
	lip_parse_result_t* result,
	lip_parse_status_t alt_status,
	lip_parse_result_t* alt_result
)
{
	lip_sexp_cleanup(&result->sexp);
	*result = *alt_result;
	return alt_status;
}

static lip_parse_status_t lip_parser_parse_list(
	lip_parser_t* parser, lip_token_t* token, lip_parse_result_t* result
)
{
	result->sexp.start = token->start;
	result->sexp.data.list = lip_array_new(parser->allocator);
	result->sexp.type = LIP_SEXP_LIST;

	lip_parse_status_t status;
	lip_parse_result_t next_result;

	while(true)
	{
		status = lip_parser_next_sexp(parser, &next_result);
		switch(status)
		{
			case LIP_PARSE_OK:
				lip_array_push(result->sexp.data.list, next_result.sexp);
				break;
			case LIP_PARSE_LEX_ERROR:
			case LIP_PARSE_UNTERMINATED_LIST:
				return lip_parser_switch_error(
					parser, result, status, &next_result
				);
			case LIP_PARSE_UNEXPECTED_TOKEN:
				if(next_result.error.unexpected_token.type == LIP_TOKEN_RPAREN)
				{
					result->sexp.end = next_result.error.unexpected_token.end;
					return LIP_PARSE_OK;
				}
				else
				{
					return lip_parser_switch_error(
						parser, result, status, &next_result
					);
				}
			case LIP_PARSE_EOS:
				lip_sexp_cleanup(&result->sexp);
				result->error.location = token->start;
				return LIP_PARSE_UNTERMINATED_LIST;
		}
	}
}

static lip_parse_status_t lip_parser_make_string(
	lip_parser_t* parser, lip_token_t* token, lip_parse_result_t* result
)
{
	switch(token->type)
	{
	case LIP_TOKEN_STRING:
		result->sexp.type = LIP_SEXP_STRING;
		break;
	case LIP_TOKEN_SYMBOL:
		result->sexp.type = LIP_SEXP_SYMBOL;
		break;
	case LIP_TOKEN_NUMBER:
		result->sexp.type = LIP_SEXP_NUMBER;
		break;
	default:
		abort();
		break;
	}

	result->sexp.start = token->start;
	result->sexp.end = token->end;
	result->sexp.data.string = token->lexeme;
	return LIP_PARSE_OK;
}

static lip_parse_status_t lip_parser_process_token(
	lip_parser_t* parser, lip_token_t* token, lip_parse_result_t* result
)
{
	switch(token->type)
	{
		case LIP_TOKEN_LPAREN:
			return lip_parser_parse_list(parser, token, result);
		case LIP_TOKEN_RPAREN:
			result->error.unexpected_token = *token;
			return LIP_PARSE_UNEXPECTED_TOKEN;
		case LIP_TOKEN_STRING:
		case LIP_TOKEN_SYMBOL:
		case LIP_TOKEN_NUMBER:
			return lip_parser_make_string(parser, token, result);
	}

	abort();
}

void lip_parser_init(
	lip_parser_t* parser, lip_lexer_t* lexer, lip_allocator_t* allocator
)
{
	parser->lexer = lexer;
	parser->allocator = allocator;
}

lip_parse_status_t lip_parser_next_sexp(
	lip_parser_t* parser, lip_parse_result_t* result
)
{
	lip_token_t token;
	lip_lex_status_t status = lip_lexer_next_token(parser->lexer, &token);
	switch(status)
	{
		case LIP_LEX_OK:
			return lip_parser_process_token(parser, &token, result);
		case LIP_LEX_BAD_NUMBER:
		case LIP_LEX_BAD_STRING:
			result->error.lex_error.status = status;
			result->error.lex_error.token = token;
			return LIP_PARSE_LEX_ERROR;
		case LIP_LEX_EOS:
			return LIP_PARSE_EOS;
	}
	abort();
}

void lip_parser_print_status(
	lip_parse_status_t status, lip_parse_result_t* result
)
{
	printf("%s", lip_parse_status_t_to_str(status));
	switch(status)
	{
		case LIP_PARSE_OK:
			printf(" ");
			lip_sexp_print(&(result->sexp), 0);
			break;
		case LIP_PARSE_LEX_ERROR:
			printf(" ");
			lip_lexer_print_status(
				result->error.lex_error.status,
				&(result->error.lex_error.token)
			);
			break;
		case LIP_PARSE_UNTERMINATED_LIST:
			printf(
				" %u:%u",
				result->error.location.line, result->error.location.column
			);
			break;
		case LIP_PARSE_UNEXPECTED_TOKEN:
			printf(" ");
			lip_token_print(&(result->error.unexpected_token));
			break;
		case LIP_PARSE_EOS:
			break;
	}
}
