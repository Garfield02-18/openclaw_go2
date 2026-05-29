#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
SKILL_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
BUILD_DIR="$SKILL_DIR/build"
SDK_ROOT=${UNITREE_SDK2_ROOT:-/home/radxa/unitree_sdk2/unitree_sdk2-main}

cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" -DUNITREE_SDK2_ROOT="$SDK_ROOT"
cmake --build "$BUILD_DIR" --target go2_controller -j"$(nproc)"

echo "Built: $BUILD_DIR/bin/go2_controller"
