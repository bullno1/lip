#ifndef LIP_LIP_H
#define LIP_LIP_H

/**
 * @defgroup runtime Runtime
 * @brief Runtime functions
 *
 * @{
 */

#include "common.h"
#include <stdarg.h>

/** @enum lip_userdata_scope_t
 * @brief Userdata scope.
 *
 * @see lip_set_userdata
 * @see lip_get_userdata
 *
 * @var LIP_SCOPE_RUNTIME
 * Userdata is attached to runtime.
 *
 * @var LIP_SCOPE_CONTEXT
 * Userdata is attached to context.
 *
 * @var LIP_SCOPE_VM
 * Userdata is attached to vm.
 */

#define LIP_USERDATA_SCOPE(F) \
	F(LIP_SCOPE_RUNTIME) \
	F(LIP_SCOPE_CONTEXT) \
	F(LIP_SCOPE_VM)

LIP_ENUM(lip_userdata_scope_t, LIP_USERDATA_SCOPE)

typedef struct lip_runtime_config_s lip_runtime_config_t;

/**
 * @brief A lip runtime.
 *
 * An application typically needs only one runtime.
 *
 * @see lip_create_runtime
 */
typedef struct lip_runtime_s lip_runtime_t;

/**
 * @brief A lip context.
 *
 * An application typically needs one context per thread.
 *
 * @see lip_create_context
 */
typedef struct lip_context_s lip_context_t;

/**
 * @brief Handle to a lip script.
 *
 * @see lip_load_script.
 */
typedef struct lip_script_s lip_script_t;
typedef struct lip_repl_handler_s lip_repl_handler_t;
typedef struct lip_context_error_s lip_context_error_t;
typedef struct lip_error_record_s lip_error_record_t;

/**
 * @brief Handle to a namespace context.
 *
 * @see lip_begin_ns
 */
typedef struct lip_ns_context_s lip_ns_context_t;

/**
 * @brief Panic handler.
 *
 * A lip context will only invoke this when it encounters an unrecoverable error.
 * When this happens, the corresponding lip runtime can be left in an inconsistent state.
 * One of the few proper course of actions is to log/display the error message and abort the program.
 *
 * The default panic handler prints the error message to stderr and then calls `abort()`.
 *
 * @param ctx The context invoking the panic handler.
 * @param msg Error message.
 *
 * @see lip_set_panic_handler
 */
typedef void(*lip_panic_fn_t)(lip_context_t* ctx, const char* msg);

/**
 * @brief Runtime configuration
 * @see lip_reset_runtime_config
 */
struct lip_runtime_config_s
{
	/**
	 * @brief An allocator that this runtime will use.
	 *
	 * @see allocator
	 */
	lip_allocator_t* allocator;

	/**
	 * @brief A filesystem interface.
	 *
	 * This can be `NULL` and the runtime will use the native filesystem.
	 *
	 * @see lip_create_native_fs
	 */
	lip_fs_t* fs;

	/**
	 * @brief Number of module search patterns.
	 *
	 * @see lip_runtime_config_s::module_search_patterns;
	 */
	unsigned int num_module_search_patterns;

	/**
	 * @brief Array of module search patterns.
	 *
	 * @see ::lip_load_module
	 */
	const lip_string_ref_t* module_search_patterns;

	/// Default configuration for virtual machines in this runtime.
	lip_vm_config_t default_vm_config;
};

/**
 * @brief Read-Eval-Print Loop (REPL) handler interface.
 *
 * @see lip_repl
 */
struct lip_repl_handler_s
{
	/**
	 * @brief Callback to read user input.
	 *
	 * This callback should behave like fread:
	 *
	 * - `buff` is guaranteed to be of at least `size` bytes.
	 * - At most `size` bytes can be read on each call.
	 * - Return 0 to signal end of input.
	 *
	 * @param self The repl handler.
	 * @param buff The buffer to write result.
	 * @param size Number of bytes to read.
	 *
	 * @return Number of bytes read or 0 for error/end of input.
	 */
	size_t(*read)(lip_repl_handler_t* self, void* buff, size_t size);

	/**
	 * @brief Callback to print evaluation result.
	 *
	 * When `status` is LIP_EXEC_OK, `result` will hold the result of the last expression.
	 *
	 * When `status` is LIP_EXEC_ERROR, `result` will hold the error of the last expression.
	 * Use ::lip_get_error to get the full stacktrace.
	 *
	 * @see lip_print_value
	 * @see print
	 *
	 * @param self The repl handler.
	 * @param status Execution status of the last expression.
	 * @param result Result or error of the last expression.
	 */
	void(*print)(lip_repl_handler_t* self, lip_exec_status_t status, lip_value_t result);
};

