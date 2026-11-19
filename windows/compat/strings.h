#pragma once
#include <string.h>

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

#ifdef __cplusplus
}
#endif
