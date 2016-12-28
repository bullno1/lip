#ifndef LIP_CONFIG_H
#define LIP_CONFIG_H

#ifdef LIP_HAVE_CONFIG_H
#	include "gen/config.h"
#else
#	define LIP_VERSION "0.1"
#	define LIP_COMPILE_FLAGS ""
#	define LIP_LINK_FLAGS ""
#	define LIP_COMPILER ""
#	define LIP_LINKER ""
#endif

#if defined(LIP_SINGLE_THREADED)
#	define LIP_THREADING_DUMMY "dummy"
#	define LIP_THREADING_API LIP_THREADING_DUMMY
#elif defined(__linux__)
#	define LIP_THREADING_PTHREAD "pthread"
#	define LIP_THREADING_API LIP_THREADING_PTHREAD
#elif defined(_WIN32) || defined(_WIN64)
#	define LIP_THREADING_WINAPI "winapi"
#	define LIP_THREADING_API LIP_THREADING_WINAPI
#else
#	error "Unspported platform. Please recompile with LIP_SINGLE_THREADED"
#endif

#endif
