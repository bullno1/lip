#include <lip/lexer.h>
#include <ctype.h>
#include <lip/array.h>
#include <lip/io.h>

void
lip_lexer_init(lip_lexer_t* lexer, lip_allocator_t* allocator)
{
	lexer->allocator = allocator;
	lexer->capture_buffs = lip_array_create(allocator, lip_array(char), 0);
	lexer->capture_buff = NULL;
	lip_lexer_reset(lexer, NULL);
}

void
lip_lexer_cleanup(lip_lexer_t* lexer)
{
	lip_lexer_reset(lexer, NULL);
	lip_array_destroy(lexer->capture_buffs);
	if(lexer->capture_buff)
	{
		lip_array_destroy(lexer->capture_buff);
	}
}

void
lip_lexer_reset(lip_lexer_t* lexer, lip_in_t* input)
{
	lexer->location.line = 1;
	lexer->location.column = 1;
	lexer->read_buff = 0;
	lexer->capturing = false;
	lexer->buffered = false;
	lexer->eos = false;
	lexer->input = input;
	lip_clear_last_error(&lexer->last_error);
	lip_array_foreach(lip_array(char), buff, lexer->capture_buffs)
	{
		lip_array_destroy(*buff);
	}
	lip_array_clear(lexer->capture_buffs);
	if(lexer->capture_buff)
	{
		lip_array_clear(lexer->capture_buff);
	}
}

static void
lip_lexer_begin_capture(lip_lexer_t* lexer)
{
	if(!lexer->capture_buff)
	{
		lexer->capture_buff = lip_array_create(lexer->allocator, char, 0);
	}
	lexer->capturing = true;
}

static void
lip_lexer_reset_capture(lip_lexer_t* lexer)
{
	if(lexer->capture_buff)
	{
		lip_array_clear(lexer->capture_buff);
	}
	lexer->capturing = false;
}

static lip_string_ref_t
lip_lexer_end_capture(lip_lexer_t* lexer)
{

	unsigned int len = lip_array_len(lexer->capture_buff);
	lip_array_push(lexer->capture_buff, 0); // null-terminate
	lip_string_ref_t ref = { len, lexer->capture_buff };
	lip_array_push(lexer->capture_buffs, lexer->capture_buff);
	lexer->capture_buff = NULL;
	lexer->capturing = false;

	return ref;
}

static lip_stream_status_t
lip_lexer_make_token(
	lip_lexer_t* lexer,
	lip_token_t* token,
	lip_token_type_t type
)
{
	token->type = type;
	token->lexeme = lip_lexer_end_capture(lexer);
	token->location.end = lexer->location;
	--token->location.end.column;

	return LIP_STREAM_OK;
}

static bool
lip_lexer_peek_char(lip_lexer_t* lexer, char* ch)
{
	if(lexer->buffered) { *ch = lexer->read_buff; return true; }

	if(lip_read(&lexer->read_buff, 1, lexer->input))
	{
		lexer->buffered = true;
		*ch = lexer->read_buff;
		return true;
	}
	else
	{
		lexer->eos = true;
		return false;
	}
}

static void
lip_lexer_consume_char(lip_lexer_t* lexer)
{
	lexer->buffered = false;
	if(lexer->capturing)
	{
		lip_array_push(lexer->capture_buff, lexer->read_buff);
	}
	++lexer->location.column;
}

static inline bool
lip_lexer_is_separator(char ch)
{
	return isspace(ch) || ch == ')' || ch == '(' || ch == ';' || ch == '"';
}

static lip_stream_status_t
lip_lexer_error(lip_lexer_t* lexer, lip_lex_error_t error)
{
	lexer->last_error.error.code = error;
	lexer->last_error.error.location.end = lexer->location;
	--lexer->last_error.error.location.end.column;
	lexer->last_error.errorp = &lexer->last_error.error;
	lip_lexer_reset_capture(lexer);

	return LIP_STREAM_ERROR;
}

static lip_stream_status_t
lip_lexer_scan_number(lip_lexer_t* lexer, lip_token_t* token)
{
	char ch;
	bool found_point = false;

	while(lip_lexer_peek_char(lexer, &ch))
	{
		if(ch == '.')
		{
			lip_lexer_consume_char(lexer);

			if(found_point)
			{
				return lip_lexer_error(lexer, LIP_LEX_BAD_NUMBER);
			}
			else
			{
				found_point = true;
				continue;
			}
		}
		else if(isdigit(ch))
		{
			lip_lexer_consume_char(lexer);
			continue;
		}
		else if(!lip_lexer_is_separator(ch))
		{
			lip_lexer_consume_char(lexer);
			return lip_lexer_error(lexer, LIP_LEX_BAD_NUMBER);
		}
		else
		{
			break;
		}
	}

	return lip_lexer_make_token(lexer, token, LIP_TOKEN_NUMBER);
}

static lip_stream_status_t
lip_lexer_scan_symbol(lip_lexer_t* lexer, lip_token_t* token)
{
	char ch;
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

lip_stream_status_t
lip_lexer_next_token(lip_lexer_t* lexer, lip_token_t* token)
{
	if(lexer->eos) { return LIP_STREAM_END; }

	lip_clear_last_error(&lexer->last_error);

	char ch;
	while(lip_lexer_peek_char(lexer, &ch))
	{
		lexer->last_error.error.location.start = token->location.start = lexer->location;
		lip_lexer_begin_capture(lexer);
		lip_lexer_consume_char(lexer);

		switch(ch)
		{
			case ' ':
			case '\t':
				lip_lexer_reset_capture(lexer);
				continue;
			case '\r':
				lip_lexer_reset_capture(lexer);
				++lexer->location.line;
				lexer->location.column = 1;

				if(lip_lexer_peek_char(lexer, &ch) && ch == '\n')
				{
					lip_lexer_consume_char(lexer);
				}
				continue;
			case '\n':
				lip_lexer_reset_capture(lexer);
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
				lip_lexer_reset_capture(lexer);
				lip_lexer_begin_capture(lexer);
				while(lip_lexer_peek_char(lexer, &ch))
				{
					// TODO: handle \"
					if(ch == '"')
					{
						lip_lexer_make_token(lexer, token, LIP_TOKEN_STRING);
						++token->location.end.column; // include '"'
						lip_lexer_consume_char(lexer);
						return LIP_STREAM_OK;
					}
					else
					{
						lip_lexer_consume_char(lexer);
					}
				}

				return lip_lexer_error(lexer, LIP_LEX_BAD_STRING);
			case '-':
				lip_lexer_peek_char(lexer, &ch);
				if(isdigit(ch))
				{
					return lip_lexer_scan_number(lexer, token);
				}
				else if(lip_lexer_is_separator(ch))
				{
					return lip_lexer_make_token(lexer, token, LIP_TOKEN_SYMBOL);
				}
				else
				{
					return lip_lexer_scan_symbol(lexer, token);
				}
			default:
				if(isdigit(ch))
				{
					return lip_lexer_scan_number(lexer, token);
				}
				else
				{
					return lip_lexer_scan_symbol(lexer, token);
				}
		}
	}

	return LIP_STREAM_END;
}

const lip_error_t*
lip_lexer_last_error(lip_lexer_t* lexer)
{
	return lip_last_error(&lexer->last_error);
}
