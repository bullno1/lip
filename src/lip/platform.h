#ifndef LIP_PLATFORM_H
#define LIP_PLATFORM_H

#if defined(__GNUC__) || defined(__clang__)
#	define _XOPEN_SOURCE 700
#endif

#include <lip/common.h>
#include <lip/config.h>

#if defined(LIP_THREADING_DUMMY)

typedef char lip_rwlock_t;

#elif defined(LIP_THREADING_PTHREAD)

#include <pthread.h>

typedef pthread_rwlock_t lip_rwlock_t;

#elif defined(LIP_THREADING_WINAPI)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef SRWLOCK lip_rwlock_t;

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
