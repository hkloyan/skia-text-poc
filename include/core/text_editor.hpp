#pragma once

#include "core/types.hpp"
#include "core/text_document.hpp"
#include "core/text_layout.hpp"
#include "include/core/SkRect.h"
#include <optional>
#include <vector>

class SkCanvas;

namespace core {

/**
 * TextEditor - Public API facade that orchestrates Document, Layout, and Drawing.
 * 
 * This is the main entry point for text editing functionality. It provides:
 * - Text content management (via TextDocument)
 * - Layout computation and metrics (via TextLayout)
 * - Rendering (via TextDrawing)
 * - Cursor and selection management
 * - Rich text builder
 * 
 * Methods that need layout auto-update internally via layoutIfNeeded().
 */
class TextEditor {
public:
    TextEditor();
    ~TextEditor();
    
    // === Static factories ===
    static TextStyle makeStyle(
        const std::string& fontFamily, float fontSize, uint32_t color,
        int fontWeight, bool italic, bool underline,
        float letterSpacing, float wordSpacing,
        uint32_t backgroundColor, bool hasBackground,
        uint32_t shadowColor, float shadowOffsetX, float shadowOffsetY,
        float shadowBlurSigma, bool hasShadow);
    
    static StyledSpan makeSpan(
        const std::string& text,
        const std::string& fontFamily, float fontSize, uint32_t color,
        int fontWeight, bool italic, bool underline,
        float letterSpacing, float wordSpacing,
        uint32_t backgroundColor, bool hasBackground,
        uint32_t shadowColor, float shadowOffsetX, float shadowOffsetY,
        float shadowBlurSigma, bool hasShadow);
    
    // === Rich text builder ===
    void beginRichText();
    void addStyledSpan(const StyledSpan& span);
    void endRichText();
    
    // === Document methods ===
    void setText(const std::string& text, const TextStyle& style);
    void setRichText(const std::vector<StyledSpan>& spans);
    std::string getText() const;
    std::string getSelectedText() const;
    int getTextLength() const;
    
    // === Layout configuration ===
    void setMaxWidth(float width);
    void setTextAlignment(TextAlignment alignment);
    void setMaxLines(int maxLines);
    void setEllipsis(const std::string& ellipsis);
    void setLineHeight(float height);
    void setStrutStyle(const TextStrutStyle& strut);
    void clearStrutStyle();
    
    // === Layout control ===
    void layoutIfNeeded();
    
    // ============================================================
    // Methods below auto-update layout if needed (may rebuild paragraph)
    // For explicit control, call layoutIfNeeded() before these.
    // ============================================================
    
    // === Layout metrics (auto-update layout) ===
    float getHeight();
    float getWidth();
    float getMaxIntrinsicWidth();
    float getMinIntrinsicWidth();
    int getLineCount();
    
    // === Text editing ===
    void insertText(const std::string& text);
    void insertStyledText(const StyledSpan& span);
    void deleteBackward();
    void deleteForward();
    void deleteWordBackward();
    void deleteWordForward();
    void deleteSelection();
    void applyStyleToSelection(const TextStyle& style);
    TextStyle getStyleAtCursor() const;
    
    // === Cursor navigation ===
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
    
    // === Cursor state ===
    void setCursorPosition(int position);
    int getCursorPosition() const;
    void setCursorPositionAtCoordinate(float x, float y);
    
    // === Selection state ===
    void setSelection(int start, int end);
    void clearSelection();
    bool hasSelection() const;
    std::pair<int, int> getSelection() const;
    void beginSelectionAtCoordinate(float x, float y);
    void extendSelectionToCoordinate(float x, float y);
    void setWordSelectionAtCoordinate(float x, float y);
    void setLineSelectionAtCoordinate(float x, float y);
    
    // === Colors ===
    void setCursorColor(Color color);
    Color getCursorColor() const;
    void setSelectionColor(Color color);
    Color getSelectionColor() const;
    
    // === Scale ===
    void setScale(float scale);
    float getScale() const;
    
    // === Rendering (auto-update layout) ===
    void render(SkCanvas* canvas, float x, float y, bool showCursor = false);
    
    // === Queries (auto-update layout) ===
    std::optional<int> getGlyphPositionAtCoordinate(float x, float y);
    std::vector<SkRect> getRectsForRange(int start, int end);
    std::optional<std::pair<int, int>> getWordBoundary(int position);
    std::optional<std::pair<int, int>> getLineBoundary(int position);
    
private:
    void clampCursorPosition();
    void invalidateCursorCache();
    void invalidateSelectionCache();
    bool isCursorCacheValid() const;
    bool isSelectionCacheValid() const;
    SkRect computeCursorRect() const;
    std::vector<SkRect> computeSelectionRects() const;
    float getXPositionForCursor();
    
    TextDocument _document;
    TextLayout _layout;
    
    // Selection state
    int _selectionAnchor = 0;
    int _selectionFocus = 0;
    bool _cursorAffinityDownstream = true;
    
    // Render styling
    Color _cursorColor = Color::black();
    Color _selectionColor = Color::fromARGB(0x400000FF);
    float _scale = 1.0f;
    
    // Rich text builder
    std::vector<StyledSpan> _richTextBuilder;
    
    // Cursor rect cache (invalidated on cursor change OR layout change)
    mutable std::optional<SkRect> _cachedCursorRect;
    mutable int _cachedCursorPosition = -1;
    mutable bool _cachedCursorAffinity = true;
    mutable uint64_t _cachedCursorLayoutRevision = 0;
    
    // Selection rects cache (invalidated on selection change OR layout change)
    mutable std::optional<std::vector<SkRect>> _cachedSelectionRects;
    mutable int _cachedSelectionStart = -1;
    mutable int _cachedSelectionEnd = -1;
    mutable uint64_t _cachedSelectionLayoutRevision = 0;
};

} // namespace core
