#pragma once

#include "core/types.hpp"
#include "include/core/SkRect.h"

class SkCanvas;

namespace core {

class TextLayout;

/**
 * TextDrawing - Stateless drawing functions for text, selection, and cursor.
 * 
 * This class provides static methods to draw text-related elements.
 * All state (layout, cursor position, selection) is passed as parameters.
 */
class TextDrawing {
public:
    struct CursorStyle {
        Color color = Color::black();
        float width = 2.0f;
    };
    
    struct SelectionStyle {
        Color color = Color::fromARGB(0x400000FF);  // semi-transparent blue
    };
    
    // Individual drawing functions
    static void drawText(
        SkCanvas* canvas,
        const TextLayout& layout,
        float x, float y);
    
    static void drawSelection(
        SkCanvas* canvas,
        const TextLayout& layout,
        float x, float y,
        int start, int end,
        const SelectionStyle& style);
    
    static void drawCursor(
        SkCanvas* canvas,
        float x, float y,
        const SkRect& cursorRect,
        const CursorStyle& style);
    
    // Cursor rect computation - public for TextEditor's cursor cache
    static SkRect computeCursorRect(
        const TextLayout& layout,
        const std::u16string& text,
        int position, bool downstream,
        float cursorWidth,
        float defaultFontSize);
    
    // Default cursor width constant
    static constexpr float kDefaultCursorWidth = 2.0f;
};

} // namespace core
