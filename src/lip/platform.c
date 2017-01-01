#include "platform.h"

#if defined(LIP_THREADING_DUMMY)

bool
lip_rwlock_init(lip_rwlock_t* rwlock)
{
	(void)rwlock;
	return true;
}

void
lip_rwlock_destroy(lip_rwlock_t* rwlock)
{
	(void)rwlock;
}

bool
lip_rwlock_begin_read(lip_rwlock_t* rwlock)
{
	(void)rwlock;
	return true;
}

void
lip_rwlock_end_read(lip_rwlock_t* rwlock)
{
	(void)rwlock;
}

bool
lip_rwlock_begin_write(lip_rwlock_t* rwlock)
{
	(void)rwlock;
	return true;
}

void
lip_rwlock_end_write(lip_rwlock_t* rwlock)
{
	(void)rwlock;
}

#elif defined(LIP_THREADING_PTHREAD)

bool
lip_rwlock_init(lip_rwlock_t* rwlock)
{
	return pthread_rwlock_init(rwlock, NULL) == 0;
}

void
lip_rwlock_destroy(lip_rwlock_t* rwlock)
{
	pthread_rwlock_destroy(rwlock);
}

bool
lip_rwlock_begin_read(lip_rwlock_t* rwlock)
{
	return pthread_rwlock_rdlock(rwlock) == 0;
}

void
lip_rwlock_end_read(lip_rwlock_t* rwlock)
{
	pthread_rwlock_unlock(rwlock);
}

bool
lip_rwlock_begin_write(lip_rwlock_t* rwlock)
{
	return pthread_rwlock_wrlock(rwlock) == 0;
}

void
lip_rwlock_end_write(lip_rwlock_t* rwlock)
{
	pthread_rwlock_unlock(rwlock);
}

#elif defined(LIP_THREADING_WINAPI)

bool
lip_rwlock_init(lip_rwlock_t* rwlock)
{
	InitializeSRWLock(rwlock);
	return true;
}

void
lip_rwlock_destroy(lip_rwlock_t* rwlock)
{
	(void)rwlock;
}

bool
lip_rwlock_begin_read(lip_rwlock_t* rwlock)
{
	AcquireSRWLockShared(rwlock);
	return true;
}

void
lip_rwlock_end_read(lip_rwlock_t* rwlock)
{
	ReleaseSRWLockShared(rwlock);
}

bool
lip_rwlock_begin_write(lip_rwlock_t* rwlock)
{
	AcquireSRWLockExclusive(rwlock);
	return true;
}

void
lip_rwlock_end_write(lip_rwlock_t* rwlock)
{
	ReleaseSRWLockExclusive(rwlock);
}

#endif
