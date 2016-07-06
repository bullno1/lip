#include "ex/parser.h"
#include "ex/lexer.h"
#include "token.h"
#include "array.h"
#include "memory.h"

static lip_stream_status_t
lip_parser_parse_list(lip_parser_t* parser, lip_token_t* token, lip_sexp_t* sexp)
{
	sexp->location.start = token->location.start;
	sexp->type = LIP_SEXP_LIST;
	lip_array(lip_sexp_t) list = lip_array_create(parser->allocator, lip_sexp_t, 1);
	lip_sexp_t element;

	while(true)
	{
		lip_stream_status_t status = lip_parser_next_sexp(parser, &element);
		switch(status)
		{
			case LIP_STREAM_OK:
				lip_array_push(list, element);
				break;
			case LIP_STREAM_ERROR:
				switch(parser->error.code)
				{
					case LIP_PARSE_LEX_ERROR:
					case LIP_PARSE_UNTERMINATED_LIST:
						lip_array_destroy(list);
						return LIP_STREAM_ERROR;
					case LIP_PARSE_UNEXPECTED_TOKEN:
						if(parser->error_token.type == LIP_TOKEN_RPAREN)
						{
							sexp->location.end = parser->error_token.location.end;
							sexp->data.list = list;
							lip_array_push(parser->lists, list);
							return LIP_STREAM_OK;
						}
						else
						{
							lip_array_destroy(list);
							return LIP_STREAM_ERROR;
						}
				}
			case LIP_STREAM_END:
				lip_array_destroy(list);
				parser->error.code = LIP_PARSE_UNTERMINATED_LIST;
				parser->error.location = token->location;
				return LIP_STREAM_ERROR;
		}
	}
}

static lip_stream_status_t
lip_parser_parse_element(lip_parser_t* parser, lip_token_t* token, lip_sexp_t* sexp)
{
	(void)parser;
	switch(token->type)
	{
	case LIP_TOKEN_STRING:
		sexp->type = LIP_SEXP_STRING;
		break;
	case LIP_TOKEN_SYMBOL:
		sexp->type = LIP_SEXP_SYMBOL;
		break;
	case LIP_TOKEN_NUMBER:
		sexp->type = LIP_SEXP_NUMBER;
		break;
	default:
		// Impossibru!!
		parser->error.location = token->location;
		parser->error.code = LIP_PARSE_UNEXPECTED_TOKEN;
		parser->error.extra = token;
		return LIP_STREAM_ERROR;
	}

	sexp->location = token->location;
	sexp->data.string = token->lexeme;
	return LIP_STREAM_OK;;
}

static lip_stream_status_t
lip_parser_parse(lip_parser_t* parser, lip_token_t* token, lip_sexp_t* sexp)
{
	switch(token->type)
	{
		case LIP_TOKEN_LPAREN:
			return lip_parser_parse_list(parser, token, sexp);
		case LIP_TOKEN_RPAREN:
			parser->error.location = token->location;
			parser->error.code = LIP_PARSE_UNEXPECTED_TOKEN;
			parser->error_token = *token;
			parser->error.extra = &parser->error_token;
			return LIP_STREAM_ERROR;
		case LIP_TOKEN_STRING:
		case LIP_TOKEN_SYMBOL:
		case LIP_TOKEN_NUMBER:
			return lip_parser_parse_element(parser, token, sexp);
	}

	// Impossibru!!
	return LIP_STREAM_ERROR;
}

lip_parser_t*
lip_parser_create(lip_allocator_t* allocator)
{
	lip_parser_t* parser = lip_new(allocator, lip_parser_t);
	lip_parser_init(parser, allocator);
	return parser;
}

void
lip_parser_destroy(lip_parser_t* parser)
{
	lip_parser_cleanup(parser);
	lip_free(parser->allocator, parser);
}

void
lip_parser_init(lip_parser_t* parser, lip_allocator_t* allocator)
{
	parser->allocator = allocator;
	lip_lexer_init(&parser->lexer, allocator);
	parser->lists = lip_array_create(allocator, lip_array(lip_sexp_t), 1);
	lip_parser_reset(parser, NULL);
}

void
lip_parser_cleanup(lip_parser_t* parser)
{
	lip_parser_reset(parser, NULL);
	lip_array_destroy(parser->lists);
	lip_lexer_cleanup(&parser->lexer);
}

void
lip_parser_reset(lip_parser_t* parser, lip_in_t* input)
{
	memset(&parser->error, 0, sizeof(parser->error));
	lip_array_foreach(lip_array(lip_sexp_t), list, parser->lists)
	{
		lip_array_destroy(*list);
	}
	lip_array_clear(parser->lists);
	lip_lexer_reset(&parser->lexer, input);
}

lip_stream_status_t
lip_parser_next_sexp(lip_parser_t* parser, lip_sexp_t* sexp)
{
	lip_token_t token;
	memset(&parser->error, 0, sizeof(parser->error));

	lip_stream_status_t status = lip_lexer_next_token(&parser->lexer, &token);
	switch(status)
	{
		case LIP_STREAM_OK:
			return lip_parser_parse(parser, &token, sexp);
		case LIP_STREAM_ERROR:
			parser->error.code = LIP_PARSE_LEX_ERROR;
			parser->error.location = lip_lexer_last_error(&parser->lexer)->location;
			parser->error.extra = lip_lexer_last_error(&parser->lexer);
			return LIP_STREAM_ERROR;
		case LIP_STREAM_END:
			return LIP_STREAM_END;
	}

	// Impossibru!!
	return LIP_STREAM_ERROR;
}

lip_error_t*
lip_parser_last_error(lip_parser_t* parser)
{
	return &parser->error;
}
