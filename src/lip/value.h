#ifndef LIP_VALUE_H
#define LIP_VALUE_H

#include <stdbool.h>
#include <stdint.h>
#include "enum.h"
#include "types.h"

#define LIP_VAL(F) \
	F(LIP_VAL_NIL) \
	F(LIP_VAL_NUMBER) \
	F(LIP_VAL_BOOLEAN) \
	F(LIP_VAL_STRING) \
	F(LIP_VAL_CONS) \
	F(LIP_VAL_CLOSURE) \
	F(LIP_VAL_PLACEHOLDER)

LIP_ENUM(lip_value_type_t, LIP_VAL)

typedef struct lip_value_t
{
	lip_value_type_t type;
	union
	{
		int32_t integer;
		void* reference;
		bool boolean;
		double number;
	} data;
} lip_value_t;

typedef struct lip_cons_t
{
	lip_value_t car;
	lip_value_t cdr;
} lip_cons_t;

void lip_value_print(
	lip_write_fn_t write_fn, void* ctx,
	lip_value_t* value, int max_depth, int indent
);

#endif
