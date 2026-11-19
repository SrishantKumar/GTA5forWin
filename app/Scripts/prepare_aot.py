#!/usr/bin/env python3
"""Cross-platform preparation script for Ahead-of-Time (AOT) WebAssembly compilation.

Converts game.wasm into C sources (app/Generated/game.h and game_*.c) using wasm2c,
extracts engine function names, generates missing host import stubs, and applies all
engine source-level patches.

Supports macOS, Linux, and Windows.
"""

import argparse
import hashlib
import os
import platform
import re
import shutil
import subprocess
import sys
import tarfile
import urllib.request
from pathlib import Path

EXPECTED_SHA256 = '11ca8d2c04c5e843d18ff4aea4899d72c86973c6b031df334e67c446b2ae83e0'

root = Path(__file__).resolve().parents[1]
tools_dir = root / 'Tools'
generated_dir = root / 'Generated'
resources_dir = root / 'Resources'

# All engine patch scripts in the order defined by build_native.sh
PATCH_SCRIPTS = [
    ('prepare_plain_engine.py', ['off']),
    ('prepare_heap_diagnostics.py', []),
    ('prepare_memory_pressure.py', []),
    ('prepare_virtual_pad.py', []),
    ('prepare_movie_wait.py', []),
    ('prepare_tv_without_video.py', []),
    ('prepare_bink_queue.py', []),
    ('prepare_menu_shadow.py', []),
    ('prepare_settings_restart.py', []),
    ('prepare_missing_text.py', []),
    ('prepare_help_label_check.py', []),
    ('prepare_clip_offsets.py', []),
    ('prepare_handler_frame.py', []),
    ('prepare_netlog_flush.py', []),
    ('prepare_input_exclusive.py', []),
    ('prepare_restore_flush.py', []),
    ('prepare_destroyed_phases.py', []),
    ('prepare_heap_sizes.py', []),
    ('prepare_dependency_guard.py', []),
    ('prepare_script_request_guard.py', []),
    ('prepare_missing_txd.py', []),
    ('prepare_missing_scaleform.py', []),
    ('prepare_mod_archives.py', []),
    ('prepare_asi_natives.py', []),
    ('prepare_asi_script_tick.py', []),
    ('prepare_unchecked_memory.py', []),
    ('prepare_trap_context.py', []),
    ('prepare_scenario_blockers.py', []),
    ('prepare_level_index.py', []),
    ('prepare_fixup_guard.py', []),
    ('prepare_format_strings.py', []),
    ('prepare_video_memory.py', []),
    ('prepare_plain_dsp.py', []),
    ('prepare_cash_stats.py', []),
    ('prepare_native_opus.py', []),
    ('prepare_native_inflate.py', []),
    ('prepare_render_list_guard.py', []),
    ('prepare_stored_vehicle_mods.py', []),
    ('prepare_hud_layout.py', []),
    ('prepare_stale_texture_slots.py', []),
    ('prepare_control_trace.py', []),
    ('prepare_pause_ready.py', []),
    ('prepare_streaming_health.py', []),
    ('prepare_evict_protect.py', []),
    ('prepare_mesh_release.py', []),
    ('prepare_txd_parent.py', []),
    ('prepare_stream_alloc_trace.py', []),
    ('prepare_request_trace.py', []),
    ('prepare_heap_return.py', []),
    ('prepare_user_music.py', []),
    ('prepare_no_debug_text.py', []),
    ('prepare_scaled_look.py', []),
    ('prepare_thread_tables.py', []),
    ('prepare_frame_limit_sleep.py', []),
    ('prepare_script_fault_abort.py', []),
    ('prepare_occlusion_late.py', []),
    ('prepare_d3d_storage_stats.py', []),
    ('prepare_d3d_lazy_copies.py', []),
    ('prepare_memory_file_create.py', []),
    ('prepare_longjmp.py', []),
    ('prepare_trap_guards.py', []),
    ('prepare_stale_cbuffers.py', []),
    ('prepare_plain_engine.py', []),
    ('prepare_memory_base.py', []),
    ('prepare_shared_tables.py', []),
]


def log(msg):
    print(f'[AOT Prepare] {msg}', flush=True)


def download_file(url, destination):
    log(f'Downloading {url}...')
    destination.parent.mkdir(parents=True, exist_ok=True)
    with urllib.request.urlopen(url, timeout=300) as response:
        destination.write_bytes(response.read())


