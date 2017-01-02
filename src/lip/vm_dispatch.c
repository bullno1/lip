#include "vm_dispatch.h"
#include <lip/lip.h>
#include <lip/vm.h>
#include <lip/asm.h>
#include <lip/memory.h>
#include "prim_ops.h"

#if !defined(LIP_NO_COMPUTED_GOTO) && (defined(__GNUC__) || defined(__GNUG__) || defined(__clang__))
#	define GENERATE_LABEL(ENUM) &&do_##ENUM,
#	define BEGIN_LOOP() \
		void* dispatch_table[] = { LIP_OP(GENERATE_LABEL) &&do_LIP_OP_ILLEGAL }; \
		lip_opcode_t opcode; \
		lip_operand_t operand; \
		DISPATCH()
#	define END_LOOP() do_LIP_OP_ILLEGAL: THROW("Illegal instruction");
#	define BEGIN_OP(OP) do_LIP_OP_##OP: {
#	define END_OP(OP) } DISPATCH();
#	define DISPATCH() \
		CALL_HOOK(); \
		lip_disasm(*(pc++), &opcode, &operand); \
		goto *dispatch_table[LIP_MIN((unsigned int)opcode, LIP_STATIC_ARRAY_LEN(dispatch_table) - 1)];
#else
#	define BEGIN_LOOP() \
		while(true) { \
			CALL_HOOK(); \
			lip_opcode_t opcode; \
			int32_t operand; \
			lip_disasm(*(pc++), &opcode, &operand); \
			switch(opcode) {
#	define END_LOOP() default: THROW("Illegal instruction"); }}
#	define BEGIN_OP(OP) case LIP_OP_##OP: {
#	define END_OP(OP) } continue;
#endif

#define LOAD_CONTEXT() \
	fp = vm->fp; \
	lip_function_layout(fp->closure->function.lip, &fn); \
	pc = vm->fp->pc; \
	ep = vm->fp->ep; \
	bp = vm->fp->bp; \
	sp = vm->sp;

#define SAVE_CONTEXT() \
	vm->sp = sp; \
	vm->fp->pc = pc;

#define PREAMBLE() \
	lip_function_layout_t fn; \
	lip_stack_frame_t* fp; \
	lip_instruction_t* pc; \
	lip_value_t* bp; \
	lip_value_t* ep; \
	lip_value_t* sp; \
	LOAD_CONTEXT() \
	BEGIN_LOOP()

#define POSTAMBLE() \
	END_LOOP()

#define THROW(MSG) \
	do { \
		*(--sp) = lip_make_string_copy(vm, lip_string_ref(MSG)); \
		SAVE_CONTEXT(); \
		return LIP_EXEC_ERROR; \
	} while(0)

#define THROW_FMT(MSG, ...) \
	do { \
		*(--sp) = lip_make_string(vm, MSG, __VA_ARGS__); \
		SAVE_CONTEXT(); \
		return LIP_EXEC_ERROR; \
	} while(0)

#define DO_PRIM_OP(op, name) \
	BEGIN_OP(name) \
		lip_exec_status_t status = lip_ ## name (vm, sp + operand - 1, operand, sp); \
		sp += operand - 1; \
		if(status != LIP_EXEC_OK) { SAVE_CONTEXT(); return status; } \
	END_OP(name)

static lip_exec_status_t
lip_vm_loop_with_hook(lip_vm_t* vm)
{
#define CALL_HOOK() SAVE_CONTEXT(); vm->hook->step(vm->hook, vm);
PREAMBLE()
#include "vm_ops"
POSTAMBLE()
#undef CALL_HOOK
}

static lip_exec_status_t
lip_vm_loop_without_hook(lip_vm_t* vm)
{
#define CALL_HOOK()
PREAMBLE()
#include "vm_ops"
POSTAMBLE()
#undef CALL_HOOK
}

lip_exec_status_t
lip_vm_loop(lip_vm_t* vm)
{
	return vm->hook
		? lip_vm_loop_with_hook(vm)
		: lip_vm_loop_without_hook(vm);
}

lip_exec_status_t
lip_vm_do_call(lip_vm_t* vm, lip_value_t* fn, uint8_t num_args)
{
	if(LIP_UNLIKELY(fn->type != LIP_VAL_FUNCTION))
	{
		lip_value_t* next_sp = vm->sp + num_args - 1;
		*next_sp = lip_make_string_copy(
			vm, lip_string_ref("Trying to call a non-function")
		);
		vm->sp = next_sp;
		return LIP_EXEC_ERROR;
	}

	vm->fp->num_args = num_args;
	vm->fp->bp = vm->sp;
	lip_closure_t* closure = (lip_closure_t*)fn->data.reference;
	vm->fp->closure = closure;

	bool is_native = closure->is_native;
	unsigned int num_locals = is_native ? 0 : closure->function.lip->num_locals;
	vm->fp->ep -= num_locals;

	if(is_native)
	{
		// Ensure that a value is always returned
		lip_value_t* next_sp = vm->sp + num_args - 1;
		lip_exec_status_t status = closure->function.native(vm, next_sp);
		vm->sp = next_sp;
		if(status == LIP_EXEC_OK) { --vm->fp; }

		return status;
	}
	else
	{
		lip_function_layout_t layout;
		lip_function_layout(closure->function.lip, &layout);
		vm->fp->pc = layout.instructions;

		const uint8_t arity = closure->function.lip->num_args;
		if(LIP_UNLIKELY(arity != num_args))
		{
			lip_value_t* next_sp = vm->sp + num_args - 1;
			*next_sp = lip_make_string(
				vm,
				"Bad number of arguments (exactly %u expected, got %u)",
				arity, num_args
			);
			vm->sp = next_sp;
			return LIP_EXEC_ERROR;
		}

		return LIP_EXEC_OK;
	}
}
