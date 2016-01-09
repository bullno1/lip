#include "hash.h"
#include "murmurhash.h"

khint_t lip_string_ref_hash(lip_string_ref_t str)
{
	return murmurhash(str.ptr, str.length, 0xB19B00B5);
}
