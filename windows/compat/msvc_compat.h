#pragma once

#ifndef _ALLOW_COMPILER_AND_STL_VERSION_MISMATCH
#define _ALLOW_COMPILER_AND_STL_VERSION_MISMATCH 1
#endif

#if defined(_MSC_VER) || defined(__clang__)

#ifndef _ENABLE_EXPERIMENTAL_C11_ATOMICS
#define _ENABLE_EXPERIMENTAL_C11_ATOMICS 1
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef _CRT_NONSTDC_NO_WARNINGS
#define _CRT_NONSTDC_NO_WARNINGS
#endif

#include <windows.h>
#include <time.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <setjmp.h>
#include <direct.h>
#include <io.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
// _USE_MATH_DEFINES makes <math.h> expose M_PI, M_E, etc. on MSVC/clang-cl.
// Must be defined before the first <math.h> inclusion.
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <math.h>

#ifndef O_NONBLOCK
#define O_NONBLOCK 00004000
#endif
#ifndef O_DIRECTORY
#define O_DIRECTORY 00200000
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 00400000
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC 02000000
#endif
#ifndef O_ACCMODE
#define O_ACCMODE (_O_RDONLY | _O_WRONLY | _O_RDWR)
#endif

#ifndef F_GETFL
#define F_GETFL 3
#endif
#ifndef F_SETFL
#define F_SETFL 4
#endif

#ifndef lstat
#define lstat stat
#endif

static inline int fcntl(int fd, int cmd, ...) {
    (void)fd;
    if (cmd == F_GETFL) return _O_RDWR;
    if (cmd == F_SETFL) return 0;
    return 0;
}

#ifndef _longjmp
#define _longjmp longjmp
#endif

#ifndef __attribute__
#ifdef _MSC_VER
#ifndef __clang__
#define __attribute__(x)
#endif
#endif
#endif

#ifndef __builtin_expect
#define __builtin_expect(expr, val) (expr)
#endif

#ifndef __builtin_unreachable
#ifdef _MSC_VER
#define __builtin_unreachable() __assume(0)
#else
#define __builtin_unreachable() __builtin_unreachable()
#endif
#endif

#ifndef __builtin_available
#define __builtin_available(...) (0)
#endif

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & _S_IFMT) == _S_IFDIR)
#endif

#ifndef S_ISREG
#define S_ISREG(mode) (((mode) & _S_IFMT) == _S_IFREG)
#endif

#ifndef mkdir
#define mkdir(path, mode) _mkdir(path)
#endif

#ifndef strtok_r
#define strtok_r strtok_s
#endif

#ifndef strcasecmp
#define strcasecmp _stricmp
#endif

#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif

#ifndef bzero
#define bzero(b, len) memset((b), 0, (len))
#endif

#ifndef bcopy
#define bcopy(b1, b2, len) memmove((b2), (b1), (len))
#endif

typedef int clockid_t;

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#endif
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif
#ifndef CLOCK_UPTIME_RAW
#define CLOCK_UPTIME_RAW 2
#endif

#define _SC_PAGESIZE 30
#define _SC_PAGE_SIZE _SC_PAGESIZE
#define _SC_NPROCESSORS_ONLN 84

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PG_HAVE_STRCASESTR
#define PG_HAVE_STRCASESTR
static inline char *strcasestr(const char *haystack, const char *needle) {
    if (!haystack || !needle) return NULL;
    if (!*needle) return (char *)haystack;
    size_t needle_len = strlen(needle);
    for (; *haystack; haystack++) {
        if (_strnicmp(haystack, needle, needle_len) == 0) {
            return (char *)haystack;
        }
    }
    return NULL;
}
#endif

static inline long pg_get_gmtoff(const struct tm *t) {
    long tz = 0;
    _get_timezone(&tz);
    long dst = 0;
    _get_dstbias(&dst);
    return -(tz + (t && t->tm_isdst > 0 ? dst : 0));
}

static inline struct tm *gmtime_r(const time_t *timer, struct tm *buf) {
    if (!timer || !buf) return NULL;
    return gmtime_s(buf, timer) == 0 ? buf : NULL;
}

static inline struct tm *localtime_r(const time_t *timer, struct tm *buf) {
    if (!timer || !buf) return NULL;
    return localtime_s(buf, timer) == 0 ? buf : NULL;
}

static inline int getpagesize(void) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (int)si.dwPageSize;
}

static inline long sysconf(int name) {
    if (name == _SC_PAGESIZE) {
        return getpagesize();
    } else if (name == _SC_NPROCESSORS_ONLN) {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        return (long)si.dwNumberOfProcessors;
    }
    return -1;
}

