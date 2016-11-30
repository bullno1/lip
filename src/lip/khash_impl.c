#include "common.h"
#include "vendor/khash.h"
#include "vendor/xxhash.h"
#include "ex/compiler.h"
#include "ex/universe.h"

#define lip_string_ref_hash(str) XXH32(str.ptr, str.length, __LINE__)

__KHASH_IMPL(
	lip_string_ref_set,
	LIP_MAYBE_UNUSED,
	lip_string_ref_t,
	char,
	0,
	lip_string_ref_hash,
	lip_string_ref_equal
)

__KHASH_IMPL(
	lip_namespace,
	LIP_MAYBE_UNUSED,
	lip_string_ref_t,
	lip_value_t,
	1,
	lip_string_ref_hash,
	lip_string_ref_equal
)

__KHASH_IMPL(
	lip_environment,
	LIP_MAYBE_UNUSED,
	lip_string_ref_t,
	khash_t(lip_namespace)*,
	1,
	lip_string_ref_hash,
	lip_string_ref_equal
)
