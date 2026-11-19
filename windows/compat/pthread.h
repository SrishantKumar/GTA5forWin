#pragma once
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef HANDLE pthread_t;

typedef struct {
    size_t stackSize;
} pthread_attr_t;

typedef CRITICAL_SECTION pthread_mutex_t;
typedef void *pthread_mutexattr_t;

#define PTHREAD_MUTEX_INITIALIZER {(void*)-1, -1, 0, 0, 0, 0}

typedef CONDITION_VARIABLE pthread_cond_t;
typedef void *pthread_condattr_t;
#define PTHREAD_COND_INITIALIZER {0}

typedef INIT_ONCE pthread_once_t;
#define PTHREAD_ONCE_INIT INIT_ONCE_STATIC_INIT

typedef int qos_class_t;
#define QOS_CLASS_USER_INTERACTIVE 1
#define QOS_CLASS_USER_INITIATED 2
#define QOS_CLASS_DEFAULT 3
#define QOS_CLASS_UTILITY 4
#define QOS_CLASS_BACKGROUND 5

static inline int pthread_attr_init(pthread_attr_t *attr) {
    if (attr) attr->stackSize = 0;
    return 0;
}

static inline int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize) {
    if (attr) attr->stackSize = stacksize;
    return 0;
}

static inline int pthread_attr_destroy(pthread_attr_t *attr) {
    (void)attr;
    return 0;
}

static inline int pthread_attr_set_qos_class_np(pthread_attr_t *attr, qos_class_t qos, int relative_priority) {
    (void)attr; (void)qos; (void)relative_priority;
    return 0;
}

static inline int pthread_set_qos_class_self_np(qos_class_t qos, int relative_priority) {
    (void)qos; (void)relative_priority;
    return 0;
}

static inline int pthread_setname_np(const char *name) {
    (void)name;
    return 0;
}

static inline int pthread_getname_np(pthread_t thread, char *name, size_t len) {
    (void)thread;
    if (name && len) name[0] = 0;
    return 0;
}

static inline pthread_t pthread_from_mach_thread_np(void *mach_thread) {
    (void)mach_thread;
    return NULL;
}

static inline int pthread_equal(pthread_t t1, pthread_t t2) {
    return t1 == t2;
}

static inline int pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *a) {
    (void)a;
    InitializeCriticalSection(m);
    return 0;
}

static inline int pthread_mutex_lock(pthread_mutex_t *m) {
    EnterCriticalSection(m);
    return 0;
}

static inline int pthread_mutex_unlock(pthread_mutex_t *m) {
    LeaveCriticalSection(m);
    return 0;
}

static inline int pthread_mutex_destroy(pthread_mutex_t *m) {
    DeleteCriticalSection(m);
    return 0;
}

static inline int pthread_cond_init(pthread_cond_t *c, const pthread_condattr_t *a) {
    (void)a;
    InitializeConditionVariable(c);
    return 0;
}

static inline int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m) {
    SleepConditionVariableCS(c, m, INFINITE);
    return 0;
}

static inline int pthread_cond_timedwait_relative_np(pthread_cond_t *c, pthread_mutex_t *m, const struct timespec *reltime) {
    DWORD ms = INFINITE;
    if (reltime) {
        ms = (DWORD)(reltime->tv_sec * 1000 + (reltime->tv_nsec / 1000000));
    }
    if (!SleepConditionVariableCS(c, m, ms)) {
        return ETIMEDOUT;
    }
    return 0;
}

static inline int pthread_cond_signal(pthread_cond_t *c) {
    WakeConditionVariable(c);
    return 0;
}

static inline int pthread_cond_broadcast(pthread_cond_t *c) {
    WakeAllConditionVariable(c);
    return 0;
}

static inline int pthread_cond_destroy(pthread_cond_t *c) {
    (void)c;
    return 0;
}

static inline BOOL CALLBACK _pthread_once_callback(PINIT_ONCE InitOnce, PVOID Parameter, PVOID *Context) {
    (void)InitOnce; (void)Context;
    void (*init_routine)(void) = (void (*)(void))Parameter;
    init_routine();
    return TRUE;
}

static inline int pthread_once(pthread_once_t *once_control, void (*init_routine)(void)) {
    InitOnceExecuteOnce(once_control, _pthread_once_callback, (void *)init_routine, NULL);
    return 0;
}

typedef struct {
    void *(*func)(void *);
    void *arg;
} pthread_trampoline_t;

static inline DWORD WINAPI pthread_win32_entry(LPVOID param) {
    pthread_trampoline_t *t = (pthread_trampoline_t *)param;
    void *(*func)(void *) = t->func;
    void *arg = t->arg;
    free(t);
    func(arg);
    return 0;
}

static inline int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg) {
    pthread_trampoline_t *t = (pthread_trampoline_t *)malloc(sizeof(pthread_trampoline_t));
    if (!t) return -1;
    t->func = start_routine;
    t->arg = arg;

    size_t stack = attr ? attr->stackSize : 0;
    HANDLE h = CreateThread(NULL, stack, pthread_win32_entry, t, 0, NULL);
    if (!h) {
        free(t);
        return -1;
    }
    if (thread) *thread = h;
    return 0;
}

static inline int pthread_join(pthread_t thread, void **retval) {
    (void)retval;
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    return 0;
}

static inline int pthread_detach(pthread_t thread) {
    CloseHandle(thread);
    return 0;
}

static inline pthread_t pthread_self(void) {
    return GetCurrentThread();
}

#ifdef __cplusplus
}
#endif
#endif
