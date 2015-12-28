#if defined(__GNUC__) || defined(__GNUG__)
#	define HAS_COMPUTED_GOTO 1
#else
#	define HAS_COMPUTED_GOTO 0
#endif

#define LOAD_CONTEXT() \
	fn = vm->ctx.closure->function_ptr.lip; \
	pc = vm->ctx.pc; \
	ep = vm->ctx.ep; \
	sp = vm->sp;

#define SAVE_CONTEXT() \
	vm->ctx.pc = pc; \
	vm->sp = sp;

#if USE_HOOK
#	define CALL_HOOK() SAVE_CONTEXT(); vm->hook(vm, vm->hook_ctx);
#else
#	define CALL_HOOK()
#endif

#if HAS_COMPUTED_GOTO

#	define GENERATE_LABEL(ENUM) &&do_##ENUM,

#	define BEGIN_LOOP \
	void* dispatch_table[] = { LIP_OP(GENERATE_LABEL) }; \
	lip_opcode_t opcode; \
	int32_t operand; \
	DISPATCH()

#	define END_LOOP

#	define BEGIN_OP(OP) do_LIP_OP_##OP: {
#	define END_OP(OP) } DISPATCH();
#	define DISPATCH() \
	CALL_HOOK(); \
	lip_disasm(*(pc++), &opcode, &operand); \
	goto *dispatch_table[opcode];

#else
#	define BEGIN_LOOP \
	while(true) { \
		CALL_HOOK(); \
		lip_opcode_t opcode; \
		int32_t operand; \
		lip_disasm(*(pc++), &opcode, &operand); \
		switch(opcode) {

#	define END_LOOP }}

#	define BEGIN_OP(OP) case LIP_OP_##OP: {
#	define END_OP(OP) } continue;

#endif

lip_function_t* fn;
lip_instruction_t* pc;
lip_value_t* ep;
lip_value_t* sp;

LOAD_CONTEXT()
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

	BEGIN_OP(LDI)
		lip_value_t* value = sp++;
		value->type = LIP_VAL_NUMBER;
		value->data.number = operand;
	END_OP(LDI)

	BEGIN_OP(NIL)
		(sp++)->type = LIP_VAL_NIL;
	END_OP(NIL)

	BEGIN_OP(JMP)
		pc = fn->instructions + operand;
	END_OP(JMP)

	BEGIN_OP(JOF)
		bool flag = (--sp)->data.boolean;
		pc = flag ? pc : fn->instructions + operand;
	END_OP(JOF)

	BEGIN_OP(CALL)
		SAVE_CONTEXT();
		*(vm->fp++) = vm->ctx;
		lip_vm_do_call(vm, operand);
		LOAD_CONTEXT();
	END_OP(CALL)

	BEGIN_OP(RET)
		SAVE_CONTEXT();
		vm->ctx = *(--vm->fp);
		if(vm->ctx.is_native) { return LIP_EXEC_OK; }
		LOAD_CONTEXT();
	END_OP(RET)

	BEGIN_OP(LT)
		lip_value_t* lhs = sp - 1;
		lip_value_t* rhs = sp - 2;
		lip_value_t result = {
			.type = LIP_VAL_BOOLEAN,
			.data = { .boolean = lhs->data.number < rhs->data.number }
		};
		--sp;
		*(sp - 1) = result;
	END_OP(LT)

	BEGIN_OP(PLUS)
		double acc = 0.0;
		for(int i = 0; i < operand; ++i)
		{
			acc += (sp - 1 - i)->data.number;
		}
		lip_value_t result = {
			.type = LIP_VAL_NUMBER,
			.data = { .number = acc }
		};
		sp -= (operand - 1);
		*(sp - 1) = result;
	END_OP(PLUS)

	BEGIN_OP(CLS)
	END_OP(CLS)

	BEGIN_OP(SET)
		*(ep - operand) = *(--sp);
	END_OP(SET)
END_LOOP

#undef LOAD_CONTEXT
#undef SAVE_CONTEXT
#undef BEGIN_LOOP
#undef END_LOOP
#undef BEGIN_OP
#undef END_OP
#undef CALL_HOOK
#undef DISPATCH
#undef GENERATE_LABEL
