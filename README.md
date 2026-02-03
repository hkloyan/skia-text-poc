# Cross-Platform Skia Text Rendering PoC

A proof-of-concept for cross-platform text rendering using Skia's SkParagraph (C++), targeting both Web (WASM) and iOS with Metal.

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
git clone <repo-url>
cd skia-text-poc
git submodule update --init --recursive
```

### 2. Sync Skia Dependencies
```bash
./scripts/sync_skia_deps.sh
```

### 3. Build Skia Libraries (one-time, ~30-60 mins per platform)
```bash
# For WASM
./scripts/build_skia_wasm.sh

# For iOS
./scripts/build_skia_ios.sh
```

## Web Platform (WASM)

### Build
```bash
./scripts/build_wasm.sh
```

### Run
```bash
# From demos/web directory
cd demos/web
python3 -m http.server 8080
# Open http://localhost:8080 in browser
```

## iOS Platform

### Prerequisites
- Xcode 15.0+
- [XcodeGen](https://github.com/yonaskolb/XcodeGen) (optional, for regenerating project)

### Generate and Open Project
```bash
cd demos/ios
xcodegen generate
open SkiaTextPoc.xcodeproj
```

### Build and Run
1. Select the `SkiaTextPoc` scheme
2. Choose a simulator (Debug) or device (Release)
3. Build and run (⌘R)

**Note:** Debug builds link against x86_64 simulator libraries, Release builds link against arm64 device libraries.

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

Both platforms render identical test text:
```
"Hello, World! This is a test of Skia text rendering across platforms. 
The quick brown fox jumps over the lazy dog. 
We want to ensure this renders identically on Web and iOS."
```

Compare metrics between platforms:
- Height (should match)
- Width (should match)
- Line Count (should match)
- Max/Min Intrinsic Width (should match)

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      Demo Apps                               │
├─────────────────────────┬───────────────────────────────────┤
│   demos/web/            │         demos/ios/                 │
│   index.html, main.js   │   ViewController, SkiaMetalView   │
└─────────────┬───────────┴───────────────┬───────────────────┘
              │                           │
              ▼                           ▼
┌─────────────────────────────────────────────────────────────┐
│                   Platform Bindings                          │
├─────────────────────────┬───────────────────────────────────┤
│   platform/web/         │       platform/ios/                │
│   bindings.cpp (Embind) │       SkiaRenderer.mm (ObjC++)    │
└─────────────┬───────────┴───────────────┬───────────────────┘
              │                           │
              ▼                           ▼
┌─────────────────────────────────────────────────────────────┐
│              Shared C++ Core (namespace: core)               │
│                include/core/ + src/                          │
│              FontManager / TextRenderer                      │
└─────────────────────────────────────────────────────────────┘
              │
              ▼
┌─────────────────────────────────────────────────────────────┐
│                     Skia Libraries                           │
│   libskia │ libskparagraph │ libskshaper │ libharfbuzz      │
└─────────────────────────────────────────────────────────────┘
```

## Notes

- Both platforms use identical font files (Roboto) for consistent rendering
- Skia commit is pinned in `SKIA_VERSION` for reproducibility
- Pre-built libraries are stored in `third_party/skia-libs/`
- The WASM build uses Skia's bundled emsdk for ABI compatibility
