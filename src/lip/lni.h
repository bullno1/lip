#ifndef LIP_LNI_H
#define LIP_LNI_H

#include "common.h"

#define LIP_NATIVE_FN(NAME, FN, ARITY) \
	{ .name = lip_string_ref(#NAME), .fn = FN, .arity = ARITY }

typedef struct lip_lni_s lip_lni_t;
typedef struct lip_native_function_s lip_native_function_t;
typedef void(*lip_native_module_entry_fn_t)(lip_lni_t*);

struct lip_lni_s
{
	void(*reg)(
		lip_lni_t* lni,
		lip_string_ref_t namespace,
		lip_native_function_t* functions,
		uint32_t num_functions
	);
};

struct lip_native_function_s
{
	lip_string_ref_t name;
	lip_native_fn_t fn;
	uint8_t arity;
	lip_value_t* environment;
	uint16_t env_len;
};

LIP_MAYBE_UNUSED static inline void
lip_lni_register(
	lip_lni_t* lni,
	lip_string_ref_t namespace,
	lip_native_function_t* functions,
	uint32_t num_functions
)
{
	lni->reg(lni, namespace, functions, num_functions);
}

#endif
