#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
// pthread is available via windows/compat/pthread.h (pthreads-win32 / winpthread)
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;
typedef uint64_t u64;
typedef int64_t s64;
typedef float f32;
typedef double f64;

typedef enum {
  WASM_RT_TRAP_NONE = 0,
  WASM_RT_TRAP_OOB = 1,
  WASM_RT_TRAP_INT_OVERFLOW = 2,
  WASM_RT_TRAP_DIV_BY_ZERO = 3,
  WASM_RT_TRAP_INVALID_CONVERSION = 4,
  WASM_RT_TRAP_UNREACHABLE = 5,
  WASM_RT_TRAP_CALL_INDIRECT = 6,
  WASM_RT_TRAP_UNALIGNED = 7,
} wasm_rt_trap_t;

typedef enum {
  WASM_RT_I32,
  WASM_RT_I64,
  WASM_RT_F32,
  WASM_RT_F64,
  WASM_RT_V128,
  WASM_RT_FUNCREF,
  WASM_RT_EXTERNREF,
} wasm_rt_type_t;

typedef const char *wasm_rt_func_type_t;
typedef void (*wasm_rt_function_ptr_t)(void);

typedef struct {
  // data/data_end use volatile _Atomic uint8_t* on the macOS side; plain void*
  // here lets the C compiler accept the assignment without -Wpointer-sign.
  // Cast to uint8_t* at point of use (EngineHost.c already does this for reads).
  volatile uint8_t *data;
  volatile uint8_t *data_end;
  size_t size;
  size_t pages;
  size_t max_pages;
  size_t page_size;
  bool is64;             // 64-bit address space flag (always true for GTA V)
  pthread_mutex_t mem_lock; // protects memory resize operations
} wasm_rt_shared_memory_t;

typedef wasm_rt_shared_memory_t wasm_rt_memory_t;

typedef struct {
  wasm_rt_func_type_t func_type;
  wasm_rt_function_ptr_t func;
  void *module_instance;
} wasm_rt_funcref_t;

typedef struct {
  wasm_rt_funcref_t *data;
  size_t size;
  size_t max_size;
} wasm_rt_funcref_table_t;

struct w2c_env;
wasm_rt_shared_memory_t *w2c_env_memory(struct w2c_env *host);

void wasm_rt_trap(wasm_rt_trap_t trap);
const char *wasm_rt_strerror(wasm_rt_trap_t trap);
void wasm_rt_init(void);
void wasm_rt_init_thread(void);
void wasm_rt_free_thread(void);
wasm_rt_trap_t wasm_rt_impl_try(void);

// Wasm atomic futex operations — declared here so any file that includes
// wasm-rt.h (via HostInternal.h → wasm-rt.h) has the prototypes.
uint32_t pg_atomic_wait32(wasm_rt_shared_memory_t *, uint64_t, uint32_t, uint64_t);
uint32_t pg_atomic_wait64(wasm_rt_shared_memory_t *, uint64_t, uint64_t, uint64_t);
uint32_t pg_atomic_notify(wasm_rt_shared_memory_t *, uint64_t, uint32_t);

#ifdef __cplusplus
}
#endif
