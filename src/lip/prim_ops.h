#ifndef LIP_PRIM_OPS_H
#define LIP_PRIM_OPS_H

#include <lip/common.h>
#include <lip/bind.h>

#define LIP_PRIM_OP(F) \
	F(+, ADD) \
	F(-, SUB) \
	F(*, MUL) \
	F(/, FDIV) \
	F(!, NOT) \
	F(cmp, CMP) \
	LIP_CMP_OP(F)

#define LIP_CMP_OP(F) \
	F(==, EQ) \
	F(!=, NEQ) \
	F(>, GT) \
	F(<, LT) \
	F(>=, GTE) \
	F(<=, LTE) \

#define LIP_PRIM_OP_FN_NAME(name) lip_pp_concat(lip_, name)
#define LIP_PRIM_OP_FN(name) \
	lip_exec_status_t LIP_PRIM_OP_FN_NAME(name)( \
		lip_vm_t* vm, lip_value_t* result, \
		unsigned int argc, const lip_value_t* argv \
	)

#define LIP_DECLARE_PRIM_OP(op, name) \
	LIP_PRIM_OP_FN(name);

LIP_PRIM_OP(LIP_DECLARE_PRIM_OP)

#endif
