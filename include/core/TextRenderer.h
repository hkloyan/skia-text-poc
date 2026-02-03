#pragma once

#include "include/core/SkCanvas.h"
#include "include/core/SkRect.h"
#include "modules/skparagraph/include/Paragraph.h"
#include <string>
#include <memory>
#include <vector>
#include <utility>
#include <optional>
#include <cstdint>

namespace core {

enum class TextAlignment {
    Left = 0,
    Right = 1,
    Center = 2,
    Justify = 3,
    Start = 4,
    End = 5
};

// Type-safe color representation (ARGB format)
struct Color {
    uint8_t a = 255;
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    
    constexpr Color() = default;
    constexpr Color(uint8_t a, uint8_t r, uint8_t g, uint8_t b) : a(a), r(r), g(g), b(b) {}
    
    // Create from ARGB uint32_t (0xAARRGGBB)
    static constexpr Color fromARGB(uint32_t argb) {
        return Color(
            static_cast<uint8_t>((argb >> 24) & 0xFF),
            static_cast<uint8_t>((argb >> 16) & 0xFF),
            static_cast<uint8_t>((argb >> 8) & 0xFF),
            static_cast<uint8_t>(argb & 0xFF)
        );
    }
    
    // Create from RGB with full alpha
    static constexpr Color fromRGB(uint8_t r, uint8_t g, uint8_t b) {
        return Color(255, r, g, b);
    }
    
    // Convert to ARGB uint32_t
    constexpr uint32_t toARGB() const {
        return (static_cast<uint32_t>(a) << 24) |
               (static_cast<uint32_t>(r) << 16) |
               (static_cast<uint32_t>(g) << 8) |
               static_cast<uint32_t>(b);
    }
    
    // Common colors
    static constexpr Color black() { return Color(255, 0, 0, 0); }
    static constexpr Color white() { return Color(255, 255, 255, 255); }
    static constexpr Color transparent() { return Color(0, 0, 0, 0); }
    static constexpr Color red() { return Color(255, 255, 0, 0); }
    static constexpr Color green() { return Color(255, 0, 255, 0); }
    static constexpr Color blue() { return Color(255, 0, 0, 255); }
    
    constexpr bool operator==(const Color& other) const {
        return a == other.a && r == other.r && g == other.g && b == other.b;
    }
    constexpr bool operator!=(const Color& other) const { return !(*this == other); }
};

struct TextShadowStyle {
    Color color = Color::black();
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float blurSigma = 0.0f;
    
    bool operator==(const TextShadowStyle& other) const {
        return color == other.color &&
               offsetX == other.offsetX &&
               offsetY == other.offsetY &&
               blurSigma == other.blurSigma;
    }
};

struct TextStrutStyle {
    bool enabled = false;
    std::string fontFamily = "Roboto";
    float fontSize = 0.0f;
    float height = 0.0f;
    float leading = 0.0f;
    bool forceHeight = false;
    bool heightOverride = false;
    bool halfLeading = false;
};

struct TextStyle;

// Builder for TextStyle - enables fluent configuration
class TextStyleBuilder {
public:
    TextStyleBuilder();
    explicit TextStyleBuilder(const TextStyle& base);
    
    TextStyleBuilder& fontFamily(const std::string& family);
    TextStyleBuilder& fontSize(float size);
    TextStyleBuilder& color(Color c);
    TextStyleBuilder& fontWeight(int weight);
    TextStyleBuilder& italic(bool value = true);
    TextStyleBuilder& underline(bool value = true);
    TextStyleBuilder& letterSpacing(float spacing);
    TextStyleBuilder& wordSpacing(float spacing);
    TextStyleBuilder& background(Color c);
    TextStyleBuilder& noBackground();
    TextStyleBuilder& shadow(Color c, float offsetX, float offsetY, float blurSigma);
    TextStyleBuilder& noShadow();
    
