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
		lip_value_t* target = operand > 0 ? ep - operand : vm->ctx.closure->environment - operand;
		*(sp++) = *target;
	END_OP(LDL)

	BEGIN_OP(LDS)
		*(sp++) = fn->import_values[operand];
	END_OP(LDS)

	BEGIN_OP(LDI)
		lip_value_t* value = sp++;
		value->type = LIP_VAL_NUMBER;
		value->data.number = operand;
	END_OP(LDI)

	BEGIN_OP(PLHR)
		lip_value_t* value = ep - operand;
		value->type = LIP_VAL_PLACEHOLDER;
		value->data.integer = operand;
	END_OP(PLHR)

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

	BEGIN_OP(TAIL)
		SAVE_CONTEXT();
		vm->ctx.ep = (vm->fp - 1)->ep;
		lip_vm_do_call(vm, operand);
		if(vm->ctx.is_native) { return LIP_EXEC_OK; }
		LOAD_CONTEXT();
	END_OP(TAIL)

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
		unsigned int function_index = operand & 0xFFF;
		unsigned int num_captures = (operand >> 12) & 0xFFF;
		size_t environment_size = num_captures * sizeof(lip_value_t);
		size_t closure_size = sizeof(lip_closure_t) + environment_size;
		lip_closure_t* closure = lip_malloc(vm->config.allocator, closure_size);
		closure->info.is_native = false;
		closure->function_ptr.lip = fn->functions[function_index];
		closure->environment_size = environment_size / sizeof(lip_value_t);
		for(unsigned int i = 0; i < num_captures; ++i)
		{
			lip_opcode_t opcode;
			int32_t local_index;
			lip_disasm(pc[i], &opcode, &local_index);
			lip_value_t* target = local_index > 0 ? ep - local_index : vm->ctx.closure->environment - local_index;
			closure->environment[i] = *target;
		}
		pc += num_captures;
		lip_value_t value = {
			.type = LIP_VAL_CLOSURE,
			.data = { .reference = closure }
		};
		*(sp++) = value;
	END_OP(CLS)

	BEGIN_OP(RCLS)
		lip_value_t* target = ep - operand;
		if(target->type == LIP_VAL_CLOSURE)
		{
			lip_closure_t* closure = target->data.reference;
			for(unsigned int i = 0; i < closure->environment_size; ++i)
			{
				lip_value_t* captured_val = &closure->environment[i];
				if(captured_val->type == LIP_VAL_PLACEHOLDER)
				{
					*captured_val = *(ep - captured_val->data.integer);
				}
			}
		}
	END_OP(RCLS)

	BEGIN_OP(SET)
		lip_value_t* target = operand > 0 ? ep - operand : vm->ctx.closure->environment - operand;
		*target = *(--sp);
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
