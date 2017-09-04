#ifndef LIP_STD_COMMON_H
#define LIP_STD_COMMON_H

#include <lip/core.h>

#if LIP_DYNAMIC == 1
#	ifdef _WIN32
#		ifdef LIP_STD_BUILDING
#			define LIP_STD_DECL __declspec(dllexport)
#		else
#			define LIP_STD_DECL __declspec(dllimport)
#		endif
#	else
#		ifdef LIP_STD_BUILDING
#			define LIP_STD_DECL __attribute__((visibility("default")))
#		else
#			define LIP_STD_DECL
#		endif
#	endif
#else
#	define LIP_STD_DECL
#endif

#define LIP_STD_API LIP_LINKAGE LIP_STD_DECL

#endif
