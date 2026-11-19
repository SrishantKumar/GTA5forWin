#!/usr/bin/env python3
"""Stub generator for Generated/asi-natives.inc on Windows CI.

prepare_asi_natives.py requires Generated/game_*.c (the wasm2c output of the
GTA V binary), which is not committed.  When those files are absent this script
writes the minimum valid include so EngineHost.c compiles; the resulting binary
will have no Script-Hook-V mod support, but everything else works.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
generated = root / 'Generated'
generated.mkdir(exist_ok=True)

# Check whether the real generator can run (game_*.c present).
if list(generated.glob('game_*.c')):
    import subprocess, sys
    result = subprocess.run([sys.executable, str(root / 'Scripts' / 'prepare_asi_natives.py')])
    raise SystemExit(result.returncode)

target = generated / 'asi-natives.inc'
stub = (
    '// Stub: Generated/game_*.c not present; Script Hook V mod support disabled.\n'
    'static inline f32 pg_asi_f32(u64 slot) { f32 value; uint32_t bits = (uint32_t)slot; memcpy(&value, &bits, 4); return value; }\n'
    'static const struct { uint64_t hash; void (*handler)(w2c_game *, u64); } pg_asi_natives[] = {\n'
    '};\n'
)
if not target.exists() or target.read_text() != stub:
    target.write_text(stub)
    print('asi-natives.inc: wrote stub (no game_*.c found).')
else:
    print('asi-natives.inc: stub already up to date.')
