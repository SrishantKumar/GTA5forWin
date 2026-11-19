#pragma once
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdlib.h>
#include <string.h>

struct dirent {
    uint64_t d_ino;
    char d_name[MAX_PATH];
    unsigned char d_type;
};

#define DT_UNKNOWN 0
#define DT_DIR 4
#define DT_REG 8

typedef struct DIR {
    HANDLE handle;
    WIN32_FIND_DATAW findData;
    struct dirent ent;
    int first;
} DIR;

#ifdef __cplusplus
extern "C" {
#endif

static inline DIR *opendir(const char *name) {
    if (!name || !*name) return NULL;
    char pattern[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\*", name);

    wchar_t wpattern[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, pattern, -1, wpattern, MAX_PATH);

    DIR *dir = (DIR *)calloc(1, sizeof(DIR));
    if (!dir) return NULL;

    dir->handle = FindFirstFileW(wpattern, &dir->findData);
    if (dir->handle == INVALID_HANDLE_VALUE) {
        free(dir);
        return NULL;
    }
    dir->first = 1;
    return dir;
}

static inline struct dirent *readdir(DIR *dir) {
    if (!dir || dir->handle == INVALID_HANDLE_VALUE) return NULL;

    while (1) {
        if (dir->first) {
            dir->first = 0;
        } else {
            if (!FindNextFileW(dir->handle, &dir->findData)) {
                return NULL;
            }
        }

        // Skip "." and ".."
        if (wcscmp(dir->findData.cFileName, L".") == 0 || wcscmp(dir->findData.cFileName, L"..") == 0) {
            continue;
        }

        WideCharToMultiByte(CP_UTF8, 0, dir->findData.cFileName, -1, dir->ent.d_name, sizeof(dir->ent.d_name), NULL, NULL);
        dir->ent.d_type = (dir->findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? DT_DIR : DT_REG;
        return &dir->ent;
    }
}

static inline DIR *fdopendir(int fd) {
    (void)fd;
    return NULL;
}

static inline int closedir(DIR *dir) {
    if (!dir) return -1;
    if (dir->handle != INVALID_HANDLE_VALUE) {
        FindClose(dir->handle);
    }
    free(dir);
    return 0;
}

#ifdef __cplusplus
}
#endif
#endif
