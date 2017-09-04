#ifndef LIP_DBG_H
#define LIP_DBG_H

#include <lip/core.h>

#if LIP_DYNAMIC == 1
#	ifdef _WIN32
#		ifdef LIP_DBG_BUILDING
#			define LIP_DBG_DECL __declspec(dllexport)
#		else
#			define LIP_DBG_DECL __declspec(dllimport)
#		endif
#	else
#		ifdef LIP_DBG_BUILDING
#			define LIP_DBG_DECL __attribute__((visibility("default")))
#		else
#			define LIP_DBG_DECL
#		endif
#	endif
#else
#	define LIP_DBG_DECL
#endif

#define LIP_DBG_API LIP_LINKAGE LIP_DBG_DECL

typedef struct lip_dbg_s lip_dbg_t;
typedef struct lip_dbg_config_s lip_dbg_config_t;

struct lip_dbg_config_s
{
	lip_allocator_t* allocator;
	lip_fs_t* fs;
	uint16_t port;
	bool hook_step;
};

LIP_DBG_API lip_dbg_t*
lip_create_debugger(lip_dbg_config_t* cfg);

LIP_DBG_API void
lip_destroy_debugger(lip_dbg_t* dbg);

LIP_DBG_API void
lip_attach_debugger(lip_dbg_t* dbg, lip_vm_t* vm);

#endif
