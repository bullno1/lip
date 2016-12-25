#ifndef LIP_PLATFORM_H
#define LIP_PLATFORM_H

#include "common.h"

#if defined(LIP_SINGLE_THREADED)

#define LIP_THREADING_DUMMY "dummy"
#define LIP_THREADING_API LIP_THREADING_DUMMY

typedef char lip_rwlock_t;

#elif defined(__linux__)

#include <pthread.h>

#define LIP_THREADING_PTHREAD "pthread"
#define LIP_THREADING_API LIP_THREADING_PTHREAD

typedef pthread_rwlock_t lip_rwlock_t;

#elif defined(_WIN32) || defined(_WIN64)

#define LIP_THREADING_WINAPI "winapi"
#define LIP_THREADING_API LIP_THREADING_WINAPI

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef SRWLOCK lip_rwlock_t;

#else

#	error "Unsupported platform"

#endif

bool
lip_rwlock_init(lip_rwlock_t* rwlock);

void
lip_rwlock_destroy(lip_rwlock_t* rwlock);

bool
lip_rwlock_begin_read(lip_rwlock_t* rwlock);

void
lip_rwlock_end_read(lip_rwlock_t* rwlock);

bool
lip_rwlock_begin_write(lip_rwlock_t* rwlock);

void
lip_rwlock_end_write(lip_rwlock_t* rwlock);

#endif
