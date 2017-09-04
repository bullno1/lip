#ifndef LIP_CORE_EXTRA_H
#define LIP_CORE_EXTRA_H

#include "common.h"
#include "vendor/khash.h"

KHASH_DECLARE(lip_string_ref_set, lip_string_ref_t, char)

#define LIP_STREAM(F) \
	F(LIP_STREAM_OK) \
	F(LIP_STREAM_ERROR) \
	F(LIP_STREAM_END)

LIP_ENUM(lip_stream_status_t, LIP_STREAM)

#define lip_error_m(T) \
	struct { \
		bool success; \
		union { \
			T result; \
			lip_error_t error; \
		} value; \
	}

typedef struct lip_function_s lip_function_t;
typedef struct lip_closure_s lip_closure_t;
typedef struct lip_error_s lip_error_t;
typedef struct lip_last_error_s lip_last_error_t;
typedef struct lip_error_m_s lip_error_m;

struct lip_error_s
{
	unsigned int code;
	lip_loc_range_t location;
	const void* extra;
};

struct lip_last_error_s
{
	lip_error_t error;
	lip_error_t* errorp;
};

#endif
