#include <lip/parser.h>
#include <lip/array.h>

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
				switch(parser->last_error.error.code)
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
				lip_set_last_error(
					&parser->last_error,
					LIP_PARSE_UNTERMINATED_LIST,
					token->location,
					NULL
				);
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
		lip_set_last_error(
			&parser->last_error,
			LIP_PARSE_UNEXPECTED_TOKEN,
			token->location,
			NULL
		);
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
			parser->error_token = *token;
			lip_set_last_error(
				&parser->last_error,
				LIP_PARSE_UNEXPECTED_TOKEN,
				token->location,
				&parser->error_token
			);
			return LIP_STREAM_ERROR;
		case LIP_TOKEN_STRING:
		case LIP_TOKEN_SYMBOL:
		case LIP_TOKEN_NUMBER:
			return lip_parser_parse_element(parser, token, sexp);
		case LIP_TOKEN_QUOTE:
		case LIP_TOKEN_QUASIQUOTE:
		case LIP_TOKEN_UNQUOTE:
		case LIP_TOKEN_UNQUOTE_SPLICING:
			{
				lip_sexp_t quoted_sexp;
				lip_stream_status_t status;
				switch(status = lip_parser_next_sexp(parser, &quoted_sexp))
				{
					case LIP_STREAM_OK:
					case LIP_STREAM_END:
						{
							lip_array(lip_sexp_t) list = lip_array_create(
								parser->allocator, lip_sexp_t, 2
							);
							lip_array_push(parser->lists, list);

							const char* symbol;
							switch(token->type)
							{
								case LIP_TOKEN_QUOTE:
									symbol = "quote";
									break;
								case LIP_TOKEN_QUASIQUOTE:
									symbol = "quasiquote";
									break;
								case LIP_TOKEN_UNQUOTE:
									symbol = "unquote";
									break;
								case LIP_TOKEN_UNQUOTE_SPLICING:
									symbol = "unquote-splicing";
									break;
								default:
									// Impossibru!!
									lip_set_last_error(
										&parser->last_error,
										LIP_PARSE_UNEXPECTED_TOKEN,
										token->location,
										NULL
									);
									return LIP_STREAM_ERROR;
							}

							lip_sexp_t quote_sexp = {
								.type = LIP_SEXP_SYMBOL,
								.location = token->location,
								.data  = { .string = lip_string_ref(symbol) }
							};

							lip_array_push(list, quote_sexp);
							lip_array_push(list, quoted_sexp);

							*sexp = (lip_sexp_t){
								.type = LIP_SEXP_LIST,
								.location = {
									.start = token->location.start,
									.end = quoted_sexp.location.end
								},
								.data = { .list = list }
							};
						}
						break;
					case LIP_STREAM_ERROR:
						break;
				}

				return status;
			}
	}

	// Impossibru!!
	return LIP_STREAM_ERROR;
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
	lip_clear_last_error(&parser->last_error);
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
	lip_clear_last_error(&parser->last_error);

	lip_stream_status_t status = lip_lexer_next_token(&parser->lexer, &token);
	switch(status)
	{
		case LIP_STREAM_OK:
			return lip_parser_parse(parser, &token, sexp);
		case LIP_STREAM_ERROR:
			lip_set_last_error(
				&parser->last_error,
				LIP_PARSE_LEX_ERROR,
				lip_lexer_last_error(&parser->lexer)->location,
				lip_lexer_last_error(&parser->lexer)
			);
			return LIP_STREAM_ERROR;
		case LIP_STREAM_END:
			return LIP_STREAM_END;
	}

	// Impossibru!!
	return LIP_STREAM_ERROR;
}

const lip_error_t*
lip_parser_last_error(lip_parser_t* parser)
{
	return lip_last_error(&parser->last_error);
}
