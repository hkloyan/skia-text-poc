#include "core/text_editor.hpp"
#include "core/text_drawing.hpp"
#include "core/text_encoding.hpp"
#include "modules/skparagraph/include/Paragraph.h"
#include "include/core/SkCanvas.h"
#include <algorithm>

using namespace skia::textlayout;

namespace core {

TextEditor::TextEditor() = default;
TextEditor::~TextEditor() = default;

// === Static factories ===

TextStyle TextEditor::makeStyle(
    const std::string& fontFamily, float fontSize, uint32_t color,
    int fontWeight, bool italic, bool underline,
    float letterSpacing, float wordSpacing,
    uint32_t backgroundColor, bool hasBackground,
    uint32_t shadowColor, float shadowOffsetX, float shadowOffsetY,
    float shadowBlurSigma, bool hasShadow)
{
    TextStyle style;
    style.fontFamily = fontFamily;
    style.fontSize = fontSize;
    style.color = Color::fromARGB(color);
    style.fontWeight = fontWeight;
    style.italic = italic;
    style.underline = underline;
    style.letterSpacing = letterSpacing;
    style.wordSpacing = wordSpacing;
    style.hasBackground = hasBackground;
    style.backgroundColor = Color::fromARGB(backgroundColor);
    style.hasShadow = hasShadow;
    style.shadow.color = Color::fromARGB(shadowColor);
    style.shadow.offsetX = shadowOffsetX;
    style.shadow.offsetY = shadowOffsetY;
    style.shadow.blurSigma = shadowBlurSigma;
    return style;
}

StyledSpan TextEditor::makeSpan(
    const std::string& text,
    const std::string& fontFamily, float fontSize, uint32_t color,
    int fontWeight, bool italic, bool underline,
    float letterSpacing, float wordSpacing,
    uint32_t backgroundColor, bool hasBackground,
    uint32_t shadowColor, float shadowOffsetX, float shadowOffsetY,
    float shadowBlurSigma, bool hasShadow)
{
    StyledSpan span;
    span.text = text;
    span.style = makeStyle(fontFamily, fontSize, color, fontWeight, italic, underline,
                          letterSpacing, wordSpacing, backgroundColor, hasBackground,
                          shadowColor, shadowOffsetX, shadowOffsetY, shadowBlurSigma, hasShadow);
    return span;
}

// === Rich text builder ===

void TextEditor::beginRichText() {
    _richTextBuilder.clear();
}

void TextEditor::addStyledSpan(const StyledSpan& span) {
    _richTextBuilder.push_back(span);
}

void TextEditor::endRichText() {
    setRichText(_richTextBuilder);
    _richTextBuilder.clear();
}

// === Document methods ===

void TextEditor::setText(const std::string& text, const TextStyle& style) {
    _document.setText(text, style);
    _selectionAnchor = _selectionFocus = 0;
    _cursorAffinityDownstream = true;
    invalidateCursorCache();
    invalidateSelectionCache();
}

void TextEditor::setRichText(const std::vector<StyledSpan>& spans) {
    _document.setRichText(spans);
    clampCursorPosition();
    invalidateCursorCache();
    invalidateSelectionCache();
}

std::string TextEditor::getText() const {
    return _document.getText();
}

std::string TextEditor::getSelectedText() const {
    if (!hasSelection()) return "";
    auto [start, end] = getSelection();
    return _document.getTextInRange(start, end);
}

int TextEditor::getTextLength() const {
    return _document.length();
}

// === Layout configuration ===

void TextEditor::setMaxWidth(float width) {
    _layout.setMaxWidth(width);
    invalidateCursorCache();
    invalidateSelectionCache();
}

void TextEditor::setTextAlignment(TextAlignment alignment) {
    _layout.setTextAlignment(alignment);
}

void TextEditor::setMaxLines(int maxLines) {
    _layout.setMaxLines(maxLines);
}

void TextEditor::setEllipsis(const std::string& ellipsis) {
    _layout.setEllipsis(ellipsis);
}

void TextEditor::setLineHeight(float height) {
    _layout.setLineHeight(height);
}

void TextEditor::setStrutStyle(const TextStrutStyle& strut) {
    _layout.setStrutStyle(strut);
}

void TextEditor::clearStrutStyle() {
    _layout.clearStrutStyle();
}

// === Layout control ===

void TextEditor::layoutIfNeeded() {
    _layout.update(_document);
}

// === Layout metrics ===

float TextEditor::getHeight() {
    layoutIfNeeded();
    return _layout.getHeight();
}

float TextEditor::getWidth() {
    layoutIfNeeded();
    return _layout.getWidth();
}

float TextEditor::getMaxIntrinsicWidth() {
    layoutIfNeeded();
    return _layout.getMaxIntrinsicWidth();
}

float TextEditor::getMinIntrinsicWidth() {
    layoutIfNeeded();
    return _layout.getMinIntrinsicWidth();
}

int TextEditor::getLineCount() {
    layoutIfNeeded();
    return _layout.getLineCount();
}

// === Text editing ===

void TextEditor::insertText(const std::string& text) {
    std::u16string text16 = TextEncoding::toUtf16(text);
    if (text16.empty()) return;
    
    // Delete selection if any
    if (hasSelection()) {
        deleteSelection();
    }
    
    _document.insertAt(_selectionFocus, text16);
    
    // Move cursor to end of inserted text
    _selectionAnchor = _selectionFocus = _selectionFocus + static_cast<int>(text16.length());
    clampCursorPosition();
    invalidateCursorCache();
}

void TextEditor::insertStyledText(const StyledSpan& span) {
    std::u16string spanText = TextEncoding::toUtf16(span.text);
    if (spanText.empty()) return;
    
    // Delete selection if any
    if (hasSelection()) {
        deleteSelection();
    }
    
    int position = _selectionFocus;
    int len = static_cast<int>(spanText.length());
    
    // Insert the text
    _document.insertAt(position, spanText);
    
    // Apply the style to the inserted range
    _document.applyStyle(position, position + len, span.style);
    
    // Move cursor to end of inserted text
    _selectionAnchor = _selectionFocus = position + len;
    clampCursorPosition();
    invalidateCursorCache();
}

void TextEditor::deleteBackward() {
    if (hasSelection()) {
        deleteSelection();
        return;
    }
    
    if (_selectionFocus <= 0) return;
    
    layoutIfNeeded();
    
    // Delete the grapheme cluster before the cursor
    int deleteStart = _selectionFocus - 1;
    int deleteEnd = _selectionFocus;
    if (_layout.getGraphemeClusterRangeAt(_selectionFocus - 1, &deleteStart, &deleteEnd)) {
        _document.deleteRange(deleteStart, deleteEnd);
        _selectionAnchor = _selectionFocus = deleteStart;
    } else {
        _document.deleteRange(deleteStart, _selectionFocus);
        _selectionAnchor = _selectionFocus = deleteStart;
    }
    clampCursorPosition();
    invalidateCursorCache();
}

void TextEditor::deleteForward() {
    if (hasSelection()) {
        deleteSelection();
        return;
    }
    
    if (_selectionFocus >= _document.length()) return;
    
    layoutIfNeeded();
    
    // Delete the grapheme cluster after the cursor
    int deleteStart = _selectionFocus;
    int deleteEnd = _selectionFocus + 1;
    if (_layout.getGraphemeClusterRangeAt(_selectionFocus, &deleteStart, &deleteEnd)) {
        _document.deleteRange(deleteStart, deleteEnd);
        _selectionAnchor = _selectionFocus = deleteStart;
    } else {
        _document.deleteRange(_selectionFocus, _selectionFocus + 1);
    }
    clampCursorPosition();
    invalidateCursorCache();
}

void TextEditor::deleteWordBackward() {
    if (hasSelection()) {
        deleteSelection();
        return;
    }
    
    if (_selectionFocus <= 0) return;
    
    layoutIfNeeded();
    
    // Find word boundary before cursor
    auto boundary = _layout.getWordBoundary(_selectionFocus - 1);
    int deleteStart = boundary ? boundary->first : 0;
    
    _document.deleteRange(deleteStart, _selectionFocus);
    _selectionAnchor = _selectionFocus = deleteStart;
    clampCursorPosition();
    invalidateCursorCache();
}

void TextEditor::deleteWordForward() {
    if (hasSelection()) {
        deleteSelection();
        return;
    }
    
    if (_selectionFocus >= _document.length()) return;
    
    layoutIfNeeded();
    
    // Find word boundary after cursor
    auto boundary = _layout.getWordBoundary(_selectionFocus);
    int wordEnd = boundary ? boundary->second : _document.length();
    
    _document.deleteRange(_selectionFocus, wordEnd);
    clampCursorPosition();
    invalidateCursorCache();
}

void TextEditor::deleteSelection() {
    if (!hasSelection()) return;
    
    auto [start, end] = getSelection();
    _document.deleteRange(start, end);
    _selectionAnchor = _selectionFocus = start;
    clampCursorPosition();
    invalidateCursorCache();
    invalidateSelectionCache();
}

void TextEditor::applyStyleToSelection(const TextStyle& style) {
    if (!hasSelection()) return;
    
    auto [start, end] = getSelection();
    _document.applyStyle(start, end, style);
}

TextStyle TextEditor::getStyleAtCursor() const {
    // When cursor is at a boundary, prefer the style to the left
    int position = _selectionFocus;
    if (position > 0) {
        return _document.getStyleAt(position - 1);
    }
    return _document.getStyleAt(position);
}

// === Cursor navigation ===

void TextEditor::moveCursorLeft(bool extendSelection) {
    if (!extendSelection && hasSelection()) {
        // Collapse selection to start
        auto [start, end] = getSelection();
        _selectionAnchor = _selectionFocus = start;
        invalidateCursorCache();
        invalidateSelectionCache();
        return;
    }
    
    if (_selectionFocus > 0) {
        layoutIfNeeded();
        int clusterStart = _selectionFocus - 1;
        int clusterEnd = _selectionFocus;
        if (_layout.getGraphemeClusterRangeAt(_selectionFocus - 1, &clusterStart, &clusterEnd)) {
            _selectionFocus = clusterStart;
        } else {
            _selectionFocus--;
        }
        if (!extendSelection) {
            _selectionAnchor = _selectionFocus;
        }
        invalidateCursorCache();
        if (extendSelection) invalidateSelectionCache();
    }
}

void TextEditor::moveCursorRight(bool extendSelection) {
    if (!extendSelection && hasSelection()) {
        // Collapse selection to end
        auto [start, end] = getSelection();
        _selectionAnchor = _selectionFocus = end;
        invalidateCursorCache();
        invalidateSelectionCache();
        return;
    }
    
    if (_selectionFocus < _document.length()) {
        layoutIfNeeded();
        int clusterStart = _selectionFocus;
        int clusterEnd = _selectionFocus + 1;
        if (_layout.getGraphemeClusterRangeAt(_selectionFocus, &clusterStart, &clusterEnd)) {
            _selectionFocus = clusterEnd;
        } else {
            _selectionFocus++;
        }
        if (!extendSelection) {
            _selectionAnchor = _selectionFocus;
        }
        invalidateCursorCache();
        if (extendSelection) invalidateSelectionCache();
    }
}

void TextEditor::moveCursorUp(bool extendSelection) {
    layoutIfNeeded();
    
    // Get current line
    int currentLine = _layout.getLineIndexForPosition(_selectionFocus);
    if (currentLine <= 0) {
        // Already on first line, move to start
        _selectionFocus = 0;
    } else {
        // Move to same x position on previous line
        auto* paragraph = _layout.paragraph();
        if (!paragraph) return;
        
        std::vector<LineMetrics> lineMetrics;
        paragraph->getLineMetrics(lineMetrics);
        
        float currentX = getXPositionForCursor();
        
        // Find position at same X on previous line
        float prevLineY = lineMetrics[currentLine - 1].fBaseline - lineMetrics[currentLine - 1].fAscent / 2;
        int pos = 0;
        bool downstream = true;
        if (_layout.getPositionWithAffinityAtCoordinate(currentX, prevLineY, &pos, &downstream)) {
            _selectionFocus = pos;
            _cursorAffinityDownstream = downstream;
        }
    }
    
    if (!extendSelection) {
        _selectionAnchor = _selectionFocus;
    }
    clampCursorPosition();
    invalidateCursorCache();
    if (extendSelection) invalidateSelectionCache();
}

void TextEditor::moveCursorDown(bool extendSelection) {
    layoutIfNeeded();
    
    auto* paragraph = _layout.paragraph();
    if (!paragraph) return;
    
    std::vector<LineMetrics> lineMetrics;
    paragraph->getLineMetrics(lineMetrics);
    
    int currentLine = _layout.getLineIndexForPosition(_selectionFocus);
    if (currentLine >= static_cast<int>(lineMetrics.size()) - 1) {
        // Already on last line, move to end
        _selectionFocus = _document.length();
    } else {
        // Move to same x position on next line
        float currentX = getXPositionForCursor();
        
        // Find position at same X on next line
        float nextLineY = lineMetrics[currentLine + 1].fBaseline - lineMetrics[currentLine + 1].fAscent / 2;
        int pos = 0;
        bool downstream = true;
        if (_layout.getPositionWithAffinityAtCoordinate(currentX, nextLineY, &pos, &downstream)) {
            _selectionFocus = pos;
            _cursorAffinityDownstream = downstream;
        }
    }
    
    if (!extendSelection) {
        _selectionAnchor = _selectionFocus;
    }
    clampCursorPosition();
    invalidateCursorCache();
    if (extendSelection) invalidateSelectionCache();
}

void TextEditor::moveCursorToWordStart(bool extendSelection) {
    if (_selectionFocus <= 0) return;
    
    layoutIfNeeded();
    auto boundary = _layout.getWordBoundary(_selectionFocus - 1);
    if (boundary) {
        _selectionFocus = boundary->first;
    }
    
    if (!extendSelection) {
        _selectionAnchor = _selectionFocus;
    }
    invalidateCursorCache();
    if (extendSelection) invalidateSelectionCache();
}

void TextEditor::moveCursorToWordEnd(bool extendSelection) {
    if (_selectionFocus >= _document.length()) return;
    
    layoutIfNeeded();
    auto boundary = _layout.getWordBoundary(_selectionFocus);
    if (boundary) {
        _selectionFocus = boundary->second;
    }
    
    if (!extendSelection) {
        _selectionAnchor = _selectionFocus;
    }
    invalidateCursorCache();
    if (extendSelection) invalidateSelectionCache();
}

void TextEditor::moveCursorToLineStart(bool extendSelection) {
    layoutIfNeeded();
    auto boundary = _layout.getLineBoundary(_selectionFocus);
    if (boundary) {
        _selectionFocus = boundary->first;
    }
    
    if (!extendSelection) {
        _selectionAnchor = _selectionFocus;
    }
    invalidateCursorCache();
    if (extendSelection) invalidateSelectionCache();
}

void TextEditor::moveCursorToLineEnd(bool extendSelection) {
    layoutIfNeeded();
    auto boundary = _layout.getLineBoundary(_selectionFocus);
    if (boundary) {
        int end = boundary->second;
        // Don't include the newline character if there is one
        const auto& text = _document.text();
        if (end > boundary->first && end <= static_cast<int>(text.length()) && 
            text[end - 1] == u'\n') {
            end--;
        }
        _selectionFocus = end;
    }
    
    if (!extendSelection) {
        _selectionAnchor = _selectionFocus;
    }
    invalidateCursorCache();
    if (extendSelection) invalidateSelectionCache();
}

void TextEditor::moveCursorToDocumentStart(bool extendSelection) {
    _selectionFocus = 0;
    
    if (!extendSelection) {
        _selectionAnchor = _selectionFocus;
    }
    invalidateCursorCache();
    if (extendSelection) invalidateSelectionCache();
}

void TextEditor::moveCursorToDocumentEnd(bool extendSelection) {
    _selectionFocus = _document.length();
    
    if (!extendSelection) {
        _selectionAnchor = _selectionFocus;
    }
    invalidateCursorCache();
    if (extendSelection) invalidateSelectionCache();
}

void TextEditor::selectAll() {
    _selectionAnchor = 0;
    _selectionFocus = _document.length();
    invalidateCursorCache();
    invalidateSelectionCache();
}

// === Cursor state ===

void TextEditor::setCursorPosition(int position) {
    _selectionAnchor = position;
    _selectionFocus = position;
    clampCursorPosition();
    _cursorAffinityDownstream = true;
    invalidateCursorCache();
    invalidateSelectionCache();
}

int TextEditor::getCursorPosition() const {
    return _selectionFocus;
}

void TextEditor::setCursorPositionAtCoordinate(float x, float y) {
    layoutIfNeeded();
    int pos = 0;
    bool downstream = true;
    if (_layout.getPositionWithAffinityAtCoordinate(x, y, &pos, &downstream)) {
        _selectionAnchor = pos;
        _selectionFocus = pos;
        _cursorAffinityDownstream = downstream;
        clampCursorPosition();
        invalidateCursorCache();
        invalidateSelectionCache();
    }
}

// === Selection state ===

void TextEditor::setSelection(int start, int end) {
    _selectionAnchor = start;
    _selectionFocus = end;
    clampCursorPosition();
    _cursorAffinityDownstream = true;
    invalidateCursorCache();
    invalidateSelectionCache();
}

void TextEditor::clearSelection() {
    _selectionAnchor = _selectionFocus;
    invalidateSelectionCache();
}

bool TextEditor::hasSelection() const {
    return _selectionAnchor != _selectionFocus;
}

std::pair<int, int> TextEditor::getSelection() const {
    return {std::min(_selectionAnchor, _selectionFocus),
            std::max(_selectionAnchor, _selectionFocus)};
}

void TextEditor::beginSelectionAtCoordinate(float x, float y) {
    layoutIfNeeded();
    int pos = 0;
    bool downstream = true;
    if (_layout.getPositionWithAffinityAtCoordinate(x, y, &pos, &downstream)) {
        _selectionAnchor = pos;
        _selectionFocus = pos;
        _cursorAffinityDownstream = downstream;
        invalidateCursorCache();
        invalidateSelectionCache();
    }
}

void TextEditor::extendSelectionToCoordinate(float x, float y) {
    layoutIfNeeded();
    int pos = 0;
    bool downstream = true;
    if (_layout.getPositionWithAffinityAtCoordinate(x, y, &pos, &downstream)) {
        _selectionFocus = pos;
        _cursorAffinityDownstream = downstream;
        // Anchor stays fixed, only focus moves
        invalidateCursorCache();
        invalidateSelectionCache();
    }
}

void TextEditor::setWordSelectionAtCoordinate(float x, float y) {
    layoutIfNeeded();
    auto pos = _layout.getPositionAtCoordinate(x, y);
    if (pos) {
        auto boundary = _layout.getWordBoundary(*pos);
        if (boundary) {
            setSelection(boundary->first, boundary->second);
        }
    }
}

void TextEditor::setLineSelectionAtCoordinate(float x, float y) {
    layoutIfNeeded();
    auto pos = _layout.getPositionAtCoordinate(x, y);
    if (pos) {
        auto boundary = _layout.getLineBoundary(*pos);
        if (boundary) {
            setSelection(boundary->first, boundary->second);
        }
    }
}

// === Colors ===

void TextEditor::setCursorColor(Color color) {
    _cursorColor = color;
}

Color TextEditor::getCursorColor() const {
    return _cursorColor;
}

void TextEditor::setSelectionColor(Color color) {
    _selectionColor = color;
}

Color TextEditor::getSelectionColor() const {
    return _selectionColor;
}

// === Scale ===

void TextEditor::setScale(float scale) {
    _scale = scale;
}

float TextEditor::getScale() const {
    return _scale;
}

// === Rendering ===

void TextEditor::render(SkCanvas* canvas, float x, float y, bool showCursor) {
    layoutIfNeeded();
    if (!_layout.paragraph()) return;
    
    // Apply scale transform for high-DPI rendering
    canvas->save();
    if (_scale != 1.0f) {
        canvas->scale(_scale, _scale);
    }
    
    // 1. Draw selection highlight (behind text)
    if (hasSelection()) {
        auto [start, end] = getSelection();
        
        // Check selection cache
        if (!isSelectionCacheValid()) {
            _cachedSelectionRects = _layout.getRectsForRange(start, end);
            _cachedSelectionStart = start;
            _cachedSelectionEnd = end;
            _cachedSelectionLayoutRevision = _layout.layoutRevision();
        }
        
        SkPaint paint;
        paint.setColor(_selectionColor.toARGB());
        paint.setStyle(SkPaint::kFill_Style);
        
        for (const auto& rect : *_cachedSelectionRects) {
            SkRect offsetRect = rect.makeOffset(x, y);
            canvas->drawRect(offsetRect, paint);
        }
    }
    
    // 2. Draw text
    TextDrawing::drawText(canvas, _layout, x, y);
    
    // 3. Draw cursor (on top of text)
    if (showCursor && !hasSelection()) {
        // Check cursor cache
        if (!isCursorCacheValid()) {
            _cachedCursorRect = computeCursorRect();
            _cachedCursorPosition = _selectionFocus;
            _cachedCursorAffinity = _cursorAffinityDownstream;
            _cachedCursorLayoutRevision = _layout.layoutRevision();
        }
        
        TextDrawing::CursorStyle curStyle;
        curStyle.color = _cursorColor;
        curStyle.width = TextDrawing::kDefaultCursorWidth;
        
        TextDrawing::drawCursor(canvas, x, y, *_cachedCursorRect, curStyle);
    }
    
    canvas->restore();
}

// === Queries ===

std::optional<int> TextEditor::getGlyphPositionAtCoordinate(float x, float y) {
    layoutIfNeeded();
    return _layout.getPositionAtCoordinate(x, y);
}

std::vector<SkRect> TextEditor::getRectsForRange(int start, int end) {
    layoutIfNeeded();
    return _layout.getRectsForRange(start, end);
}

std::optional<std::pair<int, int>> TextEditor::getWordBoundary(int position) {
    layoutIfNeeded();
    return _layout.getWordBoundary(position);
}

std::optional<std::pair<int, int>> TextEditor::getLineBoundary(int position) {
    layoutIfNeeded();
    return _layout.getLineBoundary(position);
}

// === Private helpers ===

void TextEditor::clampCursorPosition() {
    int textLen = _document.length();
    _selectionAnchor = std::clamp(_selectionAnchor, 0, textLen);
    _selectionFocus = std::clamp(_selectionFocus, 0, textLen);
}

void TextEditor::invalidateCursorCache() {
    _cachedCursorRect.reset();
}

void TextEditor::invalidateSelectionCache() {
    _cachedSelectionRects.reset();
}

bool TextEditor::isCursorCacheValid() const {
    return _cachedCursorRect &&
           _cachedCursorPosition == _selectionFocus &&
           _cachedCursorAffinity == _cursorAffinityDownstream &&
           _cachedCursorLayoutRevision == _layout.layoutRevision();
}

bool TextEditor::isSelectionCacheValid() const {
    auto [start, end] = getSelection();
    return _cachedSelectionRects &&
           _cachedSelectionStart == start &&
           _cachedSelectionEnd == end &&
           _cachedSelectionLayoutRevision == _layout.layoutRevision();
}

SkRect TextEditor::computeCursorRect() const {
    return TextDrawing::computeCursorRect(
        _layout,
        _document.text(),
        _selectionFocus,
        _cursorAffinityDownstream,
        TextDrawing::kDefaultCursorWidth,
        _document.defaultStyle().fontSize
    );
}

std::vector<SkRect> TextEditor::computeSelectionRects() const {
    auto [start, end] = getSelection();
    return _layout.getRectsForRange(start, end);
}

float TextEditor::getXPositionForCursor() {
    if (!isCursorCacheValid()) {
        _cachedCursorRect = computeCursorRect();
        _cachedCursorPosition = _selectionFocus;
        _cachedCursorAffinity = _cursorAffinityDownstream;
        _cachedCursorLayoutRevision = _layout.layoutRevision();
    }
    return _cachedCursorRect ? _cachedCursorRect->left() : 0;
}

} // namespace core
