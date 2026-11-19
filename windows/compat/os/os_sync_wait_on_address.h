#pragma once
#include <stdint.h>
#include <stddef.h>

#define OS_SYNC_WAIT_ON_ADDRESS_NONE 0
#define OS_CLOCK_MACH_ABSOLUTE_TIME 0
#define OS_SYNC_WAKE_BY_ADDRESS_NONE 0

static inline int os_sync_wait_on_address(void *addr, uint64_t val, size_t size, uint32_t flags) {
    (void)addr; (void)val; (void)size; (void)flags;
    return -1;
}

static inline int os_sync_wait_on_address_with_timeout(void *addr, uint64_t val, size_t size, uint32_t flags, int clock, uint64_t ns) {
    (void)addr; (void)val; (void)size; (void)flags; (void)clock; (void)ns;
    return -1;
}

static inline void os_sync_wake_by_address_any(void *addr, size_t size, uint32_t flags) {
    (void)addr; (void)size; (void)flags;
}
