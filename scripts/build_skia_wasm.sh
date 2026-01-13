#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
SKIA_DIR="$PROJECT_ROOT/third_party/skia"

echo "Building Skia for WASM..."

# Source emsdk environment
source "$HOME/repos/tools/emsdk/emsdk_env.sh"

# Ensure depot_tools is in PATH
export PATH="$HOME/repos/tools/depot_tools:$PATH"

cd "$SKIA_DIR"

# Read GN args from file and generate build
GN_ARGS=$(cat "$PROJECT_ROOT/gn_args/wasm.gn" | tr '\n' ' ')
bin/gn gen out/wasm --args="$GN_ARGS"

# Build required targets
ninja -C out/wasm libskia.a libskparagraph.a libskshaper.a libskunicode_core.a libskunicode_icu.a

# Copy outputs to skia-libs
mkdir -p "$PROJECT_ROOT/third_party/skia-libs/wasm"
cp out/wasm/*.a "$PROJECT_ROOT/third_party/skia-libs/wasm/"

echo "WASM build complete! Libraries copied to third_party/skia-libs/wasm/"
