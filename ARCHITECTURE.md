# Architecture

## Overview

The core text system is a pipeline with a small facade:

```
TextDocument → TextLayout → TextDrawing
       ↑            ↑            ↑
       └──────── TextEditor (public API, caches, selection)
```

Platform bindings call `TextEditor`, which orchestrates document changes, layout updates, and drawing.

## Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                         Demo Apps                           │
├────────────────────────────┬────────────────────────────────┤
│  demos/web/                │  demos/ios/                    │
│  index.html, main.js       │  ViewController, SkiaMetalView │
└─────────────┬──────────────┴────────────────┬───────────────┘
              │                               │
              ▼                               ▼
┌─────────────────────────────────────────────────────────────┐
│                     Platform Bindings                       │
├────────────────────────────┬────────────────────────────────┤
│  platform/web/             │  platform/ios/                 │
│  bindings.cpp (Embind)     │  SkiaRenderer.mm (ObjC++)      │
└─────────────┬──────────────┴────────────────┬───────────────┘
              │                               │
              ▼                               ▼
┌─────────────────────────────────────────────────────────────┐
│            Shared C++ Core (namespace: core)                │
│                                                             │
│    ┌─────────────────────────────────────────────────┐      │
│    │                   TextEditor                    │      │
│    │              (public API facade)                │      │
│    └───────────┬─────────────┬─────────────┬─────────┘      │
│                │             │             │                │
│                ▼             ▼             ▼                │
│    ┌──────────────┐  ┌────────────┐  ┌──────────────┐       │
│    │ TextDocument │  │ TextLayout │  │ TextDrawing  │       │
│    │ (data)       │  │ (Skia      │  │ (stateless   │       │
│    │ text+styles  │  │  paragraph)│  │  rendering)  │       │
│    │ versioned    │  │ metrics    │  │              │       │
│    └──────────────┘  └────────────┘  └──────────────┘       │
│                                                             │
│    FontManager (font registration)                          │
└────────────────────────────┬────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────┐
│                       Skia Libraries                        │
│   libskia │ libskparagraph │ libskshaper │ libharfbuzz      │
└─────────────────────────────────────────────────────────────┘
```

## Core Components

- `TextDocument` (`include/core/text_document.hpp`, `src/text_document.cpp`)
  - Stores UTF-16 text, style runs, and default style.
  - Owns the document version counter (increments on any mutation).
  - No Skia dependency in the header; conversion lives in `TextEncoding`.

- `TextLayout` (`include/core/text_layout.hpp`, `src/text_layout.cpp`)
  - Builds and owns Skia `Paragraph`.
  - Computes metrics and hit testing.
  - Tracks `documentVersion` and `layoutRevision`.

- `TextDrawing` (`include/core/text_drawing.hpp`, `src/text_drawing.cpp`)
  - Stateless drawing helpers for text, selection, and cursor.
  - `TextEditor` uses `drawText`/`drawCursor` to integrate caching.

- `TextEditor` (`include/core/text_editor.hpp`, `src/text_editor.cpp`)
  - Public API facade.
  - Manages selection/cursor, caching, and connects document/layout/drawing.
  - Uses layout revision for cache validation.

- `TextEncoding` (`include/core/text_encoding.hpp`, `src/text_encoding.cpp`)
  - UTF-8/UTF-16 conversion via SkUnicode.

- `FontManager` (`include/core/font_manager.hpp`, `src/font_manager.cpp`)
  - Registers fonts and provides Skia FontCollection.

## Data Flow

1. Text/style mutations update `TextDocument` and increment the document version.
2. `TextEditor::layoutIfNeeded()` calls `TextLayout::update()` to:
   - Rebuild the paragraph if the document version changed.
   - Re-layout if only width changed.
3. Rendering uses cached selection/cursor rectangles; caches are invalidated on cursor/selection changes and when `layoutRevision` changes.
4. Platform bindings call into `TextEditor` and never touch layout or drawing directly.

## Invalidation Model

```
Document changes (text/styles)  →  Paragraph rebuild + Layout
Width changes                   →  Layout only (skip rebuild)
Cursor/selection changes        →  Cache invalidation only
```

`layoutRevision` increments on any layout change (rebuild or re-layout) and is the cache key for cursor/selection rects.

## Performance Characteristics

- Paragraph rebuild is the most expensive step. It is roughly proportional to:
  - text length (UTF-16 code units),
  - number of style runs,
  - shaping and line-breaking costs inside SkParagraph/Harfbuzz.
- Layout (`paragraph->layout(width)`) is also heavy and scales with text length and line-breaking. Width changes intentionally skip rebuild and only re-layout.
- Style run updates in `TextDocument` are linear in run count; merging runs performs a sort (`O(r log r)`) where `r` is the number of runs. This merge happens on every mutation (`insertAt`, `deleteRange`, `applyStyle`). For documents with many style runs, consider maintaining sorted order incrementally or using an interval tree.
- Selection rectangles (`getRectsForRange`) scale with the number of boxes returned (often per line). These are cached in `TextEditor` to avoid recomputation during cursor blink.
- Vertical cursor movement uses line metrics and hit-testing; cost grows with line count.

## Empty Text Behavior

- An empty document still builds a paragraph so hit testing and cursor placement work.
- Metrics (`height`, `width`, `lineCount`, intrinsic widths) return **0** when text length is zero.

## Layout-Dependent Operations

The following require a valid layout (they call `layoutIfNeeded()` in `TextEditor`):

- Rendering and layout metrics.
- Coordinate-based hit testing and selection.
- Word/line boundary queries.
- Vertical cursor movement and line-based navigation.

Horizontal cursor movement and plain text mutations do **not** require layout.

## Selection and Cursor Model

- Positions are UTF-16 code units.
- Selection is represented as anchor + focus.
- Cursor affinity tracks upstream/downstream placement for bidi edge cases.

## Platform Bindings

- Web: `platform/web/bindings.cpp` (Embind).
- iOS: `platform/ios/SkiaRenderer.mm` (ObjC++ bridge).

Both call the same `TextEditor` API, keeping behavior consistent across platforms.