static inline int clock_gettime(int clock_id, struct timespec *tp) {
    if (!tp) return -1;
    if (clock_id == CLOCK_REALTIME) {
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        ULARGE_INTEGER uli;
        uli.LowPart = ft.dwLowDateTime;
        uli.HighPart = ft.dwHighDateTime;
        uint64_t t = (uli.QuadPart - 116444736000000000ULL);
        tp->tv_sec = (time_t)(t / 10000000ULL);
        tp->tv_nsec = (long)((t % 10000000ULL) * 100ULL);
        return 0;
    } else {
        static LARGE_INTEGER freq;
        if (freq.QuadPart == 0) {
            QueryPerformanceFrequency(&freq);
        }
        LARGE_INTEGER count;
        QueryPerformanceCounter(&count);
        tp->tv_sec = (time_t)(count.QuadPart / freq.QuadPart);
        tp->tv_nsec = (long)(((count.QuadPart % freq.QuadPart) * 1000000000ULL) / freq.QuadPart);
        return 0;
    }
}

static inline uint64_t clock_gettime_nsec_np(int clock_id) {
    (void)clock_id;
    static LARGE_INTEGER freq;
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
    }
    LARGE_INTEGER count;
    QueryPerformanceCounter(&count);
    return (uint64_t)(((double)count.QuadPart / (double)freq.QuadPart) * 1e9);
}

static inline int nanosleep(const struct timespec *req, struct timespec *rem) {
    (void)rem;
    if (!req) return -1;
    DWORD ms = (DWORD)(req->tv_sec * 1000 + (req->tv_nsec / 1000000));
    if (ms == 0 && req->tv_nsec > 0) ms = 1;
    Sleep(ms);
    return 0;
}

static inline int vasprintf(char **strp, const char *fmt, va_list ap) {
    if (!strp || !fmt) return -1;
    va_list ap_copy;
    va_copy(ap_copy, ap);
    int len = vsnprintf(NULL, 0, fmt, ap_copy);
    va_end(ap_copy);
    if (len < 0) return -1;
    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf) return -1;
    int written = vsnprintf(buf, (size_t)len + 1, fmt, ap);
    if (written < 0) { free(buf); return -1; }
    *strp = buf;
    return written;
}

static inline int asprintf(char **strp, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = vasprintf(strp, fmt, ap);
    va_end(ap);
    return ret;
}

static inline char *strndup(const char *s, size_t n) {
    if (!s) return NULL;
    size_t len = 0;
    while (len < n && s[len]) len++;
    char *ret = (char *)malloc(len + 1);
    if (!ret) return NULL;
    memcpy(ret, s, len);
    ret[len] = '\0';
    return ret;
}

static inline void *memmem(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen) {
    if (!haystack || !needle || needlelen == 0 || haystacklen < needlelen) return NULL;
    const unsigned char *h = (const unsigned char *)haystack;
    const unsigned char *n = (const unsigned char *)needle;
    for (size_t i = 0; i <= haystacklen - needlelen; i++) {
        if (h[i] == n[0] && memcmp(h + i, n, needlelen) == 0) {
            return (void *)(h + i);
        }
    }
    return NULL;
}

#ifndef SSIZE_MAX
#define SSIZE_MAX ((ssize_t)(SIZE_MAX >> 1))
#endif

typedef intptr_t ssize_t;

static inline ssize_t pread(int fd, void *buf, size_t count, int64_t offset) {
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE) return -1;
    OVERLAPPED ov = {0};
    ov.Offset = (DWORD)(offset & 0xFFFFFFFF);
    ov.OffsetHigh = (DWORD)((offset >> 32) & 0xFFFFFFFF);
    DWORD read_bytes = 0;
    if (!ReadFile(h, buf, (DWORD)count, &read_bytes, &ov)) {
        if (GetLastError() == ERROR_HANDLE_EOF) return 0;
        return -1;
    }
    return (ssize_t)read_bytes;
}

static inline ssize_t pwrite(int fd, const void *buf, size_t count, int64_t offset) {
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE) return -1;
    OVERLAPPED ov = {0};
    ov.Offset = (DWORD)(offset & 0xFFFFFFFF);
    ov.OffsetHigh = (DWORD)((offset >> 32) & 0xFFFFFFFF);
    DWORD written_bytes = 0;
    if (!WriteFile(h, buf, (DWORD)count, &written_bytes, &ov)) return -1;
    return (ssize_t)written_bytes;
}

#ifdef __cplusplus
}
#endif

#endif // _MSC_VER || __clang__
