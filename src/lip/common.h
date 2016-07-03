#ifndef LIP_COMMON_H
#define LIP_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
#define LIP_MAYBE_UNUSED __attribute__((unused))
#else
#define LIP_MAYBE_UNUSED
#endif

#define LIP_ENUM(NAME, FOREACH) \
	LIP_DEFINE_ENUM(NAME, FOREACH) \
	LIP_DEFINE_ENUM_TO_STR(NAME, FOREACH)

#define LIP_DEFINE_ENUM(NAME, FOREACH) \
	typedef enum NAME { FOREACH(LIP_GEN_ENUM) } NAME;
#define LIP_GEN_ENUM(ENUM) ENUM,

#define LIP_DEFINE_ENUM_TO_STR(NAME, FOREACH) \
	LIP_MAYBE_UNUSED static inline const char* NAME##_to_str(NAME value) { \
		switch(value) { \
			FOREACH(LIP_DEFINE_ENUM_CASE) \
			default: return 0; \
		} \
	}
#define LIP_DEFINE_ENUM_CASE(ENUM) case ENUM : return #ENUM ;

#define LIP_CONTAINER_OF(PTR, TYPE, MEMBER) \
	(TYPE*)((char*)PTR - offsetof(TYPE, MEMBER))

#define LIP_MIN(A, B) ((A) < (B) ? (A) : (B))
#define LIP_MAX(A, B) ((A) > (B) ? (A) : (B))

#define lip_array(T) T*
#define lip_static_array(T) T* const

#define LIP_STREAM(F) \
	F(LIP_STREAM_OK) \
	F(LIP_STREAM_ERROR) \
	F(LIP_STREAM_END)

LIP_ENUM(lip_stream_status_t, LIP_STREAM)

typedef struct lip_loc_s lip_loc_t;
typedef struct lip_loc_range_s lip_loc_range_t;
typedef struct lip_string_ref_s lip_string_ref_t;
typedef struct lip_string_s lip_string_t;
typedef struct lip_error_s lip_error_t;
typedef struct lip_in_s lip_in_t;
typedef struct lip_out_s lip_out_t;
typedef struct lip_allocator_s lip_allocator_t;

struct lip_loc_s
{
	unsigned int line;
	unsigned int column;
};

struct lip_loc_range_s
{
	lip_loc_t start;
	lip_loc_t end;
};

struct lip_string_ref_s
{
	size_t length;
	const char* ptr;
};

struct lip_string_s
{
	size_t length;
	char ptr[];
};

struct lip_error_s
{
	unsigned int code;
	lip_loc_range_t location;
	const void* extra;
};

static inline lip_string_ref_t
lip_string_ref(const char* string)
{
	lip_string_ref_t result = { strlen(string), string };
	return result;
}

static inline bool
lip_string_ref_equal(lip_string_ref_t lhs, lip_string_ref_t rhs)
{
	return lhs.length == rhs.length && memcmp(lhs.ptr, rhs.ptr, lhs.length) == 0;
}

#endif
