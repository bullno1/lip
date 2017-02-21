#include <lip/array.h>
#include <lip/memory.h>

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
lip_array_head(const void* ptr)
{
	const char* body = ptr;
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

	char* body = lip_align_ptr((char*)array + sizeof(lip_array_t), alignment);
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
	char old_offset = (char*)array - (char*)head;

	size_t mem_required =
		sizeof(lip_array_t) + head->elem_size * new_capacity + head->alignment - 1;
	head = lip_realloc(head->allocator, head, mem_required);
	head->capacity = new_capacity;

	char* new_body = lip_align_ptr((char*)head + sizeof(lip_array_t), head->alignment);
	memmove(new_body, (char*)head + old_offset, head->elem_size * head->length);
	*(new_body - 1) = new_body - (char*)head;

	return new_body;
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
		void* new_array = lip_array__realloc(array, LIP_MAX(head->capacity * 2, new_length));
		lip_array_head(new_array)->length = new_length;
		return new_array;
	}
}

size_t
lip_array_len(const void* array)
{
	return lip_array_head(array)->length;
}

void
lip_array_clear(void* array)
{
	lip_array_head(array)->length = 0;
}
