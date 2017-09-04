#ifndef LIP_CORE_MODULE_H
#define LIP_CORE_MODULE_H

#include "common.h"
#include "extra.h"

typedef struct lip_module_info_s lip_module_info_t;
typedef struct lip_module_runtime_s lip_module_runtime_t;

struct kk

struct lip_module_info_s
{
	size_t num_dependencies;
	lip_string_ref_t* dependencies;
};

typedef lip_error_m(lip_module_info_t) lip_module_preload_t;

struct lip_module_loader_s
{
	lip_module_preload_t(*begin_load)(
		lip_module_loader_t* loader, lip_string_ref_t name
	);
	bool(*initialize)(lip_module_loader_t* loader);
	bool(*load)(lip_module_loader_t* loader);
	void(*end_load)(lip_module_loader_t* loader);
};

#endif
