#pragma once
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>
#include <process.h>
#include <direct.h>

#define F_OK 0
#define R_OK 4
#define W_OK 2
#define X_OK 1

#ifdef __cplusplus
extern "C" {
#endif

static inline int usleep(unsigned int usec) {
    Sleep(usec / 1000);
    return 0;
}

static inline unsigned int sleep(unsigned int seconds) {
    Sleep(seconds * 1000);
    return 0;
}

static inline int ftruncate(int fd, int64_t length) {
    return _chsize_s(fd, length) == 0 ? 0 : -1;
}

#ifndef strcasecmp
#define strcasecmp _stricmp
#endif
#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif

#ifdef __cplusplus
}
#endif
#endif
