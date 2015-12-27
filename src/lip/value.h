#ifndef LIP_VALUE_H
#define LIP_VALUE_H

#include <stdbool.h>
#include <stdint.h>
#include "enum.h"

#define LIP_VAL(F) \
	F(LIP_VAL_NIL) \
	F(LIP_VAL_NUMBER) \
	F(LIP_VAL_BOOLEAN) \
	F(LIP_VAL_STRING) \
	F(LIP_VAL_CONS) \
	F(LIP_VAL_CLOSURE)

LIP_ENUM(lip_value_type_t, LIP_VAL)

typedef struct lip_value_t
{
	lip_value_type_t type;
	union
	{
		void* reference;
		bool boolean;
		double number;
	} data;
} lip_value_t;

typedef struct lip_string_t
{
	uint32_t length;
	char ptr[];
} lip_string_t;

typedef struct lip_cons_t
{
	lip_value_t car;
	lip_value_t cdr;
} lip_cons_t;

void lip_value_print(lip_value_t* value, int indent);

#endif
