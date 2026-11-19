#pragma once
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#define RTLD_LAZY   1
#define RTLD_NOW    2
#define RTLD_LOCAL  4
#define RTLD_GLOBAL 8
#define RTLD_DEFAULT ((void*)0)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Dl_info {
    const char *dli_fname;
    void       *dli_fbase;
    const char *dli_sname;
    void       *dli_saddr;
} Dl_info;

static inline void *dlopen(const char *filename, int flag) {
    (void)flag;
    if (!filename) return (void *)GetModuleHandleA(NULL);
    return (void *)LoadLibraryA(filename);
}

static inline void *dlsym(void *handle, const char *symbol) {
    if (!handle) handle = (void *)GetModuleHandleA(NULL);
    return (void *)GetProcAddress((HMODULE)handle, symbol);
}

static inline int dlclose(void *handle) {
    if (!handle || handle == (void *)GetModuleHandleA(NULL)) return 0;
    return FreeLibrary((HMODULE)handle) ? 0 : -1;
}

static inline const char *dlerror(void) {
    return NULL;
}

static inline int dladdr(const void *addr, Dl_info *info) {
    (void)addr;
    if (info) {
        info->dli_fname = "";
        info->dli_fbase = NULL;
        info->dli_sname = "";
        info->dli_saddr = NULL;
    }
    return 0;
}

#ifdef __cplusplus
}
#endif
#endif