/**
 * @brief Error record.
 *
 * @see lip_context_error_s.
 */
struct lip_error_record_s
{
	/// Filename where the error happened.
	lip_string_ref_t filename;
	/// Location in the file where the error happened.
	lip_loc_range_t location;
	/// Error message.
	lip_string_ref_t message;
};

/**
 * @brief Last error in a context.
 *
 * @see lip_get_error.
 */
struct lip_context_error_s
{
	/// Error message.
	lip_string_ref_t message;
	/// Number of error records.
	unsigned int num_records;
	/// Error record.
	lip_error_record_t* records;
	/// The error that leads to this error. Can be `NULL`.
	const lip_context_error_t* parent;
};

/**
 * @brief Reset a lip_runtime_config_s to its default value.
 *
 * That is:
 *
 * - Use ::lip_default_allocator for lip_runtime_config_s::allocator.
 * - Initialize lip_runtime_config_s::default_vm_config with the following values:
 *   - lip_vm_config_t::os_len: 256
 *   - lip_vm_config_t::cs_len: 256
 *   - lip_vm_config_t::env_len: 256
 * - Initialize lip_runtime_config_s::module_search_patterns with the following:
 *   `?.lip`, `?.lipc`, `?/index.lip`, `?/index.lipc`, `!.lip`, `!.lipc`,
 *   '!/index.lip`, `!/index.lipc`
 */
LIP_API void
lip_reset_runtime_config(lip_runtime_config_t* cfg);

/**
 * @brief Create a runtime instance.
 *
 * @param config Configuration for this runtime.
 * @return A runtime instance.
 */
LIP_API lip_runtime_t*
lip_create_runtime(const lip_runtime_config_t* config);

/**
 * @brief Destroy a runtime.
 *
 * All associated ::lip_context_t instances will be destroyed.
 *
 * @param runtime Runtime instance to destroy.
 */
LIP_API void
lip_destroy_runtime(lip_runtime_t* runtime);

/**
 * @brief Create a new context.
 *
 * @param runtime The associated runtime.
 * @param allocator The allocator for this context.
 *    Pass `NULL` to use the runtime's allocator.
 *    The allocator will have to be thread-safe if it is shared between contexts in different threads.
 *
 * @remarks A context can be destroyed by ::lip_destroy_context or ::lip_destroy_runtime on the corresponding runtime.
 *
 * @return A context instance.
 */
LIP_API lip_context_t*
lip_create_context(lip_runtime_t* runtime, lip_allocator_t* allocator);

/**
 * @brief Destroy a context.
 *
 * All associated ::lip_vm_s, ::lip_script_t instances will be destroyed.
 *
 * @param ctx The context to destroy.
 */
LIP_API void
lip_destroy_context(lip_context_t* ctx);

/**
 * @brief Set a panic handler for this context.
 *
 * @param ctx The context.
 * @param panic_handler The panic handler to set.
 *
 * @return The old panic handler.
 *
 * @see lip_panic_fn_t
 */
LIP_API lip_panic_fn_t
lip_set_panic_handler(lip_context_t* ctx, lip_panic_fn_t panic_handler);

/**
 * @brief Get the last error in this context.
 *
 * @see lip_print_error
 */
LIP_API const lip_context_error_t*
lip_get_error(lip_context_t* ctx);

/**
 * @brief Expand the last execution error in a vm to a full trace.
 *
 * @see lip_call
 * @see lip_print_error
 */
LIP_API const lip_context_error_t*
lip_traceback(lip_context_t* ctx, lip_vm_t* vm, lip_value_t msg);

/**
 * @brief Print the last error in a nicely formatted output.
 */
LIP_API void
lip_print_error(lip_out_t* out, lip_context_t* ctx);

/**
 * @brief Create a namespace context to start defining symbols.
 *
 * This function can be likened to the `BEGIN` statement in a SQL.
 *
 * @see lip_end_ns
 * @see lip_discard_ns
 * @see lip_declare_function
 */
LIP_API lip_ns_context_t*
lip_begin_ns(lip_context_t* ctx, lip_string_ref_t name);

