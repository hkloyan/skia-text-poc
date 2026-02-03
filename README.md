# Cross-Platform Skia Text Rendering PoC

A proof-of-concept for cross-platform text rendering using Skia's SkParagraph (C++), targeting both Web (WASM) and iOS with Metal.

## Features

- Rich text with multiple fonts, sizes, and colors
- Text styling: bold, italic, underline, letter/word spacing
- Text shadows and background highlights
- Paragraph controls: alignment, max lines, ellipsis, line height
- Interactive editing with cursor and selection
- Cross-platform metrics validation

## Project Structure

```
/skia-text-poc
  /include/core             # Public C++ headers (namespace: core)
    TextRenderer.h
    FontManager.h
  /src                      # C++ implementation
    TextRenderer.cpp
    FontManager.cpp
  /platform                 # Platform-specific bindings
    /web
      bindings.cpp          # Embind for JS interop
    /ios
      SkiaRenderer.mm/hh    # ObjC++ bridge
  /demos                    # Demo applications
    /web                    # Web demo
      index.html
      main.js
      /dist                 # Built WASM module
    /ios                    # iOS demo app
      project.yml           # XcodeGen spec
      /SkiaTextPoc          # Swift source files
  /assets
    /fonts                  # Google Fonts TTFs (Roboto, Playfair)
  /scripts
    sync_skia_deps.sh       # Sync Skia dependencies
    build_skia_wasm.sh      # Build Skia for WASM
    build_skia_ios.sh       # Build Skia for iOS
    build_wasm.sh           # Build the WASM module
  /gn_args
    wasm.gn                 # GN args for WASM build
    ios_arm64.gn            # GN args for iOS device
    ios_x64_sim.gn          # GN args for iOS simulator
  /third_party
    /skia                   # Git submodule (Google's Skia)
    /skia-libs              # Pre-built Skia libraries
  SKIA_VERSION              # Pinned Skia commit
```

## Prerequisites

### Required Tools
- **Xcode** (for iOS development)
- **depot_tools** (for Skia build)
- **Emscripten SDK** (for WASM build - automatically synced with Skia)
- **Ninja** (`brew install ninja`)

### Installing depot_tools
```bash
mkdir -p ~/repos/tools
cd ~/repos/tools
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
export PATH="$HOME/repos/tools/depot_tools:$PATH"
```

## Quick Start

### 1. Clone and Initialize
```bash
git clone https://github.com/hkloyan/skia-text-poc.git
cd skia-text-poc
git submodule update --init --recursive
```

### 2. Sync Skia Dependencies

This syncs Skia's third-party dependencies (includes the bundled Emscripten SDK needed for WASM builds):

```bash
./scripts/sync_skia_deps.sh
```

### 3. Build Skia Libraries (optional)

Pre-built Skia libraries are included in `third_party/skia-libs/`. You only need to rebuild if you want to update Skia or modify build options:

```bash
# ~30-60 mins per platform
./scripts/build_skia_wasm.sh  # For WASM
./scripts/build_skia_ios.sh   # For iOS
```

## Web Platform (WASM)

### Build
```bash
./scripts/build_wasm.sh
```

### Run
```bash
# From project root
python3 -m http.server 8080
# Open http://localhost:8080/demos/web/ in browser
```

## iOS Platform

### Prerequisites
- Xcode 15.0+
- [XcodeGen](https://github.com/yonaskolb/XcodeGen): `brew install xcodegen`

### Generate and Open Project
```bash
cd demos/ios
xcodegen generate
open SkiaTextPoc.xcodeproj
```

### Build and Run
1. Select the `SkiaTextPoc` scheme
2. Choose a physical iOS device
3. Build and run (⌘R)

### Project Structure
```
demos/ios/
├── project.yml              # XcodeGen spec
├── SkiaTextPoc.xcodeproj/   # Generated Xcode project
└── SkiaTextPoc/
    ├── AppDelegate.swift
    ├── SceneDelegate.swift
    ├── ViewController.swift
    ├── SkiaMetalView.swift
    ├── SkiaTextPoc-Bridging-Header.h
    └── Info.plist

platform/ios/
├── SkiaRenderer.hh          # ObjC++ bridge header
└── SkiaRenderer.mm          # ObjC++ bridge implementation
```

## Validation

Both demos render identical rich text with interactive editing. Compare:
- Layout metrics (height, width, line count)
- Intrinsic widths (max/min)
- Visual rendering (fonts, colors, decorations)

## Architecture

```
┌───────────────────────────────────────────────────────────┐
│                        Demo Apps                          │
├───────────────────────────┬───────────────────────────────┤
│  demos/web/               │  demos/ios/                   │
│  index.html, main.js      │  ViewController, SkiaMetalView│
└─────────────┬─────────────┴───────────────┬───────────────┘
              │                             │
              ▼                             ▼
┌───────────────────────────────────────────────────────────┐
│                    Platform Bindings                      │
├───────────────────────────┬───────────────────────────────┤
│  platform/web/            │  platform/ios/                │
│  bindings.cpp (Embind)    │  SkiaRenderer.mm (ObjC++)     │
└─────────────┬─────────────┴───────────────┬───────────────┘
              │                             │
              ▼                             ▼
┌───────────────────────────────────────────────────────────┐
│           Shared C++ Core (namespace: core)               │
│                 include/core/ + src/                      │
│             FontManager / TextRenderer                    │
└─────────────────────────────┬─────────────────────────────┘
                              │
                              ▼
┌───────────────────────────────────────────────────────────┐
│                      Skia Libraries                       │
│  libskia │ libskparagraph │ libskshaper │ libharfbuzz     │
└───────────────────────────────────────────────────────────┘
```

## Notes

- Both platforms use identical font files (Roboto, Playfair Display) for consistent rendering
- Skia commit is pinned in `SKIA_VERSION` for reproducibility
- Pre-built libraries are stored in `third_party/skia-libs/`
- The WASM build uses Skia's bundled emsdk for ABI compatibility

## Emoji Support

Emoji rendering requires a color emoji font. Each platform handles this differently:

### iOS
Emojis work automatically via CoreText fallback to Apple Color Emoji.

### Web
The bundled fonts don't include emoji glyphs. The demo provides a "Load System Emoji Font" button that uses the [Local Font Access API](https://developer.chrome.com/docs/capabilities/web-apis/local-fonts) to load your system's emoji font:
- **macOS**: Apple Color Emoji
- **Windows**: Segoe UI Emoji
- **Linux**: Noto Color Emoji (if installed)

**Notes:**
- Only works in Chromium browsers (Chrome, Edge) - Firefox/Safari don't support this API
- Permission is requested once, then remembered for subsequent visits
- After granting permission, emojis load automatically on page refresh

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

For third-party licenses (Skia, fonts, etc.), see [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).