    TextStyle build() const;
    
private:
    std::string fontFamily_ = "Roboto";
    float fontSize_ = 16.0f;
    Color color_ = Color::black();
    int fontWeight_ = 400;
    bool italic_ = false;
    bool underline_ = false;
    float letterSpacing_ = 0.0f;
    float wordSpacing_ = 0.0f;
    bool hasBackground_ = false;
    Color backgroundColor_ = Color::transparent();
    bool hasShadow_ = false;
    TextShadowStyle shadow_;
};

struct TextStyle {
    std::string fontFamily = "Roboto";
    float fontSize = 16.0f;
    Color color = Color::black();
    int fontWeight = 400;         // 400 = normal, 700 = bold
    bool italic = false;
    bool underline = false;
    float letterSpacing = 0.0f;
    float wordSpacing = 0.0f;
    bool hasBackground = false;
    Color backgroundColor = Color::transparent();
    bool hasShadow = false;
    TextShadowStyle shadow;
    
    bool operator==(const TextStyle& other) const {
        return fontFamily == other.fontFamily &&
               fontSize == other.fontSize &&
               color == other.color &&
               fontWeight == other.fontWeight &&
               italic == other.italic &&
               underline == other.underline &&
               letterSpacing == other.letterSpacing &&
               wordSpacing == other.wordSpacing &&
               hasBackground == other.hasBackground &&
               backgroundColor == other.backgroundColor &&
               hasShadow == other.hasShadow &&
               shadow == other.shadow;
    }
    
    // Start building from this style
    TextStyleBuilder toBuilder() const { return TextStyleBuilder(*this); }
    
    // Static builder factory
    static TextStyleBuilder builder() { return TextStyleBuilder(); }
};

struct StyledSpan {
    std::string text;
    TextStyle style;
};

// Internal representation for editable rich text
struct StyleRun {
    int start;      // inclusive, in UTF-16 code units
    int end;        // exclusive, in UTF-16 code units
    TextStyle style;
};

class TextRenderer {
public:
    TextRenderer();
    ~TextRenderer();
    
    // Set the text content with styling
    void setText(const std::string& text, const TextStyle& style);
    
    // Set rich text with multiple styled spans
    void setRichText(const std::vector<StyledSpan>& spans);

    // Paragraph-level layout configuration (applies to all text)
    void setTextAlignment(TextAlignment alignment);
    void setMaxLines(int maxLines);
    void setEllipsis(const std::string& ellipsis);
    void setLineHeight(float height);
    void setStrutStyle(const TextStrutStyle& strutStyle);
    void clearStrutStyle();

    // Update layout width (layout happens lazily)
    void setMaxWidth(float maxWidth);
    void layoutIfNeeded() const;
    
    // Render the text to a canvas at the specified position
    // showCursor: whether to draw the cursor (for blink animation)
    void render(SkCanvas* canvas, float x, float y, bool showCursor = false) const;
    
    // Get layout metrics
    float getHeight() const;
    float getWidth() const;
    float getMaxIntrinsicWidth() const;
    float getMinIntrinsicWidth() const;
    int getLineCount() const;
    int getTextLength() const;  // UTF-16 code units
    
    // === Text Content ===
    std::string getText() const;
    std::string getSelectedText() const;
    
    // === Text Editing ===
    void insertText(const std::string& text);       // Insert at cursor, replaces selection (inherits style from left)
    void insertStyledText(const StyledSpan& span);  // Insert at cursor with explicit style
    void deleteBackward();                          // Backspace
    void deleteForward();                           // Delete key
    void deleteWordBackward();                      // Opt/Ctrl+Backspace
    void deleteWordForward();                       // Opt/Ctrl+Delete
    void deleteSelection();                         // Delete selected text
    
    // === Text Styling ===
    void applyStyleToSelection(const TextStyle& style);  // Apply style to selected text
    TextStyle getStyleAtCursor() const;                  // Get style at cursor position (for UI state)
    
    // === Cursor Navigation ===
    void moveCursorLeft(bool extendSelection = false);
    void moveCursorRight(bool extendSelection = false);
    void moveCursorUp(bool extendSelection = false);
    void moveCursorDown(bool extendSelection = false);
    void moveCursorToWordStart(bool extendSelection = false);
    void moveCursorToWordEnd(bool extendSelection = false);
    void moveCursorToLineStart(bool extendSelection = false);
    void moveCursorToLineEnd(bool extendSelection = false);
    void moveCursorToDocumentStart(bool extendSelection = false);
    void moveCursorToDocumentEnd(bool extendSelection = false);
    void selectAll();
    
    // === Cursor ===
    // Indices are UTF-16 code units.
    void setCursorPosition(int position);
    int getCursorPosition() const;
    void setCursorPositionAtCoordinate(float x, float y);
    