/**
 * @brief Finish namespace definition.
 *
 * After this function returns, all defined symbols will be visible to all virtual machines in the same runtime.
 * This function can be likened to the `COMMIT` statement in SQL.
 *
 * @remarks The namespace context will become invalid and must not be accessed again.
 *
 * @see lip_begin_ns
 * @see lip_discard_ns
 * @see lip_declare_function
 */
LIP_API void
lip_end_ns(lip_context_t* ctx, lip_ns_context_t* ns);

/**
 * @brief Discard a namespace definition context.
 *
 * Discard all symbols defined in the current namespace context.
 * This function can be likened to the `ROLLBACK` statement in SQL.
 *
 * @remarks The namespace context will become invalid and must not be accessed again.
 *
 * @see lip_begin_ns
 * @see lip_end_ns
 * @see lip_declare_function
 */
LIP_API void
lip_discard_ns(lip_context_t* ctx, lip_ns_context_t* ns);


/**
 * @brief Define a native function in the given namespace.
 *
 * @remarks After a series of calls to ::lip_declare_function, one must call ::lip_end_ns for the definitions to be visible.
 *
 * @see lip_begin_ns
 * @see lip_end_ns
 */
LIP_API void
lip_declare_function(
	lip_ns_context_t* ns, lip_string_ref_t name, lip_native_fn_t fn
);

/**
 * @brief Create a virtual machine instance.
 *
 * @param ctx The context that this vm will belong to.
 * @param config Configuration for the vm. Pass `NULL` to use default values.
 *
 * @remarks A vm can be destroyed by ::lip_destroy_vm or ::lip_destroy_context on the corresponding context.
 *
 * @see lip_runtime_config_s
 */
LIP_API lip_vm_t*
lip_create_vm(lip_context_t* ctx, const lip_vm_config_t* config);

/// Destroy a vm previously created with ::lip_destroy_vm.
LIP_API void
lip_destroy_vm(lip_context_t* ctx, lip_vm_t* vm);

/**
 * @brief Lookup a symbol.
 *
 * @param ctx A context.
 * @param symbol Value of the symbol.
 * @param result Result.
 *
 * @return Whether the symbol was found.
 */
LIP_API bool
lip_lookup_symbol(lip_context_t* ctx, lip_string_ref_t symbol, lip_value_t* result);

/**
 * @brief Load a script.
 *
 * @param ctx A context.
 * @param filename Filename.
 * @param input Input stream or `NULL` to read from the filesystem.
 * @param link Should the script be linked.
 *   A linked script means all dependent modules will be loaded and symbols will be resolved.
 *   Normally, this parameter should be true, unless the script is only loaded for introspection
 *   purposes or to be dumped using ::lip_dump_script.
 *
 * @return The script instance or `NULL` if an error happened.
 *
 * @remarks A script can be unloaded using ::lip_unload_script or ::lip_destroy_context on the corresponding context.
 *
 * @see lip_get_error
 * @see lip_runtime_config_s::fs
 */
LIP_API lip_script_t*
lip_load_script(
	lip_context_t* ctx,
	lip_string_ref_t filename,
	lip_in_t* input,
	bool link
);

/**
 * @brief Dump a script's compiled bytecode.
 *
 * The dumped bytecode can be loaded using ::lip_load_script.
 *
 * @param ctx The context used to load the script.
 * @param script The script to dump.
 * @param filename Output filename.
 * @param output Output stream or `NULL` to write to the filesystem.
 *
 * @return Whether the script was dumped successfully.
 *
 * @remarks Error can be retrieved using ::lip_get_error
 * @see lip_get_error
 * @see lip_runtime_config_s::fs
 */
LIP_API bool
lip_dump_script(
	lip_context_t* ctx,
	lip_script_t* script,
	lip_string_ref_t filename,
	lip_out_t* output
);

/// Unload a script previously loaded with ::lip_load_script.
LIP_API void
lip_unload_script(lip_context_t* ctx, lip_script_t* script);

/**
 * @brief Execute a script.
 *
 * @param vm The vm to execute the script on.
 * @param script The script to execute.
 * @param result Result.
 *
 * @return Execution status.
 *
 * @remarks In case of error, use ::lip_get_error to get the full error message.
 * The vm is now in an inconsistent state, use ::lip_reset_vm before using it again.
 */
LIP_API lip_exec_status_t
lip_exec_script(lip_vm_t* vm, lip_script_t* script, lip_value_t* result);

