#pragma once
#include <stdint.h>
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
    return 1;
}
