#include <string.h>
#include "vm.h"
#include "asm.h"

void lip_vm_do_call(lip_vm_t* vm, uint8_t num_args)
{
	lip_value_t* value = lip_vm_pop(vm);
	lip_closure_t* closure = (lip_closure_t*)value->data.reference;

	bool is_native = closure->info.is_native;
	lip_function_t* lip_function = closure->function_ptr.lip;
	size_t stack_size = is_native ? num_args : lip_function->stack_size;

	*(vm->fp++) = vm->ctx;

	// Pop arguments from operand stack into environment
	vm->ctx.ep += stack_size;
	vm->sp -= num_args;
	memcpy(
		vm->ctx.ep - num_args,
		vm->sp,
		num_args * sizeof(lip_value_t)
	);

	if(is_native)
	{
		closure->function_ptr.native(vm);
		vm->ctx = *(--vm->fp);
	}
	else
	{
		vm->ctx.pc = lip_function->instructions;
		vm->ctx.closure = closure;
	}
}

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
