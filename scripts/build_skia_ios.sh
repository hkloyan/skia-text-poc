#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
SKIA_DIR="$PROJECT_ROOT/third_party/skia"

# Ensure depot_tools is in PATH
export PATH="$HOME/repos/tools/depot_tools:$PATH"

cd "$SKIA_DIR"

echo "Building Skia for iOS arm64 (device)..."

# Build for arm64 (device)
GN_ARGS_ARM64=$(cat "$PROJECT_ROOT/gn_args/ios_arm64.gn" | tr '\n' ' ')
bin/gn gen out/ios_arm64 --args="$GN_ARGS_ARM64"
ninja -C out/ios_arm64 libskia.a libskparagraph.a libskshaper.a libskunicode_core.a libskunicode_icu.a

mkdir -p "$PROJECT_ROOT/third_party/skia-libs/ios-arm64"
cp out/ios_arm64/*.a "$PROJECT_ROOT/third_party/skia-libs/ios-arm64/"

echo "iOS arm64 build complete!"

echo "Building Skia for iOS x64 (simulator)..."

# Build for x64 (simulator)
GN_ARGS_X64=$(cat "$PROJECT_ROOT/gn_args/ios_x64_sim.gn" | tr '\n' ' ')
bin/gn gen out/ios_x64_sim --args="$GN_ARGS_X64"
ninja -C out/ios_x64_sim libskia.a libskparagraph.a libskshaper.a libskunicode_core.a libskunicode_icu.a

mkdir -p "$PROJECT_ROOT/third_party/skia-libs/ios-x64-sim"
cp out/ios_x64_sim/*.a "$PROJECT_ROOT/third_party/skia-libs/ios-x64-sim/"

echo "iOS x64 simulator build complete!"
echo "Libraries copied to third_party/skia-libs/ios-arm64/ and third_party/skia-libs/ios-x64-sim/"
