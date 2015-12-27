#include "vm.h"
#include "asm.h"
#include <stdio.h>

void lip_vm_do_call(lip_vm_t* vm, int32_t arity);

#define LOAD_CONTEXT \
	fn = vm->ctx.closure ? vm->ctx.closure->function_ptr.lip : 0; \
	pc = vm->ctx.pc; \
	ep = vm->ctx.ep; \
	fp = vm->fp; \
	sp = vm->sp;

#define SAVE_CONTEXT \
	vm->ctx.pc = pc; \
	vm->ctx.ep = ep; \
	vm->fp = fp; \
	vm->sp = sp;

#define BEGIN_LOOP \
	while(true) { \
		lip_opcode_t opcode; \
		int32_t operand; \
		lip_disasm(*(pc++), &opcode, &operand); \
		switch(opcode) {

#define END_LOOP }}

#define BEGIN_OP(OP) case LIP_OP_##OP: {
#define END_OP(OP) } continue;

lip_exec_status_t lip_vm_loop(lip_vm_t* vm)
{
	lip_function_t* fn;
	lip_instruction_t* pc;
	lip_value_t* sp;
	lip_value_t* ep;
	lip_vm_context_t* fp;

	LOAD_CONTEXT
	BEGIN_LOOP
		BEGIN_OP(NOP)
		END_OP(NOP)

		BEGIN_OP(POP)
			--sp;
		END_OP(NOP)

		BEGIN_OP(LDC)
			*(sp++) = fn->constants[operand];
		END_OP(LDC)

		BEGIN_OP(LDL)
			*(sp++) = *(ep - operand);
		END_OP(LDL)

		BEGIN_OP(LDS)
			*(sp++) = fn->import_values[operand];
		END_OP(LDS)

		BEGIN_OP(JMP)
			pc = fn->instructions + operand;
		END_OP(JMP)

		BEGIN_OP(JOF)
			bool flag = (--sp)->data.boolean;
			pc = flag ? pc : fn->instructions + operand;
		END_OP(JOF)

		BEGIN_OP(CALL)
			SAVE_CONTEXT;
			vm->ctx.is_native = false;
			lip_vm_do_call(vm, operand);
			LOAD_CONTEXT;
		END_OP(CALL)

		BEGIN_OP(RET)
			vm->ctx = *(--fp);
			LOAD_CONTEXT;
			if(vm->ctx.is_native) { return LIP_EXEC_OK; }
		END_OP(RET)

		BEGIN_OP(CLS)
		END_OP(CLS)

		BEGIN_OP(SET)
			*(ep - operand) = *(--sp);
		END_OP(SET)
	END_LOOP
}