def ensure_wabt():
    """Ensure WABT 1.0.42 tools and runtime are available."""
    wabt_dir = tools_dir / 'wabt-1.0.42'
    if wabt_dir.exists():
        return wabt_dir

    system = platform.system()
    machine = platform.machine().lower()
    log(f'Detecting platform for WABT release: system={system}, machine={machine}')

    release = 'https://github.com/WebAssembly/wabt/releases/download/1.0.42/'
    if system == 'Windows':
        filename = 'wabt-1.0.42-windows-x64.tar.gz'
    elif system == 'Darwin':
        filename = 'wabt-1.0.42-macos-arm64.tar.gz' if ('arm' in machine or 'aarch64' in machine) else 'wabt-1.0.42-macos-12.tar.gz'
    else:
        filename = 'wabt-1.0.42-linux-x64.tar.gz'

    archive = tools_dir / 'wabt.tar.gz'
    if not archive.exists():
        download_file(release + filename, archive)

    log(f'Extracting {archive}...')
    with tarfile.open(archive) as package:
        package.extractall(tools_dir)

    return wabt_dir


def ensure_simde():
    """Ensure simde header library is available in Tools/simde."""
    simde_dir = tools_dir / 'simde'
    if simde_dir.exists():
        return simde_dir

    archive = tools_dir / 'simde.tar.gz'
    if not archive.exists():
        download_file('https://github.com/simd-everywhere/simde/archive/refs/tags/v0.8.2.tar.gz', archive)

    log(f'Extracting {archive}...')
    with tarfile.open(archive) as package:
        package.extractall(tools_dir)

    extracted = tools_dir / 'simde-0.8.2'
    if extracted.exists() and not simde_dir.exists():
        extracted.rename(simde_dir)

    return simde_dir


def patch_wabt_runtime(wabt_dir):
    """Patch wasm-rt-impl.c to route traps to pg_record_trap in a cross-platform manner."""
    runtime = wabt_dir / 'share/wabt/wasm2c/wasm-rt-impl.c'
    if not runtime.exists():
        return
    text = runtime.read_text()
    if 'pg_record_trap' not in text:
        replacement = (
            '#if defined(_MSC_VER)\n'
            'extern void pg_record_trap(int);\n'
            '#define PG_HAVE_RECORD_TRAP 1\n'
            '#else\n'
            'extern void pg_record_trap(int) __attribute__((weak));\n'
            '#define PG_HAVE_RECORD_TRAP (pg_record_trap != NULL)\n'
            '#endif\n'
            'void wasm_rt_trap(wasm_rt_trap_t code) {\n'
            '  if (PG_HAVE_RECORD_TRAP) pg_record_trap(code);'
        )
        text = text.replace('void wasm_rt_trap(wasm_rt_trap_t code) {', replacement, 1)
        runtime.write_text(text)
        log('Patched wabt runtime wasm-rt-impl.c for pg_record_trap.')


def get_wasm2c_executable(wabt_dir):
    """Find or build the wasm2c native executable."""
    native_exec = tools_dir / ('wasm2c-native.exe' if platform.system() == 'Windows' else 'wasm2c-native')
    if native_exec.exists():
        return str(native_exec)

    system = platform.system()
    # On macOS / Linux, build wasm2c-native with AtomicWait patch if possible
    if system in ('Darwin', 'Linux'):
        c_orig = tools_dir / 'c-writer-original.cc'
        wasm2c_main = tools_dir / 'wasm2c-main.cc'
        if not c_orig.exists():
            download_file('https://raw.githubusercontent.com/WebAssembly/wabt/1.0.42/src/c-writer.cc', c_orig)
        if not wasm2c_main.exists():
            download_file('https://raw.githubusercontent.com/WebAssembly/wabt/1.0.42/src/tools/wasm2c.cc', wasm2c_main)

        subprocess.run([sys.executable, str(root / 'Scripts/patch_wasm2c.py')], check=True)
        sha256_shim = tools_dir / 'sha256_shim.o'
        subprocess.run(['clang', '-Wno-deprecated-declarations', '-c', str(root / 'Scripts/sha256_shim.c'), '-o', str(sha256_shim)], check=True)
        subprocess.run(['clang++', '-std=c++20', '-O1', '-I', str(wabt_dir / 'include'),
                        str(wasm2c_main), str(tools_dir / 'c-writer-patched.cc'), str(sha256_shim),
                        str(wabt_dir / 'lib/libwabt.a'), '-o', str(native_exec)], check=True)
        return str(native_exec)

    # Fallback to pre-built wasm2c from wabt release
    wabt_bin = wabt_dir / 'bin' / ('wasm2c.exe' if system == 'Windows' else 'wasm2c')
    if wabt_bin.exists():
        return str(wabt_bin)

    raise RuntimeError(f'Could not locate wasm2c executable in {wabt_dir}')


