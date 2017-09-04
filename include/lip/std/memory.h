#ifndef LIP_STD_MEMORY_H
#define LIP_STD_MEMORY_H

#include "common.h"

/// An allocator that uses `realloc` and `free`
LIP_STD_API lip_allocator_t* const lip_std_allocator;

#endif
