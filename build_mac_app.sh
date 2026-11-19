#!/bin/bash
set -euo pipefail

# GTA5Port Mac & AOT Engine preparation script
# Usage: ./build_mac_app.sh --mirror /path/to/game-folder

cd "$(dirname "$0")"

echo "======================================================================"
echo "  Grand Theft Auto V — Ahead-of-Time (AOT) Engine Generator"
echo "======================================================================"

python3 app/Scripts/prepare_aot.py "$@"

echo ""
echo "======================================================================"
echo "  AOT Engine Preparation Completed Successfully!"
echo "  Files generated in app/Generated:"
echo "    - game.h"
echo "    - game_0.c .. game_63.c"
echo "    - Imports.c"
echo "    - engine-functions.h"
echo "    - asi-natives.inc"
echo "======================================================================"
