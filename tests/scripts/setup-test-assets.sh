#!/bin/bash
# Setup test assets by creating symlinks to the WASM build and fonts

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TESTS_DIR="$(dirname "$SCRIPT_DIR")"
PROJECT_ROOT="$(dirname "$TESTS_DIR")"

echo "Setting up test assets..."

# Create public directory structure
mkdir -p "$TESTS_DIR/public/dist"
mkdir -p "$TESTS_DIR/public/fonts"

# Link WASM build artifacts
if [ -d "$PROJECT_ROOT/demos/web/dist" ]; then
    echo "Linking WASM build from demos/web/dist..."
    ln -sf "$PROJECT_ROOT/demos/web/dist/skia_text.js" "$TESTS_DIR/public/dist/skia_text.js"
    ln -sf "$PROJECT_ROOT/demos/web/dist/skia_text.wasm" "$TESTS_DIR/public/dist/skia_text.wasm"
else
    echo "WARNING: demos/web/dist not found. Run the WASM build first."
    echo "  cd $PROJECT_ROOT && ./scripts/build_wasm.sh"
fi

# Link font files
echo "Linking fonts from assets/fonts..."
ln -sf "$PROJECT_ROOT/assets/fonts/Roboto-Regular.ttf" "$TESTS_DIR/public/fonts/Roboto-Regular.ttf"
ln -sf "$PROJECT_ROOT/assets/fonts/Roboto-Bold.ttf" "$TESTS_DIR/public/fonts/Roboto-Bold.ttf"
ln -sf "$PROJECT_ROOT/assets/fonts/PlayfairDisplay-Regular.ttf" "$TESTS_DIR/public/fonts/PlayfairDisplay-Regular.ttf"
ln -sf "$PROJECT_ROOT/assets/fonts/PlayfairDisplay-Italic.ttf" "$TESTS_DIR/public/fonts/PlayfairDisplay-Italic.ttf"

echo "Test assets setup complete!"
echo ""
echo "To run tests:"
echo "  cd $TESTS_DIR"
echo "  npm install"
echo "  npm test"
