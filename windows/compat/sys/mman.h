#pragma once
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stddef.h>

#define PROT_NONE  0
#define PROT_READ  1
#define PROT_WRITE 2
#define PROT_EXEC  4

#define MAP_SHARED    0x01
#define MAP_PRIVATE   0x02
#define MAP_FIXED     0x10
#define MAP_ANON      0x20
#define MAP_ANONYMOUS MAP_ANON
#define MAP_FAILED    ((void *)-1)

#ifdef __cplusplus
extern "C" {
#endif

static inline void *mmap(void *addr, size_t length, int prot, int flags, int fd, size_t offset) {
    (void)addr; (void)flags; (void)fd; (void)offset;
    DWORD flProtect = PAGE_NOACCESS;
    if ((prot & PROT_READ) && (prot & PROT_WRITE)) flProtect = PAGE_READWRITE;
    else if (prot & PROT_READ) flProtect = PAGE_READONLY;
    else if (prot & PROT_WRITE) flProtect = PAGE_READWRITE;

    void *ptr = VirtualAlloc(addr, length, MEM_COMMIT | MEM_RESERVE, flProtect);
    return ptr ? ptr : MAP_FAILED;
}

static inline int munmap(void *addr, size_t length) {
    (void)length;
    return VirtualFree(addr, 0, MEM_RELEASE) ? 0 : -1;
}

static inline int mprotect(void *addr, size_t len, int prot) {
    DWORD flProtect = PAGE_NOACCESS;
    if ((prot & PROT_READ) && (prot & PROT_WRITE)) flProtect = PAGE_READWRITE;
    else if (prot & PROT_READ) flProtect = PAGE_READONLY;
    DWORD oldProtect;
    return VirtualProtect(addr, len, flProtect, &oldProtect) ? 0 : -1;
}

#ifdef __cplusplus
}
#endif
#endif
