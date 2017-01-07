#include <lip/preprocessor.h>
#include <lip/array.h>
#include <lip/memory.h>

static lip_pp_result_t
lip_pp_success(lip_sexp_t* sexp)
{
	return (lip_pp_result_t){
		.success = true,
		.value = { .result = sexp }
	};
}

static lip_pp_result_t
lip_pp_error(lip_loc_range_t location, const char* msg)
{
	return (lip_pp_result_t){
		.success = false,
		.value = {
			.error = {
				.location = location,
				.extra = msg
			}
		}
	};
}

static lip_pp_result_t
lip_quote(lip_pp_t* pp, lip_sexp_t* sexp);

static lip_pp_result_t
lip_quote_list(lip_pp_t* pp, lip_sexp_t* sexp)
{
	lip_array(lip_sexp_t) list = sexp->data.list;

	size_t length = lip_array_len(list);
	lip_array(lip_sexp_t) new_list = lip_array_create(pp->allocator, lip_sexp_t, length + 1);
	lip_array_push(new_list, ((lip_sexp_t){
		.type =  LIP_SEXP_SYMBOL,
		.location = sexp->location,
		.data = { .string = lip_string_ref("/list") }
	}));

	for(size_t i = 0; i < length; ++i)
	{
		lip_pp_result_t result = lip_quote(pp, &list[i]);
		if(!result.success) { return result; }

		lip_array_push(new_list, *result.value.result);
	}

	lip_sexp_t* list_sexp = lip_new(pp->allocator, lip_sexp_t);
	*list_sexp = (lip_sexp_t){
		.type = LIP_SEXP_LIST,
		.location = sexp->location,
		.data = { .list = new_list }
	};
	return lip_pp_success(list_sexp);
}

static lip_pp_result_t
lip_quote(lip_pp_t* pp, lip_sexp_t* sexp)
{
	switch(sexp->type)
	{
		case LIP_SEXP_NUMBER:
		case LIP_SEXP_STRING:
			return lip_pp_success(sexp);
		case LIP_SEXP_SYMBOL:
			{
				lip_array(lip_sexp_t) quote_list =
					lip_array_create(pp->allocator, lip_sexp_t, 2);
				lip_array_push(quote_list, ((lip_sexp_t){
					.type = LIP_SEXP_SYMBOL,
					.data = { .string = lip_string_ref("quote") }
				}));
				lip_array_push(quote_list, *sexp);

				lip_sexp_t* quote_sexp = lip_new(pp->allocator, lip_sexp_t);
				*quote_sexp = (lip_sexp_t){
					.type = LIP_SEXP_LIST,
					.location = sexp->location,
					.data = { .list = quote_list }
				};
				return lip_pp_success(quote_sexp);
			}
			break;
		case LIP_SEXP_LIST:
			return lip_quote_list(pp, sexp);
	}

	return lip_pp_error(sexp->location, "Unknown error");
}

static lip_pp_result_t
lip_quasiquote(lip_pp_t* pp, lip_sexp_t* sexp);

static lip_pp_result_t
lip_quasiquote_list(lip_pp_t* pp, lip_sexp_t* sexp)
{
	lip_array(lip_sexp_t) list = sexp->data.list;

	size_t length = lip_array_len(list);
	lip_array(lip_sexp_t) new_list = lip_array_create(
		pp->allocator, lip_sexp_t, length + 1
	);
	lip_array_push(new_list, ((lip_sexp_t){
		.type =  LIP_SEXP_SYMBOL,
		.location = sexp->location,
		.data = { .string = lip_string_ref("list/concat") }
	}));

	for(size_t i = 0; i < length; ++i)
	{
		if(list[i].type == LIP_SEXP_LIST
			&& lip_array_len(list[i].data.list) > 0
			&& list[i].data.list[0].type == LIP_SEXP_SYMBOL
			&& lip_string_ref_equal(list[i].data.list[0].data.string, lip_string_ref("unquote-splicing"))
		)
		{
			if(lip_array_len(list[i].data.list) != 2)
			{
				return lip_pp_error(
					list[i].location,
					"'unquote-splicing' must have the form: (unquote-splicing <sexp>)"
				);
			}

			lip_sexp_type_t type = list[i].data.list[1].type;
			if(type == LIP_SEXP_NUMBER || type == LIP_SEXP_STRING)
			{
				return lip_pp_error(
					list[i].data.list[1].location,
					"The expression passed to unquote-splicing must evaluate to a list"
				);
			}

			lip_array_push(new_list, list[i].data.list[1]);
		}
		else
		{
			lip_pp_result_t result = lip_quasiquote(pp, &list[i]);
			if(!result.success) { return result; }

			lip_array(lip_sexp_t) list = lip_array_create(pp->allocator, lip_sexp_t, 2);
			lip_array_push(list, ((lip_sexp_t){
				.type = LIP_SEXP_SYMBOL,
				.location = list[i].location,
				.data = { .string = lip_string_ref("/list") }
			}));
			lip_array_push(list, *result.value.result);

			lip_sexp_t* list_sexp = lip_new(pp->allocator, lip_sexp_t);
			*list_sexp = (lip_sexp_t){
				.type = LIP_SEXP_LIST,
				.location = list[i].location,
				.data = { .list = list }
			};

			lip_array_push(new_list, *list_sexp);
		}
	}

	lip_sexp_t* list_sexp = lip_new(pp->allocator, lip_sexp_t);
	*list_sexp = (lip_sexp_t){
		.type = LIP_SEXP_LIST,
		.location = sexp->location,
		.data = { .list = new_list }
	};
	return lip_pp_success(list_sexp);
}

