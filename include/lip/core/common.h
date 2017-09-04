#ifndef LIP_CORE_COMMON_H
#define LIP_CORE_COMMON_H

/**
 * @defgroup common Common
 * @brief Common structures and functions
 *
 * @{
 */

#include <lip/config.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#if LIP_DYNAMIC == 1
#	ifdef _WIN32
#		ifdef LIP_CORE_BUILDING
#			define LIP_CORE_DECL __declspec(dllexport)
#		else
#			define LIP_CORE_DECL __declspec(dllimport)
#		endif
#	else
#		ifdef LIP_CORE_BUILDING
#			define LIP_CORE_DECL __attribute__((visibility("default")))
#		else
#			define LIP_CORE_DECL
#		endif
#	endif
#else
#	define LIP_CORE_DECL
#endif

#define LIP_CORE_API LIP_LINKAGE LIP_CORE_DECL

#if defined(__GNUC__) || defined(__clang__)
#	define LIP_MAYBE_UNUSED __attribute__((unused))
#	define LIP_PRINTF_LIKE(x, y) __attribute__((format(printf, x, y)))
#else
#	define LIP_MAYBE_UNUSED
#	define LIP_PRINTF_LIKE(x, y)
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

/// Convenient macro to help during implementation of various interfaces.
#define LIP_CONTAINER_OF(PTR, TYPE, MEMBER) \
	(TYPE*)((char*)PTR - offsetof(TYPE, MEMBER))

#define LIP_MIN(A, B) ((A) < (B) ? (A) : (B))
#define LIP_MAX(A, B) ((A) > (B) ? (A) : (B))

/**
 * @brief Declaration macro for array type.
 *
 * @see array
 */
#define lip_array(T) T*

#ifdef __cplusplus
#	define LIP_ALIGN_OF(TYPE) alignof(TYPE)
#else
#	define LIP_ALIGN_OF(TYPE) offsetof(struct { char c; TYPE t;}, t)
#endif

/** @enum lip_value_type_t Type of a ::lip_value_s.
 * @var LIP_VAL_NIL
 * A null value.
 *
 * @var LIP_VAL_NUMBER
 * A double precision floating point number.
 * When a lip_value_s has this type, access it through lip_value_s::number.
 *
 * @var LIP_VAL_BOOLEAN
 * A boolen value.
 * When a lip_value_s has this type, access it through lip_value_s::boolean.
 *
 * @var LIP_VAL_STRING
 * A string.
 * When lip_value_s has this value, access it using ::lip_as_string.
 *
 * @var LIP_VAL_SYMBOL
 * A symbol.
 * When lip_value_s has this value, access it using ::lip_as_string.
 *
 * @var LIP_VAL_LIST
 * A list.
 * When lip_value_s has this value, access it using ::lip_as_list.
 *
 * @var LIP_VAL_FUNCTION
 * A function.
 * Use ::lip_call to call a function.
 *
 * @var LIP_VAL_PLACEHOLDER
 * A placeholder value.
 * Under normal circumstances, it should be impossible to encounter this value.
 *
 * @var LIP_VAL_NATIVE
 * An opaque native pointer.
 */

#define LIP_VAL(F) \
	F(LIP_VAL_NIL) \
	F(LIP_VAL_NUMBER) \
	F(LIP_VAL_BOOLEAN) \
	F(LIP_VAL_STRING) \
	F(LIP_VAL_SYMBOL) \
	F(LIP_VAL_LIST) \
	F(LIP_VAL_FUNCTION) \
	F(LIP_VAL_PLACEHOLDER) \
	F(LIP_VAL_NATIVE)

LIP_ENUM(lip_value_type_t, LIP_VAL)

/** @enum lip_exec_status_t Execution status.
 * @var LIP_EXEC_OK
 * Successful execution.
 *
 * @var LIP_EXEC_ERROR
 * Error during execution.
 */

#define LIP_EXEC(F) \
	F(LIP_EXEC_OK) \
	F(LIP_EXEC_ERROR)

LIP_ENUM(lip_exec_status_t, LIP_EXEC)

#ifdef __cplusplus
#	define LIP_FLEXIBLE_ARRAY_MEMBER(TYPE, NAME) TYPE NAME[1]
#else
#	define LIP_FLEXIBLE_ARRAY_MEMBER(TYPE, NAME) TYPE NAME[]
#endif

typedef struct lip_fs_s lip_fs_t;
typedef struct lip_loc_s lip_loc_t;
typedef struct lip_loc_range_s lip_loc_range_t;
typedef struct lip_string_ref_s lip_string_ref_t;
typedef struct lip_string_s lip_string_t;
typedef struct lip_list_s lip_list_t;
typedef struct lip_in_s lip_in_t;
typedef struct lip_out_s lip_out_t;
typedef struct lip_allocator_s lip_allocator_t;
typedef struct lip_value_s lip_value_t;
typedef struct lip_vm_s lip_vm_t;
typedef struct lip_vm_config_s lip_vm_config_t;
typedef struct lip_vm_hook_s lip_vm_hook_t;
typedef struct lip_module_loader_s lip_module_loader_t;

