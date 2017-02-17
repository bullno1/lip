#ifndef LIP_IO_H
#define LIP_IO_H

/**
 * @defgroup io IO
 * @brief IO functions and structures
 *
 * @{
 */

#include <stdarg.h>
#include <stdio.h>
#include "common.h"

/**
 * @brief Filesystem interface.
 *
 * Callbacks might be invoked from different threads at the same time.
 */
struct lip_fs_s
{
	/**
	 * @brief Callback to open a file for reading.
	 *
	 * Should have similar behaviour to `fopen(path, "rb")`.
	 *
	 * @param self Filesystem.
	 * @param path Path to file.
	 * @return an input stream or `NULL` if file cannot be openned.
	 *
	 * @see lip_fs_s::last_error
	 */
	lip_in_t*(*begin_read)(lip_fs_t* self, lip_string_ref_t path);

	/**
	 * @brief Callback to close a file previously openned for reading.
	 *
	 * Should have similar behaviour to `fclose(file)`.
	 */
	void(*end_read)(lip_fs_t* self, lip_in_t* input);

	/**
	 * @brief Callback to get the last error of the filesystem.
	 *
	 * Should have similar behaviour to `strerror(errno)`.
	 */
	lip_string_ref_t(*last_error)(lip_fs_t* self);
};

/// Input stream interface.
struct lip_in_s
{
	/// This callback should behave like fread.
	size_t (*read)(void* buff, size_t size, lip_in_t* input);
};

/// Output stream interface.
struct lip_out_s
{
	/// This callback should behave like fwrite.
	size_t (*write)(const void* buff, size_t size, lip_out_t* output);
};

/**
 * @brief Input string stream.
 * @see lip_make_isstream
 */
struct lip_isstream_s
{
	lip_in_t vtable;
	lip_string_ref_t str;
	size_t pos;
};

/**
 * @brief Output string stream.
 * @see lip_make_osstream
 */
struct lip_osstream_s
{
	lip_out_t vtable;
	lip_array(char)* buffer;
};

/**
 * @brief Output file stream.
 * @see lip_make_ofstream
 */
struct lip_ofstream_s
{
	lip_out_t vtable;
	FILE* file;
};

/**
 * @brief Input file stream.
 * @see lip_make_ifstream
 */
struct lip_ifstream_s
{
	lip_in_t vtable;
	FILE* file;
};

/// Returns stdin.
LIP_API lip_in_t*
lip_stdin(void);

/// Returns stdout.
LIP_API lip_out_t*
lip_stdout(void);

/// Returns stderr.
LIP_API lip_out_t*
lip_stderr(void);

/**
 * @brief Create an instance of a native filesystem (fopen, fread, fclose...).
 * @param allocator Allocator that the filesystem will use.
 */
LIP_API lip_fs_t*
lip_create_native_fs(lip_allocator_t* allocator);

/// Destroy the filesystem implementation previously created with ::lip_create_native_fs.
LIP_API void
lip_destroy_native_fs(lip_fs_t* fs);

/// Read from a ::lip_in_s, similar behaviour to fread.
LIP_MAYBE_UNUSED static inline size_t
lip_read(void* buff, size_t size, lip_in_t* input)
{
	return input->read(buff, size, input);
}

/// Write to a ::lip_in_s, similar behaviour to fwrite.
LIP_MAYBE_UNUSED static inline size_t
lip_write(const void* buff, size_t size, lip_out_t* output)
{
	return output->write(buff, size, output);
}

/// Print formatted text to a ::lip_out_s, similar behaviour to fprintf.
LIP_API LIP_PRINTF_LIKE(2, 3) size_t
lip_printf(lip_out_t* output, const char* format, ...);

/// Print formatted text to a ::lip_out_s, similar behaviour to fvprintf.
LIP_API size_t
lip_vprintf(lip_out_t* output, const char* format, va_list args);

/// Create an input string stream.
LIP_API lip_in_t*
lip_make_isstream(lip_string_ref_t str, struct lip_isstream_s* sstream);

/**
 * @brief Create an output string stream.
 *
 * @param buffer An array to hold the output.
 * @param sstream An instance of ::lip_osstream_s (usually stack-allocated).
 *
 * @see array
 */
LIP_API lip_out_t*
lip_make_osstream(lip_array(char)* buffer, struct lip_osstream_s* sstream);

/// Create an output file stream.
LIP_API lip_out_t*
lip_make_ofstream(FILE* file, struct lip_ofstream_s* ofstream);

/// Create an input file stream.
LIP_API lip_in_t*
lip_make_ifstream(FILE* file, struct lip_ifstream_s* ifstream);

/**
 * @}
 */

#endif
