#include "common.h"
#include "memory.h"
#include "ex/lip.h"
#include "vendor/khash.h"
#include "vendor/xxhash.h"

#define lip_string_ref_hash(str) XXH32(str.ptr, str.length, __LINE__)
#define lip_value_hash(ptr) XXH32(&ptr, sizeof(ptr), __LINE__)
#define lip_value_equal(lhs, rhs) ((lhs) == (rhs))

__KHASH_IMPL(
	lip_string_ref_set,
	,
	lip_string_ref_t,
	char,
	0,
	lip_string_ref_hash,
	lip_string_ref_equal
)

__KHASH_IMPL(
	lip_ns,
	,
	lip_string_ref_t,
	lip_symbol_t,
	1,
	lip_string_ref_hash,
	lip_string_ref_equal
)

__KHASH_IMPL(
	lip_symtab,
	,
	lip_string_ref_t,
	khash_t(lip_ns)*,
	1,
	lip_string_ref_hash,
	lip_string_ref_equal
)

__KHASH_IMPL(
	lip_ptr_set,
	,
	void*,
	char,
	0,
	lip_value_hash,
	lip_value_equal
)
