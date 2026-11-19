#include "wasm-rt.h"
#include <stdio.h>
#include <stdlib.h>

void wasm_rt_trap(wasm_rt_trap_t trap) {
  fprintf(stderr, "[WASM-RT TRAP] code: %d\n", trap);
}

const char *wasm_rt_strerror(wasm_rt_trap_t trap) {
  switch (trap) {
    case WASM_RT_TRAP_NONE: return "none";
    case WASM_RT_TRAP_OOB: return "out of bounds memory access";
    case WASM_RT_TRAP_INT_OVERFLOW: return "integer overflow";
    case WASM_RT_TRAP_DIV_BY_ZERO: return "integer divide by zero";
    case WASM_RT_TRAP_INVALID_CONVERSION: return "invalid conversion to integer";
    case WASM_RT_TRAP_UNREACHABLE: return "unreachable executed";
    case WASM_RT_TRAP_CALL_INDIRECT: return "invalid indirect call";
    case WASM_RT_TRAP_UNALIGNED: return "unaligned atomic access";
    default: return "unknown trap";
  }
}

void wasm_rt_init(void) {}
void wasm_rt_init_thread(void) {}
void wasm_rt_free_thread(void) {}
wasm_rt_trap_t wasm_rt_impl_try(void) { return WASM_RT_TRAP_NONE; }
