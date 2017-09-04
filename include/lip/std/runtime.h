#ifndef LIP_STD_RUNTIME_H
#define LIP_STD_RUNTIME_H

#include <lip/core.h>
#include "common.h"

/**
 * @brief Create a standard lip_runtime_config_s.
 *
 * @param allocator The allocator for the runtime config.
 * Pass NULL to use ::lip_std_allocator
 *
 * Details:
 *
 * - Use ::lip_std_allocator for lip_runtime_config_s::allocator.
 * - Initialize lip_runtime_config_s::default_vm_config with the following values:
 *   - lip_vm_config_t::os_len: 256
 *   - lip_vm_config_t::cs_len: 256
 *   - lip_vm_config_t::env_len: 256
 * - Initialize lip_runtime_config_s::module_search_patterns with the following:
 *   `?.lip`, `?.lipc`, `?/index.lip`, `?/index.lipc`, `!.lip`, `!.lipc`,
 *   '!/index.lip`, `!/index.lipc`
 *
 */
LIP_STD_API lip_runtime_config_t*
lip_create_std_runtime_config(lip_allocator_t* allocator);

/// Destroy a previously created configuration
LIP_STD_API void
lip_destroy_std_runtime_config(lip_runtime_config_t* cfg);

#endif
