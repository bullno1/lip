#ifndef LIP_STD_IO_H
#define LIP_STD_IO_H

#include "common.h"
#include <lip/core/io.h>
#include <stdio.h>

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

/**
 * @brief Create an instance of a native filesystem (fopen, fread, fclose...).
 * @param allocator Allocator that the filesystem will use.
 */
LIP_STD_API lip_fs_t*
lip_create_std_fs(lip_allocator_t* allocator);

/// Destroy the filesystem implementation previously created with ::lip_create_std_fs.
LIP_STD_API void
lip_destroy_std_fs(lip_fs_t* fs);

/// Returns stdin.
LIP_STD_API lip_in_t*
lip_stdin(void);

/// Returns stdout.
LIP_STD_API lip_out_t*
lip_stdout(void);

/// Returns stderr.
LIP_STD_API lip_out_t*
lip_stderr(void);

/// Create an output file stream.
LIP_STD_API lip_out_t*
lip_make_ofstream(FILE* file, struct lip_ofstream_s* ofstream);

/// Create an input file stream.
LIP_STD_API lip_in_t*
lip_make_ifstream(FILE* file, struct lip_ifstream_s* ifstream);

#endif
