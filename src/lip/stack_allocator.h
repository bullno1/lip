#ifndef LIP_STACK_ALLOCATOR_H
#define LIP_STACK_ALLOCATOR_H

#include "allocator.h"

lip_allocator_t* lip_stack_allocator_new(lip_allocator_t* allocator, size_t size);
void lip_stack_allocator_delete(lip_allocator_t* allocator);
void lip_stack_allocator_reset(lip_allocator_t* allocator);

#endif
