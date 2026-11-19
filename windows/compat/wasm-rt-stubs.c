#include "wasm-rt.h"
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

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

static char gSelectedLanguage[32] = "en-US";
void PGSetSelectedGameLanguage(const char *lang) {
  if (lang && *lang) {
    strncpy(gSelectedLanguage, lang, sizeof(gSelectedLanguage) - 1);
    gSelectedLanguage[sizeof(gSelectedLanguage) - 1] = '\0';
  }
}
const char *PGSelectedGameLanguage(void) {
  return gSelectedLanguage;
}

static bool gModsOff = false;
void PGSetModsOff(bool off) {
  gModsOff = off;
}

static double gSubtitleRaise = 0.0;
void PGSetSubtitleRaise(double raise) {
  gSubtitleRaise = (raise >= 0.0 && raise < 0.3) ? raise : 0.0;
}

static double gMinimapDX = 0.0, gMinimapDY = 0.0, gMinimapScale = 1.0;
void PGSetMinimapLayout(double dx, double dy, double scale) {
  gMinimapDX = dx;
  gMinimapDY = dy;
  gMinimapScale = (scale > 0.4 && scale < 2.5) ? scale : 1.0;
}

static double gHelpX = 0.0, gHelpY = 0.0, gHelpScale = 1.0;
void PGSetHelpTextLayout(double x, double y, double scale) {
  gHelpX = (x >= -0.1 && x <= 1.0) ? x : 0.0;
  gHelpY = (y >= -0.1 && y <= 1.0) ? y : 0.0;
  gHelpScale = (scale > 0.4 && scale < 2.5) ? scale : 1.0;
}

uint64_t PGGPUTextureBytes(void) { return 0; }
uint64_t PGGPUBufferBytes(void) { return 0; }
uint64_t PGFrameRingBytes(void) { return 0; }
uint64_t PGConstantCopyBytes(void) { return 0; }
const char *pg_assets_manifest_override(void) { return NULL; }
void pg_assets_configure(const char *manifest, const char *root) { (void)manifest; (void)root; }
void pg_assets_prewarm(void) {}
void pg_mod_path_used(const char *path) { (void)path; }
const char *pg_mods_folder_path(void) { return ""; }
const char *pg_game_index_path(void) { return ""; }
const char *pg_game_files_version(void) { return "current"; }
bool pg_mods_archive_path(const char *enginePath, char *out, size_t cap) { (void)enginePath; (void)out; (void)cap; return false; }
struct PGModPiece;
int pg_mods_for_archive(const char *enginePath, const struct PGModPiece **pieces) { (void)enginePath; if (pieces) *pieces = NULL; return 0; }

static char gLocalGameFolder[1024] = {0};
void PGSetLocalGameFolder(const char *dataPath) {
  if (dataPath && *dataPath) {
    snprintf(gLocalGameFolder, sizeof(gLocalGameFolder), "%s", dataPath);
  } else {
    gLocalGameFolder[0] = 0;
  }
}
bool PGUsingLocalGameFolder(void) {
  return gLocalGameFolder[0] != 0;
}