static lip_pp_result_t
lip_quasiquote(lip_pp_t* pp, lip_sexp_t* sexp)
{
	switch(sexp->type)
	{
		case LIP_SEXP_NUMBER:
		case LIP_SEXP_STRING:
		case LIP_SEXP_SYMBOL:
			return lip_quote(pp, sexp);
		case LIP_SEXP_LIST:
			if(lip_array_len(sexp->data.list) > 0
				&& sexp->data.list[0].type == LIP_SEXP_SYMBOL
				&& lip_string_ref_equal(sexp->data.list[0].data.string, lip_string_ref("unquote"))
			)
			{
				if(lip_array_len(sexp->data.list) != 2)
				{
					return lip_pp_error(
						sexp->location,
						"'unquote' must have the form: (unquote <sexp>)"
					);
				}

				return lip_pp_success(&sexp->data.list[1]);
			}
			else if(lip_array_len(sexp->data.list) > 0
				&& sexp->data.list[0].type == LIP_SEXP_SYMBOL
				&& lip_string_ref_equal(
					sexp->data.list[0].data.string,
					lip_string_ref("unquote-splicing")
				)
			)
			{
				return lip_pp_error(
					sexp->location,
					"Cannot unquote-splicing outside of quasiquoted list"
				);
			}
			else
			{
				return lip_quasiquote_list(pp, sexp);
			}
			break;
	}

	return lip_pp_error(sexp->location, "Unknown error");
}

static lip_pp_result_t
lip_preprocess_regular_list(lip_pp_t* pp, lip_sexp_t* sexp)
{
	lip_array_foreach(lip_sexp_t, subsexp, sexp->data.list)
	{
		lip_pp_result_t result = lip_preprocess(pp, subsexp);
		if(!result.success) { return result; }

		*subsexp = *result.value.result;
	}

	return lip_pp_success(sexp);
}

static lip_pp_result_t
lip_preprocess_list(lip_pp_t* pp, lip_sexp_t* sexp)
{
	if(sexp->data.list[0].type == LIP_SEXP_SYMBOL)
	{
		lip_string_ref_t symbol = sexp->data.list[0].data.string;
		if(lip_string_ref_equal(symbol, lip_string_ref("quote")))
		{
			if(lip_array_len(sexp->data.list) != 2)
			{
				return lip_pp_error(
					sexp->location,
					"'quote' must have the form: (quote <sexp>)"
				);
			}

			if(sexp->data.list[1].type == LIP_SEXP_SYMBOL)
			{
				return lip_pp_success(sexp);
			}
			else
			{
				lip_pp_result_t result = lip_quote(pp, &sexp->data.list[1]);
				if(!result.success) { return result; }

				return lip_preprocess(pp, result.value.result);
			}
		}
		else if(lip_string_ref_equal(symbol, lip_string_ref("quasiquote")))
		{
			if(lip_array_len(sexp->data.list) != 2)
			{
				return lip_pp_error(
					sexp->location,
					"'quasiquote' must have the form: (quasiquote <sexp>)"
				);
			}

			lip_pp_result_t result = lip_quasiquote(pp, &sexp->data.list[1]);
			if(!result.success) { return result; }

			return lip_preprocess(pp, result.value.result);
		}
		else if(lip_string_ref_equal(symbol, lip_string_ref("unquote")))
		{
			return lip_pp_error(
				sexp->location, "Cannot unquote outside of quasiquote"
			);
		}
		else if(lip_string_ref_equal(symbol, lip_string_ref("unquote-splicing")))
		{
			return lip_pp_error(
				sexp->location, "Cannot unquote-splicing outside of quasiquoted list"
			);
		}
		else
		{
			return lip_preprocess_regular_list(pp, sexp);
		}
	}
	else
	{
		return lip_preprocess_regular_list(pp, sexp);
	}
}

lip_pp_result_t
lip_preprocess(lip_pp_t* pp, lip_sexp_t* sexp)
{
	return sexp->type == LIP_SEXP_LIST && lip_array_len(sexp->data.list) > 0
		? lip_preprocess_list(pp, sexp)
		: lip_pp_success(sexp);
}
