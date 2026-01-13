#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
SKIA_DIR="$PROJECT_ROOT/third_party/skia"
SKIA_LIBS="$PROJECT_ROOT/third_party/skia-libs/wasm"

echo "Building WASM module..."

# Source emsdk environment
source "$HOME/repos/tools/emsdk/emsdk_env.sh"

cd "$PROJECT_ROOT"

# Create output directory
mkdir -p platform/web/dist

em++ \
  -std=c++17 \
  -O2 \
  -I "$SKIA_DIR" \
  -I "$SKIA_DIR/include" \
  -I "$SKIA_DIR/modules/skparagraph/include" \
  -I "$SKIA_DIR/modules/skunicode/include" \
  core/TextRenderer.cpp \
  core/FontManager.cpp \
  platform/web/bindings.cpp \
  -L "$SKIA_LIBS" \
  -lskia -lskparagraph -lskshaper -lskunicode_core -lskunicode_icu \
  --bind \
  -sWASM=1 \
  -sALLOW_MEMORY_GROWTH=1 \
  -sUSE_WEBGL2=1 \
  -sMAX_WEBGL_VERSION=2 \
  -sMODULARIZE=1 \
  -sEXPORT_NAME="SkiaTextModule" \
  -o platform/web/dist/skia_text.js

echo "Build complete: platform/web/dist/skia_text.js"
