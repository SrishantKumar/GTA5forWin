#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline uint32_t _dyld_image_count(void) { return 0; }
static inline const char *_dyld_get_image_name(uint32_t i) { (void)i; return ""; }
static inline const void *_dyld_get_image_header(uint32_t i) { (void)i; return NULL; }
static inline intptr_t _dyld_get_image_vmaddr_slide(uint32_t i) { (void)i; return 0; }

#ifdef __cplusplus
}
#endif
