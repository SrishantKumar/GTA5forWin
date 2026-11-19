#pragma once
#include <time.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#ifndef _TIMEVAL_DEFINED
#define _TIMEVAL_DEFINED
struct timeval {
    long tv_sec;
    long tv_usec;
};
#endif

#ifndef _TIMEZONE_DEFINED
#define _TIMEZONE_DEFINED
struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};
#endif

#ifdef __cplusplus
extern "C" {
#endif

static inline int gettimeofday(struct timeval *tv, struct timezone *tz) {
    (void)tz;
    if (tv) {
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        ULARGE_INTEGER uli;
        uli.LowPart = ft.dwLowDateTime;
        uli.HighPart = ft.dwHighDateTime;
        uint64_t t = (uli.QuadPart - 116444736000000000ULL) / 10ULL;
        tv->tv_sec = (long)(t / 1000000ULL);
        tv->tv_usec = (long)(t % 1000000ULL);
    }
    return 0;
}

#ifdef __cplusplus
}
#endif
#endif
