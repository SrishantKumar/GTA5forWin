#pragma once
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline size_t os_proc_available_memory(void) {
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        return (size_t)status.ullAvailPhys;
    }
    return (size_t)(4096ull << 20);
}

#ifdef __cplusplus
}
#endif
#endif