/**
 * @brief Load a module and all its dependencies.
 *
 * When lip needs to load a module, it consults the array of module search
 * paterns (lip_runtime_config_s::module_search_patterns). Each element is a
 * string which will be expanded into a path.
 *
 * A module search pattern is a regular path with one of these special
 * characters:
 *
 * - `?` will be expanded in a manner similar to a "Java classpath".
 *   For example: while trying to load module `foo.bar` the pattern `deps/?/index.lip`
 *   will expand to `deps/foo/bar/index.lip`. That is, each `.` character
 *   in the module name will be replaced with a path separator character.
 * - `!` will expand to the module name itself. For example: while searching for
 *   `foo.bar` the pattern `bin/!.lipc` will expand to `bin/foo.bar.lipc`.
 *
 * The first file that exists will be executed. It must contains a series of
 * `declare` calls at the top level. Before the file is executed, lip will
 * ensure that all modules that it references (and transitively, all modules
 * that the dependencies reference) will be successfully loaded. After execution,
 * all external references will be resolved and any undefined external reference
 * will reult in an error.
 *
 * If any error occurs during module loading, the process will be aborted and no
 * change will be made to the runtime system.
 *
 * @param ctx The context to initiate module loading.
 * @param name Name of the module.
 * @return Whether the module was successfully loaded.
 *
 * @remarks ::lip_get_error can be used to retrieve the error if this function
 * returns `false`.
 */
LIP_API bool
lip_load_module(lip_context_t* ctx, lip_string_ref_t name);

/**
 * @brief Start a Read-Eval-Print loop.
 *
 * @param vm The vm to run the repl.
 * @param source_name Name of the input stream.
 * @param repl_handler The repl handler.
 */
LIP_API void
lip_repl(
	lip_vm_t* vm, lip_string_ref_t source_name, lip_repl_handler_t* repl_handler
);

/**
 * @brief Retrieve the default VM of a context.
 *
 * The default VM is used whenever a context needs to implicitly execute code
 * e.g: macro expansion, module loading... It can be retrieved for debugging
 * or getting/setting userdata without creating a VM.
 *
 * @param ctx A context.
 * @return The default VM of the given context.
 */
LIP_API lip_vm_t*
lip_get_default_vm(lip_context_t* ctx);

/**
 * @brief Get userdata.
 *
 * @param vm A vm.
 * @param scope Where to get userdata.
 * @param key An unique pointer for the userdata.
 *
 * @return The value previously stored with ::lip_set_userdata using the same `key` and `scope` or `NULL`.
 */
LIP_API void*
lip_get_userdata(lip_vm_t* vm, lip_userdata_scope_t scope, const void* key);

/**
 * @brief Set userdata.
 *
 * @param vm A vm.
 * @param scope Where to attach userdata.
 * @param key An unique pointer for the userdata.
 * @param value Pointer to userdata.
 *
 * @return The value previously stored with ::lip_set_userdata using the same `key` and `scope` or `NULL`.
 */
LIP_API void*
lip_set_userdata(
	lip_vm_t* vm, lip_userdata_scope_t scope, const void* key, void* value
);

/// Load builtin functions.
LIP_API void
lip_load_builtins(lip_context_t* ctx);

/// Reset the VM's state.
LIP_API void
lip_reset_vm(lip_vm_t* vm);

/**
 * @brief Set a hook on this VM.
 *
 * @param vm The vm.
 * @param hook The hook to set or `NULL` to clear.
 *
 * @return The previous hook or `NULL` if none was set.
 */
LIP_API lip_vm_hook_t*
lip_set_vm_hook(lip_vm_t* vm, lip_vm_hook_t* hook);

/**
 * @brief Call a lip function from native code.
 *
 * @param vm The vm to make the call.
 * @param result Result.
 * @param fn The function to be called.
 * @param num_args Number of arguments.
 * @param ... `num_args` number of ::lip_value_s as arguments.
 *
 * @return Execution status.
 *
 * @see lip_lookup_symbol
 *
 * @remarks In case of error, use ::lip_traceback to get the full stacktrace.
 * The vm is now in an inconsistent state, use ::lip_reset_vm before using it again.
 */
LIP_API lip_exec_status_t
lip_call(
	lip_vm_t* vm,
	lip_value_t* result,
	lip_value_t fn,
	unsigned int num_args,
	...
);

