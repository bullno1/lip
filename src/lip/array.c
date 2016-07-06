#include "array.h"
#include "memory.h"

typedef struct lip_array_s
{
	lip_allocator_t* allocator;
	size_t elem_size;
	size_t length;
	size_t capacity;
	uint8_t alignment;
	uint8_t padding;
} lip_array_t;

static inline lip_array_t*
lip_array_head(void* ptr)
{
	char* body = ptr;
	return (lip_array_t*)(body - (*(body - 1)));
}

void*
lip_array__create(
	lip_allocator_t* allocator,
	size_t elem_size,
	uint8_t alignment,
	size_t capacity
)
{
	size_t mem_required = sizeof(lip_array_t) + elem_size * capacity + alignment - 1;
	lip_array_t* array = lip_malloc(allocator, mem_required);
	array->allocator = allocator;
	array->elem_size = elem_size;
	array->length = 0;
	array->capacity = capacity;
	array->alignment = alignment;

	char* head_end = (char*)array + sizeof(lip_array_t);
	char* body = lip_align_ptr(head_end, alignment);
	char offset = body - (char*)array;
	*(body - 1) = offset;
	return body;
}

void
lip_array_destroy(void* array)
{
	lip_array_t* head = lip_array_head(array);
	lip_free(head->allocator, head);
}

static
void* lip_array__realloc(void* array, size_t new_capacity)
{
	lip_array_t* head = lip_array_head(array);
	void* new_array = lip_array__create(
		head->allocator,
		head->elem_size,
		head->alignment,
		new_capacity
	);
	memcpy(new_array, array, head->length * head->elem_size);
	lip_free(head->allocator, head);
	return new_array;
}

void*
lip_array__resize(void* array, size_t new_length)
{
	lip_array_t* head = lip_array_head(array);
	if(new_length <= head->capacity)
	{
		head->length = new_length;
		return array;
	}
	else
	{
		void* new_array = lip_array__realloc(array, new_length);
		lip_array_head(new_array)->length = new_length;
		return new_array;
	}
}

void*
lip_array__prepare_push(void* array)
{
	lip_array_t* head = lip_array_head(array);
	size_t new_length = head->length + 1;
	if(new_length <= head->capacity)
	{
		head->length = new_length;
		return array;
	}
	else
	{
		void* new_array = lip_array__realloc(array, head->capacity * 2);
		lip_array_head(new_array)->length = new_length;
		return new_array;
	}
}

size_t
lip_array_len(void* array)
{
	return lip_array_head(array)->length;
}

void
lip_array_clear(void* array)
{
	lip_array_head(array)->length = 0;
}
