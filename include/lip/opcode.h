#ifndef LIP_OPCODE_H
#define LIP_OPCODE_H

#include "common.h"

#define LIP_LDI_MIN -8388608
#define LIP_LDI_MAX 8388607

#define LIP_OP(F) \
	F(LIP_OP_NOP) \
	F(LIP_OP_POP) \
	F(LIP_OP_NIL) \
	F(LIP_OP_LDK) \
	F(LIP_OP_LDI) \
	F(LIP_OP_LDB) \
	F(LIP_OP_PLHR) \
	F(LIP_OP_LARG) \
	F(LIP_OP_LDLV) \
	F(LIP_OP_LDCV) \
	F(LIP_OP_IMP) \
	F(LIP_OP_SET) \
	F(LIP_OP_JMP) \
	F(LIP_OP_JOF) \
	F(LIP_OP_CALL) \
	F(LIP_OP_TAIL) \
	F(LIP_OP_RET) \
	F(LIP_OP_CLS) \
	F(LIP_OP_RCLS) \
	F(LIP_OP_ADD) \
	F(LIP_OP_SUB) \
	F(LIP_OP_MUL) \
	F(LIP_OP_FDIV) \
	F(LIP_OP_NOT) \
	F(LIP_OP_CMP) \
	F(LIP_OP_EQ) \
	F(LIP_OP_NEQ) \
	F(LIP_OP_GT) \
	F(LIP_OP_LT) \
	F(LIP_OP_GTE) \
	F(LIP_OP_LTE)

LIP_ENUM(lip_opcode_t, LIP_OP)

typedef int32_t lip_instruction_t;
typedef int32_t lip_operand_t;

#endif