/**
 * @brief Register a native source location as the current position in the current native stackframe.
 *
 * This function should not be called directly.
 * #lip_call and #lip_exec_script macros calls ::lip_set_native_location before passing control to the corresponding functions.
 */
LIP_API void
lip_set_native_location(
	lip_vm_t* vm, const char* function, const char* file, int line
);

/**
 * @brief Get arguments.
 *
 * This can only be called inside a native binding function.
 *
 * @param vm The vm passed to a native function.
 * @param num_args Number of arguments.
 * @return Array of arguments.
 *
 * @see lip_native_fn_t
 */
LIP_API const lip_value_t*
lip_get_args(const lip_vm_t* vm, uint8_t* num_args);

/**
 * @brief Get bound variables.
 *
 * This can only be called inside a native binding function.
 *
 * @param vm The vm passed to a native function.
 * @param env_len Number of bound variables.
 * @return Array of bound variables.
 *
 * @see lip_native_fn_t
 * @see lip_make_function
 */
LIP_API const lip_value_t*
lip_get_env(const lip_vm_t* vm, uint8_t* env_len);

/// Create a boolean value.
LIP_MAYBE_UNUSED static inline lip_value_t
lip_make_boolean(lip_vm_t* vm, bool boolean)
{
	(void)vm;
	lip_value_t val;
	val.type = LIP_VAL_BOOLEAN;
	val.data.boolean = boolean;
	return val;
}

/// Create a nil value.
LIP_MAYBE_UNUSED static inline lip_value_t
lip_make_nil(lip_vm_t* vm)
{
	(void)vm;
	lip_value_t val;
	val.type = LIP_VAL_NIL;
	val.data.reference = NULL;
	return val;
}

/// Create a number value.
LIP_MAYBE_UNUSED static inline lip_value_t
lip_make_number(lip_vm_t* vm, double number)
{
	(void)vm;
	(void)vm;
	lip_value_t val;
	val.type = LIP_VAL_NUMBER;
	val.data.number = number;
	return val;
}

/// Create a string value by copying.
LIP_API lip_value_t
lip_make_string_copy(lip_vm_t* vm, lip_string_ref_t str);

/// Create a formatted string, similar to sprintf.
LIP_API LIP_PRINTF_LIKE(2, 3) lip_value_t
lip_make_string(lip_vm_t* vm, const char* fmt, ...);

/// Create a formatted string, similar to vsprintf.
LIP_API lip_value_t
lip_make_stringv(lip_vm_t* vm, const char* fmt, va_list args);

/**
 * @brief Create an anonymous native function.
 *
 * @param vm The VM this function is bound to.
 * @param native_fn Pointer to native function.
 * @param env_len Number of bound variables.
 * @param env Array of `env_len` bound variables.
 * @return The function.
 */
LIP_API lip_value_t
lip_make_function(
	lip_vm_t* vm,
	lip_native_fn_t native_fn,
	uint8_t env_len,
	lip_value_t env[]
);

/// Convert a lip_value_s to a string
LIP_MAYBE_UNUSED static inline lip_string_t*
lip_as_string(lip_value_t val)
{
	return val.type == LIP_VAL_STRING || val.type == LIP_VAL_SYMBOL
		? (lip_string_t*)val.data.reference : NULL;
}

/// Convert a lip_value_s to a list
LIP_MAYBE_UNUSED static inline const lip_list_t*
lip_as_list(lip_value_t val)
{
	return val.type == LIP_VAL_LIST ? (lip_list_t*)val.data.reference : NULL;
}

#ifndef LIP_NO_MAGIC

/**
 * @brief Macro wrapper for the function ::lip_exec_script.
 *
 * This macro will register the current C/C++ source position before calling ::lip_exec_script.
 * Disable the creation of this macro by defining `LIP_NO_MAGIC` before including lip.h.
 */
#define lip_exec_script(vm, ...) \
	(lip_set_native_location(vm, __func__, __FILE__, __LINE__), lip_exec_script(vm, __VA_ARGS__))

/**
 * @brief Macro wrapper for the function ::lip_call.
 *
 * This macro will register the current C/C++ source position before calling ::lip_call.
 * Disable the creation of this macro by defining `LIP_NO_MAGIC` before including lip.h.
 */
#define lip_call(vm, ...) \
	(lip_set_native_location(vm, __func__, __FILE__, __LINE__), lip_call(vm, __VA_ARGS__))

#endif

/**
 * @}
 */

#endif
