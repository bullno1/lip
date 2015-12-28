#ifndef LIP_OPCODE_H
#define LIP_OPCODE_H

#include "enum.h"

#define LIP_OP(F) \
	F(LIP_OP_NOP) \
	F(LIP_OP_POP) \
	F(LIP_OP_LDC) \
	F(LIP_OP_LDL) \
	F(LIP_OP_LDS) \
	F(LIP_OP_LDI) \
	F(LIP_OP_NIL) \
	F(LIP_OP_SET) \
	F(LIP_OP_JMP) \
	F(LIP_OP_JOF) \
	F(LIP_OP_CALL) \
	F(LIP_OP_RET) \
	F(LIP_OP_PLUS) \
	F(LIP_OP_LT) \
	F(LIP_OP_CLS)

LIP_ENUM(lip_opcode_t, LIP_OP)

typedef int32_t lip_instruction_t;

#endif
