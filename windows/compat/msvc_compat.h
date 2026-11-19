#pragma once

#ifdef _MSC_VER

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
#include <direct.h>
#include <io.h>
#include <stdlib.h>
#include <string.h>

#ifndef __attribute__
#define __attribute__(x)
#endif

#ifndef __builtin_expect
#define __builtin_expect(expr, val) (expr)
#endif

#ifndef __builtin_unreachable
#define __builtin_unreachable() __assume(0)
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

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#endif
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif
#ifndef CLOCK_UPTIME_RAW
#define CLOCK_UPTIME_RAW 2
#endif

#ifdef __cplusplus
extern "C" {
#endif

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
        static LARGE_INTEGER freq = {0};
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
    static LARGE_INTEGER freq = {0};
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
    }
    LARGE_INTEGER count;
    QueryPerformanceCounter(&count);
    return (uint64_t)(((double)count.QuadPart / (double)freq.QuadPart) * 1e9);
}

#ifdef __cplusplus
}
#endif

#endif // _MSC_VER
