#include "array.h"
#include "allocator.h"

typedef struct
{
	lip_allocator_t* allocator;
	size_t length;
	size_t capacity;
} lip_array_t;

static inline lip_array_t* lip_array_head(void* ptr)
{
	return (lip_array_t*)((char*)ptr - sizeof(lip_array_t));
}

static inline void* lip_array_body(lip_array_t* head)
{
	return (char*)head + sizeof(lip_array_t);
}

void* lip_array_new(lip_allocator_t* allocator)
{
	lip_array_t* head = lip_new(allocator, lip_array_t);
	head->allocator = allocator;
	head->length = 0;
	head->capacity = sizeof(lip_array_t);
	return lip_array_body(head);
}

void lip_array_delete(void* array)
{
	lip_array_t* head = lip_array_head(array);
	lip_free(head->allocator, head);
}

void* lip_array__resize(void* array, size_t new_length, size_t item_size)
{
	lip_array_t* head = lip_array_head(array);
	size_t required_size = new_length * item_size + sizeof(lip_array_t);
	if(required_size > head->capacity)
	{
		size_t new_capacity = required_size * 2;
		head = lip_realloc(head->allocator, head, new_capacity);
		head->capacity = new_capacity;
	}
	head->length = new_length;
	return lip_array_body(head);
}

size_t lip_array_len(void* array)
{
	return lip_array_head(array)->length;
}

void lip_array_clear(void* array)
{
	lip_array_head(array)->length = 0;
}
