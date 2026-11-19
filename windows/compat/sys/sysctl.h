#pragma once
#include <stddef.h>
static inline int sysctlbyname(const char *name, void *oldp, size_t *oldlenp, void *newp, size_t newlen) {
    (void)name; (void)oldp; (void)oldlenp; (void)newp; (void)newlen;
    return -1;
}
