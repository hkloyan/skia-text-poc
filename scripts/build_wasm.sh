#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
SKIA_DIR="$PROJECT_ROOT/third_party/skia"
SKIA_LIBS="$PROJECT_ROOT/third_party/skia-libs/wasm"

echo "Building WASM module..."

# Use Skia's bundled emsdk for ABI compatibility, or fall back to PATH (CI)
# Version is defined in third_party/skia/bin/activate-emsdk
SKIA_EMSDK="$SKIA_DIR/third_party/externals/emsdk"
if [ -f "$SKIA_EMSDK/emsdk_env.sh" ]; then
    source "$SKIA_EMSDK/emsdk_env.sh"
elif command -v em++ &> /dev/null; then
    # emsdk already in PATH (e.g., CI with setup-emsdk action)
    echo "Using em++ from PATH: $(which em++)"
else
    echo "Error: Emscripten not found."
    echo "Run ./scripts/sync_skia_deps.sh to install Skia's bundled emsdk."
    exit 1
fi

cd "$PROJECT_ROOT"

# Create output directory
mkdir -p demos/web/dist

em++ \
  -std=c++17 \
  -Os \
  -fno-exceptions \
  -fno-rtti \
  -DEMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES=0 \
  -I "$PROJECT_ROOT/include" \
  -I "$SKIA_DIR" \
  -I "$SKIA_DIR/include" \
  -I "$SKIA_DIR/modules/skparagraph/include" \
  -I "$SKIA_DIR/modules/skunicode/include" \
  -DSK_GANESH \
  -DSK_GL \
  src/TextRenderer.cpp \
  src/FontManager.cpp \
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
  -o demos/web/dist/skia_text.js

echo "Build complete: demos/web/dist/skia_text.js"