def find_game_wasm(options):
    """Locate or download game.wasm."""
    if options.url:
        wasm_path = tools_dir / 'game.wasm'
        download_file(options.url, wasm_path)
        return wasm_path

    if options.wasm:
        p = Path(options.wasm).resolve()
        if p.exists():
            return p
        raise FileNotFoundError(f'Specified game.wasm not found: {options.wasm}')

    if options.mirror:
        m = Path(options.mirror).resolve()
        candidates = [
            m / 'b/8b0b5899ed/game.wasm',
            m / 'game.wasm',
            m / '8b0b5899ed/game.wasm',
        ]
        for c in candidates:
            if c.exists():
                return c
        raise FileNotFoundError(f'Could not find game.wasm in mirror directory: {options.mirror}')

    # Try common local defaults
    defaults = [
        Path('/Volumes/Zebronics/GTV-5/b/8b0b5899ed/game.wasm'),
        root.parent / 'b/8b0b5899ed/game.wasm',
        root / 'b/8b0b5899ed/game.wasm',
        root / 'game.wasm',
    ]
    for d in defaults:
        if d.exists():
            return d

    raise FileNotFoundError('game.wasm not found. Pass --mirror, --wasm, or --url.')


def main():
    parser = argparse.ArgumentParser(description='Prepare AOT WebAssembly engine files for compilation')
    parser.add_argument('--mirror', type=Path, help='Directory containing data/ and b/8b0b5899ed/game.wasm')
    parser.add_argument('--wasm', type=Path, help='Direct path to game.wasm')
    parser.add_argument('--url', type=str, help='Direct download URL for game.wasm')
    parser.add_argument('--num-outputs', type=int, default=64, help='Number of split C source outputs (default: 64)')
    parser.add_argument('--skip-sha', action='store_true', help='Skip SHA-256 verification')
    args = parser.parse_args()

    for d in (tools_dir, generated_dir, resources_dir):
        d.mkdir(parents=True, exist_ok=True)

    wasm_path = find_game_wasm(args)
    log(f'Using game.wasm: {wasm_path} ({wasm_path.stat().st_size / (1024*1024):.1f} MB)')

    if not args.skip_sha:
        log('Verifying SHA-256...')
        h = hashlib.sha256(wasm_path.read_bytes()).hexdigest()
        if h != EXPECTED_SHA256:
            log(f'WARNING: SHA-256 mismatch!\n  Got:      {h}\n  Expected: {EXPECTED_SHA256}')
        else:
            log('SHA-256 checksum verified (build 8b0b5899ed).')

    wabt_dir = ensure_wabt()
    ensure_simde()
    patch_wabt_runtime(wabt_dir)
    wasm2c = get_wasm2c_executable(wabt_dir)
    log(f'Using wasm2c compiler: {wasm2c}')

    out_c = generated_dir / 'game.c'
    cmd = [
        wasm2c,
        '--enable-threads',
        '--no-debug-names',
        '--module-name=game',
        f'--num-outputs={args.num_outputs}',
        str(wasm_path),
        '-o',
        str(out_c),
    ]
    log(f'Running wasm2c AOT translation: {" ".join(cmd)}')
    subprocess.run(cmd, check=True)

    log('Extracting engine function names...')
    subprocess.run([sys.executable, str(root / 'Scripts/engine_names.py'), '--wasm', str(wasm_path)], check=True)

    log('Generating missing import bindings (Imports.c)...')
    subprocess.run([sys.executable, str(root / 'Scripts/generate_imports.py')], check=True)

    log('Applying engine code patches...')
    scripts_dir = root / 'Scripts'
    for script_name, extra_args in PATCH_SCRIPTS:
        script_file = scripts_dir / script_name
        if script_file.exists():
            subprocess.run([sys.executable, str(script_file)] + extra_args, cwd=str(root), check=True)

    # Copy manifest.json if present in mirror
    if args.mirror:
        manifest_cand = args.mirror / 'data/manifest.json'
        if manifest_cand.exists():
            shutil.copyfile(manifest_cand, resources_dir / 'manifest.json')
            log(f'Copied manifest.json to {resources_dir / "manifest.json"}')

    log('Ahead-of-Time (AOT) engine preparation complete!')
    log(f'Generated files in {generated_dir}:')
    for f in sorted(generated_dir.glob('*')):
        if f.is_file():
            size_mb = f.stat().st_size / (1024 * 1024)
            if size_mb > 0.1:
                log(f'  {f.name} ({size_mb:.1f} MB)')
            else:
                log(f'  {f.name} ({f.stat().st_size / 1024:.1f} KB)')


if __name__ == '__main__':
    main()
