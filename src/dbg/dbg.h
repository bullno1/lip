#ifndef LIP_DBG_H
#define LIP_DBG_H

#include <lip/lip.h>

typedef struct lip_dbg_s lip_dbg_t;
typedef struct lip_dbg_config_s lip_dbg_config_t;

struct lip_dbg_config_s
{
	lip_allocator_t* allocator;
	lip_fs_t* fs;
	uint16_t port;
};

void
lip_reset_dbg_config(lip_dbg_config_t* cfg);

lip_dbg_t*
lip_create_debugger(lip_dbg_config_t* cfg);

void
lip_destroy_debugger(lip_dbg_t* dbg);

void
lip_attach_debugger(lip_dbg_t* dbg, lip_vm_t* vm);

#endif
