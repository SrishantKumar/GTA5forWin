#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

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

typedef void *wasm_rt_func_type_t;

typedef struct {
  void *data;
  void *data_end;
  size_t size;
  size_t pages;
  size_t max_pages;
  size_t page_size;
} wasm_rt_shared_memory_t;

typedef wasm_rt_shared_memory_t wasm_rt_memory_t;

typedef struct {
  uint32_t func_type;
  void *func;
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

#ifdef __cplusplus
}
#endif
