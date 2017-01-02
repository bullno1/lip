#ifndef LIP_BIND_H
#define LIP_BIND_H

#include "pp.h"

#define lip_function(name) \
	lip_exec_status_t name(lip_vm_t* vm, lip_value_t* result)

#define lip_bind_track_native_location(vm) \
	lip_set_native_location(vm, __func__, __FILE__, __LINE__); \

#define lip_bind_prepare(vm) \
	uint8_t argc; const lip_value_t* argv = lip_get_args(vm, &argc); (void)argv;

#define lip_bind_args(...) \
	lip_bind_prepare(vm); \
	const unsigned int arity_min = 0 lip_pp_map(lip_bind_count_arity, __VA_ARGS__); \
	const unsigned int arity_max = lip_pp_len(__VA_ARGS__); \
	if(arity_min != arity_max) { \
		lip_bind_assert_argc_at_least(arity_min); \
		lip_bind_assert_argc_at_most(arity_max); \
	} else { \
		lip_bind_assert_argc(arity_min); \
	} \
	lip_pp_map(lip_bind_arg, __VA_ARGS__)

#define lip_bind_arg(i, spec) \
	lip_bind_arg1( \
		i, \
		lip_pp_nth(1, spec, any), \
		lip_pp_nth(2, spec, missing_argument_name[-1]), \
		lip_pp_nth(3, spec, (required)) \
	)
#define lip_bind_arg1(i, type, name, extra) \
	lip_pp_concat(lip_bind_declare_, type)(name); \
	lip_pp_concat(lip_pp_concat(lip_bind_, lip_pp_nth(1, extra, required)), _arg)( \
		i, type, name, extra \
	)

#define lip_bind_count_arity(i, spec) \
    lip_bind_count_arity1(i, lip_pp_nth(1, lip_pp_nth(3, spec, (required)), required))
#define lip_bind_count_arity1(i, spec) \
	+ lip_pp_concat(lip_bind_count_arity_, spec)
#define lip_bind_count_arity_required 1
#define lip_bind_count_arity_optional 0

#define lip_bind_required_arg(i, type, name, extra) \
	lip_pp_concat(lip_bind_load_, type)(i, name, argv[i - 1]);

#define lip_bind_optional_arg(i, type, name, extra) \
	do { \
		if(i <= argc) { \
			lip_bind_required_arg(i, type, name, extra); \
		} else { \
			name = lip_pp_nth(2, extra, missing_optional); \
		} \
	} while(0);

#define lip_bind_assert_argc_at_least(arity_min) \
	lip_bind_assert_fmt( \
		argc >= arity_min, \
		"Bad number of arguments (at least %u expected, got %u)", \
		arity_min, argc \
	);

#define lip_bind_assert_argc_at_most(arity_max) \
	lip_bind_assert_fmt( \
		argc <= arity_max, \
		"Bad number of arguments (at most %u expected, got %u)", \
		arity_max, argc \
	);

#define lip_bind_assert_argc(arity) \
	lip_bind_assert_fmt( \
		argc == arity, \
		"Bad number of arguments (exactly %d expected, got %d)", \
		arity, argc \
	);

#define lip_bind_wrap_function(name, return_type, ...) \
	lip_function(lip_bind_wrapper(name)) { \
		lip_bind_args(lip_pp_map(lip_bind_wrapper_gen_binding, __VA_ARGS__)); \
		lip_pp_concat(lip_bind_store_, return_type)( \
			(*result), \
			name(lip_pp_map(lip_bind_wrapper_ref_binding, __VA_ARGS__)) \
		); \
		return LIP_EXEC_OK; \
	}

#define lip_bind_wrapper_gen_binding(i, type) lip_pp_sep(i) (type, lip_pp_concat(arg, i))
#define lip_bind_wrapper_ref_binding(i, type) lip_pp_sep(i) lip_pp_concat(arg, i)

#define lip_bind_wrapper(name) lip_pp_concat(lip_, lip_pp_concat(name, _wrapper))

#define lip_bind_declare_any(name) lip_value_t name;
#define lip_bind_load_any(i, name, value) name = value;

#define lip_bind_declare_string(name) lip_value_t name;
#define lip_bind_load_string(i, name, value) \
	do { \
		lip_bind_check_type(i, LIP_VAL_STRING, value.type); \
		name = value; \
	} while(0)

#define lip_bind_declare_number(name) double name;
#define lip_bind_load_number(i, name, value) \
	do { \
		lip_bind_check_type(i, LIP_VAL_NUMBER, value.type); \
		name = value.data.number; \
	} while(0)
#define lip_bind_store_number(target, value) \
	target = (lip_value_t){ \
		.type = LIP_VAL_NUMBER, \
		.data = { .number = value } \
	}

#define lip_bind_declare_list(name) lip_value_t name;
#define lip_bind_load_list(i, name, value) \
	do { \
		lip_bind_check_type(i, LIP_VAL_LIST, value.type); \
		name = value; \
	} while(0)

#define lip_bind_declare_function(name) lip_value_t name;
#define lip_bind_load_function(i, name, value) \
	do { \
		lip_bind_check_type(i, LIP_VAL_FUNCTION, value.type); \
		name = value; \
	} while(0)

#define lip_bind_check_type(i, expected_type, actual_type) \
	lip_bind_assert_fmt( \
		expected_type == actual_type, \
		"Bad argument #%d (%s expected, got %s)", \
		i, lip_value_type_t_to_str(expected_type), lip_value_type_t_to_str(actual_type) \
	)

#define lip_bind_assert(cond, msg) \
	do { if(!(cond)) { lip_throw(msg); } } while(0)

#define lip_bind_assert_fmt(cond, fmt, ...) \
	do { if(!(cond)) { lip_throw_fmt(fmt, __VA_ARGS__); } } while(0)

#define lip_return(val) do { *result = val; return LIP_EXEC_OK; } while(0)

#define lip_throw(err) \
		do { \
			*result = lip_make_string_copy(vm, lip_string_ref(err)); \
			lip_bind_track_native_location(vm); \
			return LIP_EXEC_ERROR; \
		} while(0)

#define lip_throw_fmt(fmt, ...) \
		do { \
			*result = lip_make_string(vm, fmt, __VA_ARGS__); \
			lip_bind_track_native_location(vm); \
			return LIP_EXEC_ERROR; \
		} while(0)

#endif
