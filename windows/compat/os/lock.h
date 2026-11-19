#pragma once
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <stdbool.h>

typedef SRWLOCK os_unfair_lock;
#define OS_UNFAIR_LOCK_INIT SRWLOCK_INIT

static inline void os_unfair_lock_lock(os_unfair_lock *lock) {
    AcquireSRWLockExclusive(lock);
}

static inline void os_unfair_lock_unlock(os_unfair_lock *lock) {
    ReleaseSRWLockExclusive(lock);
}

static inline bool os_unfair_lock_trylock(os_unfair_lock *lock) {
    return TryAcquireSRWLockExclusive(lock) != 0;
}
#endif
