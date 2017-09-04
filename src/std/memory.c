#include <lip/core/memory.h>
#include <stdlib.h>

static void*
lip_crt_realloc(lip_allocator_t* self, void* old, size_t size)
{
	(void)self;
	return realloc(old, size);
}

static void
lip_crt_free(lip_allocator_t* self, void* mem)
{
	(void)self;
	free(mem);
}
static lip_allocator_t lip_crt_allocator = {
	.realloc = lip_crt_realloc,
	.free = lip_crt_free
};

lip_allocator_t* const lip_std_allocator = &lip_crt_allocator;