/**
 * @brief Native function signature.
 *
 * @param vm The VM making the call.
 * @param result Pointer to write result (or error) to.
 *
 * @return Execution status.
 *
 * @see lip_declare_function
 * @see lip_get_args
 * @see lip_get_env
 */
typedef lip_exec_status_t(*lip_native_fn_t)(lip_vm_t* vm, lip_value_t* result);

/// Source file location.
struct lip_loc_s
{
	/// Line number (first line is 1).
	uint32_t line;
	/// Column number (first character is 1).
	uint32_t column;
};

/// A range in source file.
struct lip_loc_range_s
{
	/// Start position.
	lip_loc_t start;
	/// End position.
	lip_loc_t end;
};

/// Referenced string type.
struct lip_string_ref_s
{
	/// Length of the string.
	size_t length;
	/// Pointer to first char.
	const char* ptr;
};

/// Runtime string type.
struct lip_string_s
{
	/// Length of the string.
	size_t length;
	/// Content of the string.
	LIP_FLEXIBLE_ARRAY_MEMBER(char, ptr);
};

/**
 * @brief Lip's list type.
 *
 * Members are packed contiguously in memory, starting with lip_list_s::elements.
 * When a list is sliced, it retains a reference to its parent through lip_list_s::root.
 *
 * @see LIP_VAL_LIST
 */
struct lip_list_s
{
	/// Number of elmements.
	size_t length;
	/// Pointer to first element.
	lip_value_t* elements;
	/// Reference to parent list.
	lip_value_t* root;
};

/**
 * @brief A value in lip
 *
 * Except for `type`, its members should not be accessed directly.
 * Unless specified otherwise, use `lip_as_*` and `lip_make_*` functions to create and access this struct instead.
 */
struct lip_value_s
{
	/// Type of the value
	lip_value_type_t type;
	union
	{
		/// Should not be accessed directly.
		uint32_t index;
		/// Should not be accessed directly.
		void* reference;
		/// Valid when lip_value_s::type is ::LIP_VAL_BOOLEAN.
		bool boolean;
		/// Valid when lip_value_s::type is ::LIP_VAL_NUMBER.
		double number;
	} data;
};

/// Configuration for a ::lip_vm_s instance.
struct lip_vm_config_s
{
	/// Length of the operand stack (in number of entries).
	uint32_t os_len;
	/// Length of the call stack (in number of entries).
	uint32_t cs_len;
	/// Length of the environment stack (in number of entries).
	uint32_t env_len;
};

/**
 * @brief VM execution hook.
 *
 * @see lip_set_hook
 */
struct lip_vm_hook_s
{
	/**
	 * @brief Called before an instruction is executed.
	 *
	 * @param hook The current hook.
	 * @param vm The vm executing the hook.
	 */
	void(*step)(lip_vm_hook_t* hook, const lip_vm_t* vm);

	/**
	 * @brief Called when an error is thrown.
	 *
	 * @param hook The current hook.
	 * @param vm The vm executing the hook.
	 */
	void(*error)(lip_vm_hook_t* hook, const lip_vm_t* vm);
};

/// Constant location value for pieces of code that do not have location information.
static const lip_loc_range_t LIP_LOC_NOWHERE = {
	// For C++ compatibility, I can't do this:
	//.start = { .line = 0, .column = 0 },
	//.end = { .line = 0, .column = 0 }
	{ 0, 0 },
	{ 0, 0 }
};

/// Create a string ref from a C-string.
LIP_MAYBE_UNUSED static inline lip_string_ref_t
lip_string_ref(const char* string)
{
	lip_string_ref_t result = { strlen(string), string };
	return result;
}

/// Compare two lip_string_ref_s for equality.
LIP_MAYBE_UNUSED static inline bool
lip_string_ref_equal(lip_string_ref_t lhs, lip_string_ref_t rhs)
{
	return lhs.length == rhs.length
		&& (lhs.ptr == rhs.ptr || memcmp(lhs.ptr, rhs.ptr, lhs.length) == 0);
}

/// Compare two lip_string_s for equality.
LIP_MAYBE_UNUSED static inline bool
lip_string_equal(lip_string_t* lhs, lip_string_t* rhs)
{
	return lhs == rhs
		|| (lhs->length == rhs->length && memcmp(lhs->ptr, rhs->ptr, lhs->length) == 0);
}

/**
 * @}
 */

#endif
