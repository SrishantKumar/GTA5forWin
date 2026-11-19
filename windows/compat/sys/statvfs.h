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
#include <string.h>

struct statvfs {
    unsigned long f_bsize;
    unsigned long f_frsize;
    uint64_t f_blocks;
    uint64_t f_bfree;
    uint64_t f_bavail;
    uint64_t f_files;
    uint64_t f_ffree;
    unsigned long f_fsid;
    unsigned long f_flag;
    unsigned long f_namemax;
};

#ifdef __cplusplus
extern "C" {
#endif

static inline int statvfs(const char *path, struct statvfs *buf) {
    if (!buf) return -1;
    memset(buf, 0, sizeof(*buf));
    ULARGE_INTEGER freeBytesAvailable, totalNumberOfBytes, totalNumberOfFreeBytes;
    if (GetDiskFreeSpaceExA(path, &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
        buf->f_bsize = 4096;
        buf->f_frsize = 4096;
        buf->f_blocks = totalNumberOfBytes.QuadPart / 4096;
        buf->f_bfree = totalNumberOfFreeBytes.QuadPart / 4096;
        buf->f_bavail = freeBytesAvailable.QuadPart / 4096;
        buf->f_files = 1000000;
        buf->f_ffree = 1000000;
        buf->f_namemax = MAX_PATH;
        return 0;
    }
    return -1;
}

#ifdef __cplusplus
}
#endif
#endif
