#include <stdlib.h>
#include "memory.h"

static void*
lip_crt_realloc(void* context, void* old, size_t size)
{
	(void)context;
	return realloc(old, size);
}

static void
lip_crt_free(void* context, void* mem)
{
	(void)context;
	free(mem);
}

static lip_allocator_t lip_crt_allocator = {
	.realloc = lip_crt_realloc,
	.free = lip_crt_free
};

lip_allocator_t* lip_default_allocator = &lip_crt_allocator;
