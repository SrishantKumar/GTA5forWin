#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "wasm-rt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define wasm2c_game_pagesize_env_memory 65536u

struct w2c_env;
struct w2c_wasi__snapshot__preview1;

typedef struct w2c_game {
    uint32_t w2c_g0;
    uint64_t w2c_g1;
    uint64_t w2c_g19;
    wasm_rt_memory_t *w2c_env_memory;
} w2c_game;

// Preflight engine stubs when AOT game.c has not been generated yet
static inline uint32_t w2c_game_htons(w2c_game *instance, uint32_t val) { (void)instance; return ((val & 0xFF) << 8) | ((val >> 8) & 0xFF); }
static inline void w2c_game_0x5F_wasm_call_ctors(w2c_game *instance) { (void)instance; }
static inline uint64_t w2c_game_emscripten_stack_get_current(w2c_game *instance) { (void)instance; return 0; }
static inline void w2c_game_0x5Femscripten_stack_restore(w2c_game *instance, uint64_t val) { (void)instance; (void)val; }
static inline uint32_t w2c_game_0x5F_main_argc_argv(w2c_game *instance, uint32_t argc, uint64_t argv) { (void)instance; (void)argc; (void)argv; return 0; }
static inline void w2c_game_emscripten_stack_set_limits(w2c_game *instance, uint64_t high, uint64_t low) { (void)instance; (void)high; (void)low; }
static inline void w2c_game_0x5Femscripten_run_js_on_main_thread_done(w2c_game *instance, uint64_t ctx, uint64_t ctxargs, double res) { (void)instance; (void)ctx; (void)ctxargs; (void)res; }
static inline void wasm2c_game_free(w2c_game *instance) { (void)instance; }
static inline wasm_rt_funcref_table_t *w2c_game_0x5F_indirect_function_table(w2c_game *instance) { (void)instance; static wasm_rt_funcref_table_t empty = {0}; return &empty; }
static inline wasm_rt_func_type_t wasm2c_game_get_func_type(uint32_t a, uint32_t b, ...) { (void)a; (void)b; static char dummy[32] = {0}; return dummy; }
static inline void wasm2c_game_instantiate(w2c_game *instance, struct w2c_env *env, struct w2c_wasi__snapshot__preview1 *wasi) {
    (void)instance; (void)env; (void)wasi;
}
static inline void w2c_game_0x5Femscripten_thread_init(w2c_game *instance, uint64_t a, uint32_t b, uint32_t c, uint32_t d, uint64_t e, uint32_t f) {
    (void)instance; (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
}
static inline void w2c_game_0x5Femscripten_thread_exit(w2c_game *instance, uint32_t result) {
    (void)instance; (void)result;
}
static inline void w2c_game_0x5Femscripten_tls_init(w2c_game *instance) {
    (void)instance;
}
static inline void w2c_game_f3111(w2c_game *instance, uint64_t job) {
    (void)instance; (void)job;
}

#ifdef __cplusplus
}
#endif
