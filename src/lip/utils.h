#ifndef LIP_UTILS_H
#define LIP_UTILS_H

#include "memory.h"
#include "ex/vm.h"
#include "vendor/xxhash.h"

#define lip_string_ref_hash(str) XXH32(str.ptr, str.length, __LINE__)

#define LIP_IMPLEMENT_CONSTRUCTOR_AND_DESTRUCTOR(TYPE) \
	LIP_IMPLEMENT_CONSTRUCTOR(TYPE) \
	LIP_IMPLEMENT_DESTRUCTOR(TYPE)

#define LIP_IMPLEMENT_CONSTRUCTOR(TYPE) \
	TYPE##_t* TYPE##_create(lip_allocator_t* allocator) { \
		TYPE##_t* instance = lip_new(allocator, TYPE##_t); \
		TYPE##_init(instance, allocator); \
		return instance; \
	}

#define LIP_IMPLEMENT_DESTRUCTOR(TYPE) \
	void TYPE##_destroy(TYPE##_t* instance) { \
		TYPE##_cleanup(instance); \
		lip_free(instance->allocator, instance); \
	}

#endif
