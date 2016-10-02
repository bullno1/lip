#ifndef LIP_VM_DISPATCH_H
#define LIP_VM_DISPATCH_H

#include "vm.h"

lip_exec_status_t
lip_vm_loop(lip_vm_t* vm);

lip_exec_status_t
lip_vm_do_call(lip_vm_t* vm, uint8_t num_args);

#endif
