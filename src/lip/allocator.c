#include "allocator.h"

void* lip_crt_realloc(void* context, void* old, size_t size)
{
	return realloc(old, size);
}

void lip_crt_free(void* context, void* mem) { free(mem); }

static lip_allocator_t lip_crt_allocator = {
	lip_crt_realloc, lip_crt_free
};

lip_allocator_t* lip_default_allocator = &lip_crt_allocator;