uint64_t w2c_game_fn_begin_low_quality_photo_copy(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_begin_take_high_quality_photo(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_buddy_get_size(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_clear_wanted(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_clear_weather(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_clear_weather_override(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_create_vehicle(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_current_script_handler(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_delete_vehicle(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_dirty_adv_gfx_settings(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_dirty_gfx_settings(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_do_auto_save(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_entity_exists(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_entity_owned(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_get_current_ped_vehicle_weapon(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_get_entity_coords(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_get_entity_heading(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_get_entity_model(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_get_entity_speed(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_get_max_wanted(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_get_ped_drawable(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_get_ped_texture(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_get_player_invincible(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_get_wanted(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_give_weapon(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_ground_vehicle(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_hash_string_try_get(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_has_model_loaded(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_inflate(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_instructional_button(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_is_vehicle_model(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_load_scene_is_loaded(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_load_scene_start(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_lod_scale_this_frame(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_low_quality_photo_copy_status(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_menu_graphics_settings(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_model_info_at_index(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_operator_delete(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_operator_new(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_opus_decode_frame(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_parked_density_this_frame(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_ped_density_this_frame(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_ped_vehicle(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_player_id(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_player_ped_id(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_play_movie(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_preload_movie(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_profile_settings_set(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_reduce_ped_model_budget(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_reduce_vehicle_model_budget(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_release_model(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_repair_vehicle(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_request_model(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_request_scaleform_movie(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_resolution_index(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_sbrk(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_selected_weapon(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_set_clock(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_set_current_ped_vehicle_weapon(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_set_entity_coords_no_offset(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_set_entity_health(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_set_max_wanted(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_set_menu_preference(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_set_ped_into_vehicle(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_set_player_invincible(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_set_tv_channel(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_set_weather(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_stat_get_int(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_stat_set_int(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_stream_in_image(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_stream_object_name(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_stream_physical_size(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_stream_virtual_size(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_sys_ipc_sleep(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_take_high_quality_photo_status(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_tv_current_show_hash(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_tv_scale_to_real_time(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_tv_setup_lester1(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_tv_show_duration(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_tv_show_play_time(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_txd_find_slot(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_ui_settings(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_vehicle_class(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_vehicle_density_this_frame(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_vehicle_dirt(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_vehicle_engine_health(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_vehicle_label(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_vehicle_occupant(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_vehicle_seats(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_weapon_entity(void *inst, ...) { (void)inst; return 0; }
uint64_t w2c_game_fn_zstd_decompress_stream(void *inst, ...) { (void)inst; return 0; }

// Network syscall stubs — signatures must match game.h declarations exactly.
// All return ENOSYS (-1 as u32) since networking is not supported on Windows.
u32 w2c_env_0x5F_syscall_accept4(struct w2c_env *h, u32 a, u64 b, u64 c, u32 d, u32 e, u32 f) { (void)h;(void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return (u32)-1; }
u32 w2c_env_0x5F_syscall_connect(struct w2c_env *h, u32 a, u64 b, u32 c, u32 d, u32 e, u32 f) { (void)h;(void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return (u32)-1; }
u32 w2c_env_0x5F_syscall_getsockname(struct w2c_env *h, u32 a, u64 b, u64 c, u32 d, u32 e, u32 f) { (void)h;(void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return (u32)-1; }
u32 w2c_env_0x5F_syscall_getsockopt(struct w2c_env *h, u32 a, u32 b, u32 c, u64 d, u64 e, u32 f) { (void)h;(void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return (u32)-1; }
u32 w2c_env_0x5F_syscall_poll(struct w2c_env *h, u64 a, u32 b, u32 c) { (void)h;(void)a;(void)b;(void)c; return (u32)-1; }
u32 w2c_env_0x5F_syscall_recvfrom(struct w2c_env *h, u32 a, u64 b, u64 c, u32 d, u64 e, u64 f) { (void)h;(void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return (u32)-1; }
u32 w2c_env_0x5F_syscall_sendto(struct w2c_env *h, u32 a, u64 b, u64 c, u32 d, u64 e, u32 f) { (void)h;(void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return (u32)-1; }
u32 w2c_env_0x5F_syscall_setsockopt(struct w2c_env *h, u32 a, u32 b, u32 c, u64 d, u32 e, u32 f) { (void)h;(void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return (u32)-1; }
u32 w2c_env_0x5F_syscall_shutdown(struct w2c_env *h, u32 a, u32 b, u32 c, u32 d, u32 e, u32 f) { (void)h;(void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return (u32)-1; }
u32 w2c_env_0x5F_syscall_socket(struct w2c_env *h, u32 a, u32 b, u32 c, u32 d, u32 e, u32 f) { (void)h;(void)a;(void)b;(void)c;(void)d;(void)e;(void)f; return (u32)-1; }

const char *pg_app_symbol(uintptr_t pc) {
  (void)pc;
  return NULL;
}