    // === Selection ===
    // Selection is defined by anchor (start of drag) and focus (current position/cursor)
    // When anchor == focus, there's no highlight, just a cursor
    void setSelection(int start, int end);
    void clearSelection();
    bool hasSelection() const;
    std::pair<int, int> getSelection() const;  // returns {min, max} of anchor/focus
    
    // Coordinate-based selection (convenience methods)
    void setWordSelectionAtCoordinate(float x, float y);   // double-click
    void setLineSelectionAtCoordinate(float x, float y);   // triple-click
    
    // Drag selection
    void beginSelectionAtCoordinate(float x, float y);     // touch/mouse down
    void extendSelectionToCoordinate(float x, float y);    // touch/mouse move
    
    // === Colors ===
    void setCursorColor(Color color);
    Color getCursorColor() const;
    void setSelectionColor(Color color);
    Color getSelectionColor() const;
    
    // Scale factor for high-DPI rendering
    // When set, applies a canvas transform so all coordinates are in logical pixels
    void setScale(float scale);
    float getScale() const;
    
    // === Query methods (for advanced use) ===
    // Returns nullopt if position cannot be determined (e.g., no paragraph)
    std::optional<int> getGlyphPositionAtCoordinate(float x, float y) const;
    std::vector<SkRect> getRectsForRange(int start, int end) const;
    // Returns nullopt if boundary cannot be determined
    std::optional<std::pair<int, int>> getWordBoundary(int position) const;
    std::optional<std::pair<int, int>> getLineBoundary(int position) const;

private:
    void rebuildParagraph() const;
    void rebuildParagraphIfNeeded() const;
    void deleteRange(int start, int end);
    void insertTextAt(const std::u16string& text, int position);
    void adjustStyleRunsForInsert(int position, int length);
    void adjustStyleRunsForDelete(int start, int end);
    TextStyle getStyleAtPosition(int position) const;
    bool getGraphemeClusterRangeAt(int position, int* start, int* end) const;
    void mergeAdjacentStyleRuns();
    void clampCursorPosition();
    int getLineIndexForPosition(int position) const;
    float getXPositionForCursor() const;
    bool getGlyphPositionWithAffinityAtCoordinate(float x, float y, int* position, bool* downstream) const;
    
    void drawSelection(SkCanvas* canvas, float x, float y) const;
    void drawCursor(SkCanvas* canvas, float x, float y) const;
    SkRect getCursorRect() const;
    void invalidateCursorCache() const;
    void invalidateSelectionCache() const;
    float getDefaultCursorHeight() const;
    
    mutable std::unique_ptr<skia::textlayout::Paragraph> paragraph_;
    float maxWidth_ = 0;
    mutable bool needsLayout_ = false;
    mutable bool needsRebuildParagraph_ = false;
    
    // Internal text storage for editing
    std::u16string text_;
    std::vector<StyleRun> styleRuns_;
    TextStyle defaultStyle_;
    TextAlignment textAlignment_ = TextAlignment::Left;
    int maxLines_ = 0;  // <= 0 means unlimited
    std::u16string ellipsis_;
    float lineHeight_ = 0.0f;  // <= 0 means unset
    TextStrutStyle strutStyle_;
    
    // Cursor/Selection state
    int selectionAnchor_ = 0;  // where selection started
    int selectionFocus_ = 0;   // where cursor is (end of selection)
    mutable bool cursorAffinityDownstream_ = true;
    
    // Colors
    Color cursorColor_ = Color::black();
    Color selectionColor_ = Color::fromARGB(0x400000FF);  // semi-transparent blue
    
    // Scale factor for high-DPI displays (applies canvas transform in render)
    float scale_ = 1.0f;
    
    // Cursor dimensions
    static constexpr float kCursorWidth = 2.0f;
    
    // Cached cursor rect (invalidated on cursor position change)
    mutable std::optional<SkRect> cachedCursorRect_;
    mutable int cachedCursorPosition_ = -1;
    mutable bool cachedCursorAffinity_ = true;
    
    // Cached selection rects (invalidated on selection change)
    mutable std::optional<std::vector<SkRect>> cachedSelectionRects_;
    mutable int cachedSelectionStart_ = -1;
    mutable int cachedSelectionEnd_ = -1;
};

} // namespace core
