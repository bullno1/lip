PROLOG
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

	BEGIN_OP(LDI)
		lip_value_t* value = sp++;
		value->type = LIP_VAL_NUMBER;
		value->data.number = operand;
	END_OP(LDI)

	BEGIN_OP(JMP)
		pc = fn->instructions + operand;
	END_OP(JMP)

	BEGIN_OP(JOF)
		bool flag = (--sp)->data.boolean;
		pc = flag ? pc : fn->instructions + operand;
	END_OP(JOF)

	BEGIN_OP(CALL)
		SAVE_CONTEXT
		*(vm->fp++) = vm->ctx;
		lip_vm_do_call(vm, operand);
		LOAD_CONTEXT
	END_OP(CALL)

	BEGIN_OP(RET)
		vm->ctx = *(--vm->fp);
		if(vm->ctx.is_native) { return LIP_EXEC_OK; }
		LOAD_CONTEXT
	END_OP(RET)

	BEGIN_OP(CLS)
	END_OP(CLS)

	BEGIN_OP(SET)
		*(ep - operand) = *(--sp);
	END_OP(SET)
END_LOOP
