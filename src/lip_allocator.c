#include "lip_allocator.h"

void* lip_crt_malloc(void* context, size_t size) { return malloc(size); }
void lip_crt_free(void* context, void* mem) { free(mem); }

static lip_allocator_vtable_t lip_crt_allocator_vtable = {
	lip_crt_malloc, lip_crt_free
};

lip_allocator_t lip_default_allocator = {
	NULL,
	&lip_crt_allocator_vtable
};
