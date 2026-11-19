#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int kern_return_t;
#define KERN_SUCCESS 0
typedef uint32_t mach_msg_type_number_t;
typedef void *mach_port_t;
#define mach_task_self() ((mach_port_t)0)
#define TASK_VM_INFO 22
#define TASK_VM_INFO_COUNT 0

typedef struct {
    uint64_t phys_footprint;
} task_vm_info_data_t;
typedef task_vm_info_data_t *task_info_t;

static inline kern_return_t task_info(mach_port_t t, int flavor, task_info_t info, mach_msg_type_number_t *count) {
    (void)t; (void)flavor; (void)count;
    if (info) info->phys_footprint = 0;
    return KERN_SUCCESS;
}

#define vm_page_size 4096
typedef uintptr_t vm_address_t;
typedef size_t vm_size_t;
typedef uint32_t natural_t;

typedef struct {
    int is_submap;
    uint32_t pages_dirtied;
    uint32_t pages_swapped_out;
    uint32_t user_tag;
} vm_region_submap_info_data_64_t;
typedef void *vm_region_recurse_info_t;
#define VM_REGION_SUBMAP_INFO_COUNT_64 0

static inline kern_return_t vm_region_recurse_64(mach_port_t t, vm_address_t *addr, vm_size_t *size, natural_t *depth, vm_region_recurse_info_t info, mach_msg_type_number_t *count) {
    (void)t; (void)addr; (void)size; (void)depth; (void)info; (void)count;
    return 1;
}

typedef mach_port_t thread_t;
typedef thread_t *thread_act_array_t;

static inline kern_return_t task_threads(mach_port_t t, thread_act_array_t *list, mach_msg_type_number_t *count) {
    (void)t; (void)list; (void)count;
    return 1;
}

#ifdef __cplusplus
}
#endif
