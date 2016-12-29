#ifndef LIP_BIND_H
#define LIP_BIND_H

#define lip_function(name) \
	lip_exec_status_t name(lip_vm_t* vm, lip_value_t* result)

#define lip_bind_args(...) \
	uint8_t argc; lip_value_t* argv = lip_get_args(vm, &argc); (void)argv;\
	const uint8_t arity = lip_pp_len(__VA_ARGS__); \
	lip_bind_assert_fmt( \
		argc == arity, \
		"Bad number of arguments (exactly %d expected, got %d)", \
		arity, argc \
	); \
	lip_pp_map(lip_bind_arg, __VA_ARGS__)
#define lip_bind_arg(i, spec) \
	lip_bind_arg1( \
		i, \
		lip_pp_nth(1, spec, any), \
		lip_pp_nth(2, spec, junk), \
		lip_pp_nth(3, spec, required) \
	)
#define lip_bind_arg1(i, type, name, extra) \
	lip_pp_concat(lip_bind_declare_, type)(name); \
	lip_pp_concat(lip_bind_load_, type)(i, name, argv[i - 1]);

#define lip_wrap_function(name, return_type, ...) \
	lip_function(lip_wrapper(name)) { \
		lip_bind_args(lip_pp_map(lip_wrapper_gen_binding, __VA_ARGS__)); \
		lip_pp_concat(lip_bind_store_, return_type)( \
			(*result), \
			name(lip_pp_map(lip_wrapper_ref_binding, __VA_ARGS__)) \
		); \
		return LIP_EXEC_OK; \
	}

#define lip_wrapper_gen_binding(i, type) lip_pp_sep(i) (type, lip_pp_concat(arg, i))
#define lip_wrapper_ref_binding(i, type) lip_pp_sep(i) lip_pp_concat(arg, i)

#define lip_wrapper(name) lip_pp_concat(lip_, lip_pp_concat(name, _wrapper))

#define lip_bind_declare_any(name) lip_value_t name;
#define lip_bind_load_any(i, name, value) name = value;

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
			return LIP_EXEC_ERROR; \
		} while(0)
#define lip_throw_fmt(fmt, ...) \
		do { \
			*result = lip_make_string(vm, fmt, __VA_ARGS__); \
			return LIP_EXEC_ERROR; \
		} while(0)

// Terrible preprocessor abuses ahead

#define lip_pp_map(f, ...) \
	lip_pp_map1(f, lip_pp_concat(lip_pp_apply, lip_pp_len(__VA_ARGS__)), __VA_ARGS__)
#define lip_pp_map1(f, apply_n, ...) \
	apply_n(lip_pp_tail((lip_pp_seq())), f, __VA_ARGS__)
#define lip_pp_apply1(seq, f, a) f(lip_pp_head(seq), a)
#define lip_pp_apply2(seq, f, a, ...) f(lip_pp_head(seq), a) lip_pp_apply1(lip_pp_tail(seq), f, __VA_ARGS__)
#define lip_pp_apply3(seq, f, a, ...) f(lip_pp_head(seq), a) lip_pp_apply2(lip_pp_tail(seq), f, __VA_ARGS__)
#define lip_pp_apply4(seq, f, a, ...) f(lip_pp_head(seq), a) lip_pp_apply3(lip_pp_tail(seq), f, __VA_ARGS__)
#define lip_pp_apply5(seq, f, a, ...) f(lip_pp_head(seq), a) lip_pp_apply4(lip_pp_tail(seq), f, __VA_ARGS__)
#define lip_pp_apply6(seq, f, a, ...) f(lip_pp_head(seq), a) lip_pp_apply5(lip_pp_tail(seq), f, __VA_ARGS__)
#define lip_pp_apply7(seq, f, a, ...) f(lip_pp_head(seq), a) lip_pp_apply6(lip_pp_tail(seq), f, __VA_ARGS__)
#define lip_pp_apply8(seq, f, a, ...) f(lip_pp_head(seq), a) lip_pp_apply7(lip_pp_tail(seq), f, __VA_ARGS__)
#define lip_pp_apply9(seq, f, a, ...) f(lip_pp_head(seq), a) lip_pp_apply8(lip_pp_tail(seq), f, __VA_ARGS__)
#define lip_pp_apply10(seq, f, a, ...) f(lip_pp_head(seq), a) lip_pp_apply9(lip_pp_tail(seq), f, __VA_ARGS__)

#define lip_pp_len(...) lip_pp_len1(__VA_ARGS__, lip_pp_inv_seq())
#define lip_pp_len1(...) lip_pp_len2(__VA_ARGS__)
#define lip_pp_len2( \
		x15, x14, x13, x12, \
		x11, x10,  x9,  x8, \
		 x7,  x6,  x5,  x4, \
		 x3,  x2,  x1,   N, \
         ...) N
#define lip_pp_inv_seq() \
		15, 14, 13, 12, 11, 10,  9,  8, \
		 7,  6,  5,  4,  3,  2,  1,  0
#define lip_pp_seq() \
		 0,  1,  2,  3,  4,  5,  6,  7, \
		 8,  9, 10, 11, 12, 13, 14, 15

#define lip_pp_concat(a, b) lip_pp_concat1(a, b)
#define lip_pp_concat1(a, b) a ## b

#define lip_pp_nth(n, tuple, default_) \
	lip_pp_nth_(n, lip_pp_pad(tuple, default_))
#define lip_pp_nth_(n, tuple) lip_pp_concat(lip_pp_nth, n) tuple
#define lip_pp_nth1(x, ...) x
#define lip_pp_nth2(x, ...) lip_pp_nth1(__VA_ARGS__)
#define lip_pp_nth3(x, ...) lip_pp_nth2(__VA_ARGS__)
#define lip_pp_nth4(x, ...) lip_pp_nth3(__VA_ARGS__)
#define lip_pp_nth5(x, ...) lip_pp_nth4(__VA_ARGS__)
#define lip_pp_nth6(x, ...) lip_pp_nth5(__VA_ARGS__)
#define lip_pp_nth7(x, ...) lip_pp_nth6(__VA_ARGS__)
#define lip_pp_nth8(x, ...) lip_pp_nth7(__VA_ARGS__)
#define lip_pp_nth9(x, ...) lip_pp_nth8(__VA_ARGS__)
#define lip_pp_nth10(x, ...) lip_pp_nth9(__VA_ARGS__)

#define lip_pp_pad(tuple, val) (lip_pp_expand(tuple), val, val, val, val, val, val, val, val, val, val, val, val, val, val, val, val)

#define lip_pp_expand(...) lip_pp_expand1 __VA_ARGS__
#define lip_pp_expand1(...) __VA_ARGS__

#define lip_pp_head(x) lip_pp_head_ x
#define lip_pp_head_(x, ...) x

#define lip_pp_tail(x) lip_pp_tail_ x
#define lip_pp_tail_(x, ...) (__VA_ARGS__)

#define lip_pp_sep(x) lip_pp_concat(lip_pp_sep_, x)
#define lip_pp_sep_1
#define lip_pp_sep_2 ,
#define lip_pp_sep_3 ,
#define lip_pp_sep_4 ,
#define lip_pp_sep_5 ,
#define lip_pp_sep_6 ,
#define lip_pp_sep_7 ,
#define lip_pp_sep_8 ,
#define lip_pp_sep_9 ,
#define lip_pp_sep_10 ,

#endif
