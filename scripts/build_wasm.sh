#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
SKIA_DIR="$PROJECT_ROOT/third_party/skia"
SKIA_LIBS="$PROJECT_ROOT/third_party/skia-libs/wasm"

echo "Building WASM module..."

# Source the same emsdk that Skia was built with
SKIA_EMSDK="$SKIA_DIR/third_party/externals/emsdk"
if [ -f "$SKIA_EMSDK/emsdk_env.sh" ]; then
    source "$SKIA_EMSDK/emsdk_env.sh"
else
    source "$HOME/repos/tools/emsdk/emsdk_env.sh"
fi

cd "$PROJECT_ROOT"

# Create output directory
mkdir -p platform/web/dist

em++ \
  -std=c++17 \
  -O2 \
  -fno-exceptions \
  -fno-rtti \
  -DEMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES=0 \
  -I "$PROJECT_ROOT" \
  -I "$SKIA_DIR" \
  -I "$SKIA_DIR/include" \
  -I "$SKIA_DIR/modules/skparagraph/include" \
  -I "$SKIA_DIR/modules/skunicode/include" \
  -DSK_GANESH \
  -DSK_GL \
  core/TextRenderer.cpp \
  core/FontManager.cpp \
  platform/web/bindings.cpp \
  "$SKIA_LIBS/libskparagraph.a" \
  "$SKIA_LIBS/libskshaper.a" \
  "$SKIA_LIBS/libskunicode_icu.a" \
  "$SKIA_LIBS/libskunicode_core.a" \
  "$SKIA_LIBS/libharfbuzz.a" \
  "$SKIA_LIBS/libicu.a" \
  "$SKIA_LIBS/libskia.a" \
  "$SKIA_LIBS/libfreetype2.a" \
  "$SKIA_LIBS/libpng.a" \
  "$SKIA_LIBS/libzlib.a" \
  "$SKIA_LIBS/libbrotli.a" \
  --bind \
  -sWASM=1 \
  -sALLOW_MEMORY_GROWTH=1 \
  -sUSE_WEBGL2=1 \
  -sMAX_WEBGL_VERSION=2 \
  -sMODULARIZE=1 \
  -sEXPORT_NAME="SkiaTextModule" \
  -sDISABLE_EXCEPTION_CATCHING=1 \
  -sEXPORTED_FUNCTIONS="['_malloc','_free']" \
  -sEXPORTED_RUNTIME_METHODS="['HEAPU8']" \
  -o platform/web/dist/skia_text.js

echo "Build complete: platform/web/dist/skia_text.js"
