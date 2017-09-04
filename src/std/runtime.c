#include <lip/std/runtime.h>
#include <lip/std/memory.h>
#include <lip/std/io.h>
#include <lip/core/memory.h>

#define LIP_STRING_REF_LITERAL(str) \
	{ .length = LIP_STATIC_ARRAY_LEN(str), .ptr = str}

static const lip_string_ref_t LIP_DEFAULT_MODULE_SEARCH_PATTERNS[] = {
	LIP_STRING_REF_LITERAL("?.lip"),
	LIP_STRING_REF_LITERAL("?.lipc"),
	LIP_STRING_REF_LITERAL("!.lip"),
	LIP_STRING_REF_LITERAL("!.lipc"),
	LIP_STRING_REF_LITERAL("?/init.lip"),
	LIP_STRING_REF_LITERAL("?/init.lipc"),
	LIP_STRING_REF_LITERAL("!/init.lip"),
	LIP_STRING_REF_LITERAL("!/init.lipc")
};


lip_runtime_config_t*
lip_create_std_runtime_config(lip_allocator_t* allocator)
{
	if(allocator == NULL) { allocator = lip_std_allocator; }

	lip_runtime_config_t* cfg = lip_new(allocator, lip_runtime_config_t);
	*cfg = (lip_runtime_config_t){
		.allocator = lip_std_allocator,
		.default_vm_config = {
			.os_len = 256,
			.cs_len = 256,
			.env_len = 256
		},
		.module_search_patterns = LIP_DEFAULT_MODULE_SEARCH_PATTERNS,
		.num_module_search_patterns = LIP_STATIC_ARRAY_LEN(LIP_DEFAULT_MODULE_SEARCH_PATTERNS),
		.fs = lip_create_std_fs(allocator)
	};

	return cfg;
}

void
lip_destroy_std_runtime_config(lip_runtime_config_t* cfg)
{
	lip_destroy_std_fs(cfg->fs);
	lip_free(cfg->allocator, cfg);
}
