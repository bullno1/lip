#include "ex/vm.h"
#include "memory.h"
#include "vm_dispatch.h"

lip_vm_t*
lip_vm_create(lip_allocator_t* allocator, lip_vm_config_t* config)
{
	lip_memblock_info_t vm_block = {
		.element_size = sizeof(lip_vm_t),
		.num_elements = 1,
		.alignment = LIP_ALIGN_OF(lip_vm_t)
	};

	lip_memblock_info_t vm_mem_block = {
		.element_size = lip_vm_memory_required(config),
		.num_elements = 1,
		.alignment = LIP_MAX_ALIGNMENT
	};

	lip_memblock_info_t* vm_layout[] = { &vm_block, &vm_mem_block };
	lip_memblock_info_t block_info =
		lip_align_memblocks(LIP_STATIC_ARRAY_LEN(vm_layout), vm_layout);

	void* mem = lip_malloc(allocator, block_info.num_elements);
	lip_vm_t* vm = lip_locate_memblock(mem, &vm_block);
	void* vm_mem = lip_locate_memblock(mem, &vm_mem_block);
	lip_vm_init(vm, config, vm_mem);

	return vm;
}

void
lip_vm_destroy(lip_allocator_t* allocator, lip_vm_t* vm)
{
	lip_free(allocator, vm);
}

static inline lip_memblock_info_t
lip_vm_memory_layout(
	lip_vm_config_t* config,
	lip_memblock_info_t* os_block,
	lip_memblock_info_t* env_block,
	lip_memblock_info_t* cs_block
)
{
	os_block->element_size = sizeof(lip_value_t);
	os_block->num_elements = config->os_len;
	os_block->alignment = LIP_ALIGN_OF(lip_value_t);

	env_block->element_size = sizeof(lip_value_t);
	env_block->num_elements = config->env_len;
	env_block->alignment = LIP_ALIGN_OF(lip_value_t);

	cs_block->element_size = sizeof(lip_stack_frame_t);
	cs_block->num_elements = config->cs_len;
	cs_block->alignment = LIP_ALIGN_OF(lip_stack_frame_t);

	lip_memblock_info_t* mem_layout[] = { os_block, env_block, cs_block };
	return lip_align_memblocks(LIP_STATIC_ARRAY_LEN(mem_layout), mem_layout);
}

size_t
lip_vm_memory_required(lip_vm_config_t* config)
{
	lip_memblock_info_t os_block, env_block, cs_block;
	lip_memblock_info_t mem_layout =
		lip_vm_memory_layout(config, &os_block, &env_block, &cs_block);

	return mem_layout.num_elements;
}

void
lip_vm_reset(lip_vm_t* vm)
{
	lip_vm_init(vm, &vm->config, vm->mem);
}

void
lip_vm_init(lip_vm_t* vm, lip_vm_config_t* config, void* mem)
{
	vm->config = *config;
	vm->hook = NULL;
	vm->mem = mem;

	lip_memblock_info_t os_block, env_block, cs_block;
	lip_vm_memory_layout(config, &os_block, &env_block, &cs_block);
	vm->sp = (lip_value_t*)lip_locate_memblock(mem, &os_block) + config->os_len;
	vm->fp = lip_locate_memblock(mem, &cs_block);
	vm->fp->ep = (lip_value_t*)lip_locate_memblock(mem, &env_block) + config->env_len;
	vm->fp->is_native = false;
	vm->fp->pc = NULL;
	vm->fp->closure = NULL;
}

void
lip_vm_push_value(lip_vm_t* vm, lip_value_t value)
{
	*(--vm->sp) = value;
}

lip_exec_status_t
lip_vm_call(lip_vm_t* vm, uint8_t num_args, lip_value_t* result)
{
	lip_stack_frame_t* old_fp = vm->fp;
	vm->fp->is_native = true;
	*(++vm->fp) = *old_fp;

	lip_exec_status_t status = lip_vm_do_call(vm, num_args);
	if(status != LIP_EXEC_OK) { return status; }

	if(vm->fp == old_fp)
	{
		status = LIP_EXEC_OK;
	}
	else
	{
		status = lip_vm_loop(vm);
	}

	++vm->sp;
	*result = *(vm->sp - 1);
	return status;
}

lip_value_t*
lip_vm_get_args(lip_vm_t* vm, uint8_t* num_args)
{
	if(num_args) { *num_args = vm->fp->num_args; }
	return vm->fp->ep;
}

lip_value_t*
lip_vm_get_env(lip_vm_t* vm, uint8_t* env_len)
{
	if(env_len) { *env_len = vm->fp->closure->env_len; }
	return vm->fp->closure->environment;
}
