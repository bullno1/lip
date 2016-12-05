#ifndef LIP_COMMON_EX_H
#define LIP_COMMON_EX_H

#include "../common.h"

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

LIP_MAYBE_UNUSED static inline void
lip_set_last_error(
	lip_last_error_t* last_error,
	unsigned int code, lip_loc_range_t location, const void* extra
)
{
	last_error->errorp = &last_error->error;
	last_error->error.code = code;
	last_error->error.location = location;
	last_error->error.extra = extra;
}

LIP_MAYBE_UNUSED static inline void
lip_clear_last_error(lip_last_error_t* last_error)
{
	last_error->errorp = NULL;
}

LIP_MAYBE_UNUSED static inline const lip_error_t*
lip_last_error(lip_last_error_t* last_error)
{
	return last_error->errorp;
}

#endif
