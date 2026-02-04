#include "core/text_renderer.hpp"
#include "core/font_manager.hpp"
#include "modules/skparagraph/include/ParagraphBuilder.h"
#include "modules/skparagraph/include/ParagraphStyle.h"
#include "modules/skparagraph/include/TextStyle.h"
#include "modules/skparagraph/include/TextShadow.h"
#include "modules/skunicode/include/SkUnicode.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPoint.h"
#include <algorithm>

using namespace skia::textlayout;

namespace core {

// === TextStyleBuilder implementation ===

TextStyleBuilder::TextStyleBuilder() = default;

TextStyleBuilder::TextStyleBuilder(const TextStyle& base)
    : _fontFamily(base.fontFamily)
    , _fontSize(base.fontSize)
    , _color(base.color)
    , _fontWeight(base.fontWeight)
    , _italic(base.italic)
    , _underline(base.underline)
    , _letterSpacing(base.letterSpacing)
    , _wordSpacing(base.wordSpacing)
    , _hasBackground(base.hasBackground)
    , _backgroundColor(base.backgroundColor)
    , _hasShadow(base.hasShadow)
    , _shadow(base.shadow)
{}

TextStyleBuilder& TextStyleBuilder::fontFamily(const std::string& family) {
    _fontFamily = family;
    return *this;
}

TextStyleBuilder& TextStyleBuilder::fontSize(float size) {
    _fontSize = size;
    return *this;
}

TextStyleBuilder& TextStyleBuilder::color(Color c) {
    _color = c;
    return *this;
}

TextStyleBuilder& TextStyleBuilder::fontWeight(int weight) {
    _fontWeight = weight;
    return *this;
}

TextStyleBuilder& TextStyleBuilder::italic(bool value) {
    _italic = value;
    return *this;
}

TextStyleBuilder& TextStyleBuilder::underline(bool value) {
    _underline = value;
    return *this;
}

TextStyleBuilder& TextStyleBuilder::letterSpacing(float spacing) {
    _letterSpacing = spacing;
    return *this;
}

TextStyleBuilder& TextStyleBuilder::wordSpacing(float spacing) {
    _wordSpacing = spacing;
    return *this;
}

TextStyleBuilder& TextStyleBuilder::background(Color c) {
    _hasBackground = true;
    _backgroundColor = c;
    return *this;
}

TextStyleBuilder& TextStyleBuilder::noBackground() {
    _hasBackground = false;
    _backgroundColor = Color::transparent();
    return *this;
}

TextStyleBuilder& TextStyleBuilder::shadow(Color c, float offsetX, float offsetY, float blurSigma) {
    _hasShadow = true;
    _shadow.color = c;
    _shadow.offsetX = offsetX;
    _shadow.offsetY = offsetY;
    _shadow.blurSigma = blurSigma;
    return *this;
}

TextStyleBuilder& TextStyleBuilder::noShadow() {
    _hasShadow = false;
    _shadow = TextShadowStyle{};
    return *this;
}

TextStyle TextStyleBuilder::build() const {
    TextStyle style;
    style.fontFamily = _fontFamily;
    style.fontSize = _fontSize;
    style.color = _color;
    style.fontWeight = _fontWeight;
    style.italic = _italic;
    style.underline = _underline;
    style.letterSpacing = _letterSpacing;
    style.wordSpacing = _wordSpacing;
    style.hasBackground = _hasBackground;
    style.backgroundColor = _backgroundColor;
    style.hasShadow = _hasShadow;
    style.shadow = _shadow;
    return style;
}

// === TextRenderer implementation ===

TextRenderer::TextRenderer() = default;
TextRenderer::~TextRenderer() = default;

// Helper to convert our TextStyle to Skia's TextStyle
static skia::textlayout::TextStyle toSkiaStyle(const TextStyle& style, float lineHeight) {
    skia::textlayout::TextStyle skStyle;
    
#if defined(__APPLE__)
    // iOS: CoreText handles emoji fallback automatically (Apple Color Emoji uses 'emjc' format)
    skStyle.setFontFamilies({SkString(style.fontFamily.c_str())});
#else
    // Web: Explicit emoji font fallback (loaded via Local Font Access)
    skStyle.setFontFamilies({
        SkString(style.fontFamily.c_str()),
        SkString("System Emoji")
    });
#endif
    
    skStyle.setFontSize(style.fontSize);
    skStyle.setColor(style.color.toARGB());
    skStyle.setLetterSpacing(style.letterSpacing);
    skStyle.setWordSpacing(style.wordSpacing);
    skStyle.setFontStyle(SkFontStyle(
        style.fontWeight,
        SkFontStyle::kNormal_Width,
        style.italic ? SkFontStyle::kItalic_Slant : SkFontStyle::kUpright_Slant
    ));
    if (lineHeight > 0.0f) {
        skStyle.setHeight(lineHeight);
        skStyle.setHeightOverride(true);
    } else {
        skStyle.setHeightOverride(false);
    }
    if (style.underline) {
        skStyle.setDecoration(TextDecoration::kUnderline);
        skStyle.setDecorationColor(style.color.toARGB());
    }
    if (style.hasBackground) {
        SkPaint paint;
        paint.setColor(style.backgroundColor.toARGB());
        skStyle.setBackgroundPaint(paint);
    }
    if (style.hasShadow) {
        skStyle.addShadow(skia::textlayout::TextShadow(
            style.shadow.color.toARGB(),
            SkPoint::Make(style.shadow.offsetX, style.shadow.offsetY),
            style.shadow.blurSigma
        ));
    }
    return skStyle;
}

static skia::textlayout::TextAlign toSkiaAlignment(TextAlignment alignment) {
    using skia::textlayout::TextAlign;
    switch (alignment) {
        case TextAlignment::Right:
            return TextAlign::kRight;
        case TextAlignment::Center:
            return TextAlign::kCenter;
        case TextAlignment::Justify:
            return TextAlign::kJustify;
        case TextAlignment::Start:
            return TextAlign::kStart;
        case TextAlignment::End:
            return TextAlign::kEnd;
        case TextAlignment::Left:
        default:
            return TextAlign::kLeft;
    }
}

static std::u16string toUtf16(const std::string& utf8) {
    return SkUnicode::convertUtf8ToUtf16(utf8.c_str(), static_cast<int>(utf8.size()));
}

static std::string toUtf8(const std::u16string& utf16) {
    SkString utf8 = SkUnicode::convertUtf16ToUtf8(utf16);
    return std::string(utf8.c_str(), utf8.size());
}

void TextRenderer::setText(const std::string& text, const TextStyle& style) {
    // Single style is just a rich text with one span
    setRichText({{text, style}});
}

void TextRenderer::setRichText(const std::vector<StyledSpan>& spans) {
    if (spans.empty()) {
        _text.clear();
        _styleRuns.clear();
        _paragraph.reset();
        _needsLayout = false;
        _needsRebuildParagraph = false;
        _selectionAnchor = _selectionFocus = 0;
        _cursorAffinityDownstream = true;
        invalidateCursorCache();
        invalidateSelectionCache();
        return;
    }
    
    // Store internally for editing
    _text.clear();
    _styleRuns.clear();
    _defaultStyle = spans[0].style;
    
    int currentPos = 0;
    for (const auto& span : spans) {
        std::u16string spanText = toUtf16(span.text);
        int spanLen = static_cast<int>(spanText.length());
        if (spanLen > 0) {
            _text += spanText;
            _styleRuns.push_back({currentPos, currentPos + spanLen, span.style});
            currentPos += spanLen;
        }
    }
    
    mergeAdjacentStyleRuns();
    _needsRebuildParagraph = true;
    clampCursorPosition();
    invalidateCursorCache();
    invalidateSelectionCache();
}

void TextRenderer::setTextAlignment(TextAlignment alignment) {
    _textAlignment = alignment;
    _needsRebuildParagraph = true;
}

void TextRenderer::setMaxLines(int maxLines) {
    _maxLines = maxLines;
    _needsRebuildParagraph = true;
}

void TextRenderer::setEllipsis(const std::string& ellipsis) {
    _ellipsis = toUtf16(ellipsis);
    _needsRebuildParagraph = true;
}

void TextRenderer::setLineHeight(float height) {
    _lineHeight = height > 0.0f ? height : 0.0f;
    _needsRebuildParagraph = true;
}

void TextRenderer::setStrutStyle(const TextStrutStyle& strutStyle) {
    _strutStyle = strutStyle;
    _needsRebuildParagraph = true;
}

void TextRenderer::clearStrutStyle() {
    _strutStyle.enabled = false;
    _needsRebuildParagraph = true;
}

void TextRenderer::rebuildParagraph() const {
    // Get font collection
    auto fontCollection = FontManager::instance().getFontCollection();
    
    // Configure paragraph style
    ParagraphStyle paragraphStyle;
    paragraphStyle.setTextAlign(toSkiaAlignment(_textAlignment));
    if (_maxLines > 0) {
        paragraphStyle.setMaxLines(static_cast<size_t>(_maxLines));
    }
    if (!_ellipsis.empty()) {
        paragraphStyle.setEllipsis(_ellipsis);
    }
    if (_strutStyle.enabled) {
        StrutStyle strut;
        strut.setStrutEnabled(true);
        if (!_strutStyle.fontFamily.empty()) {
            strut.setFontFamilies({SkString(_strutStyle.fontFamily.c_str())});
        }
        if (_strutStyle.fontSize > 0.0f) {
            strut.setFontSize(_strutStyle.fontSize);
        } else if (_defaultStyle.fontSize > 0.0f) {
            strut.setFontSize(_defaultStyle.fontSize);
        }
        if (_strutStyle.height > 0.0f) {
            strut.setHeight(_strutStyle.height);
        }
        if (_strutStyle.leading > 0.0f) {
            strut.setLeading(_strutStyle.leading);
        }
        strut.setForceStrutHeight(_strutStyle.forceHeight);
        strut.setHeightOverride(_strutStyle.heightOverride);
        strut.setHalfLeading(_strutStyle.halfLeading);
        paragraphStyle.setStrutStyle(strut);
    }
    
    // Use default style
    skia::textlayout::TextStyle defaultSkStyle = toSkiaStyle(_defaultStyle, _lineHeight);
    paragraphStyle.setTextStyle(defaultSkStyle);
    
    // Build paragraph
    auto builder = ParagraphBuilder::make(paragraphStyle, fontCollection);
    if (!builder) {
        return;
    }
    
    if (_text.empty()) {
        // Empty text - just build empty paragraph
        _paragraph = builder->Build();
    } else if (_styleRuns.empty()) {
        // No style runs - use default style
        builder->pushStyle(defaultSkStyle);
        builder->addText(_text);
        builder->pop();
        _paragraph = builder->Build();
    } else {
        // Add each style run
        for (const auto& run : _styleRuns) {
            if (run.start < run.end && run.start >= 0 && run.end <= static_cast<int>(_text.length())) {
                skia::textlayout::TextStyle skStyle = toSkiaStyle(run.style, _lineHeight);
                builder->pushStyle(skStyle);
                std::u16string runText = _text.substr(run.start, run.end - run.start);
                builder->addText(runText);
                builder->pop();
            }
        }
        _paragraph = builder->Build();
    }
    
    if (_paragraph) {
        _needsLayout = true;
    }
    _needsRebuildParagraph = false;
    
    // Invalidate caches since paragraph changed
    invalidateCursorCache();
    invalidateSelectionCache();
}

void TextRenderer::rebuildParagraphIfNeeded() const {
    if (_needsRebuildParagraph) {
        rebuildParagraph();
    }
}

void TextRenderer::setMaxWidth(float maxWidth) {
    _maxWidth = maxWidth;
    _needsLayout = true;
    invalidateCursorCache();
    invalidateSelectionCache();
}

void TextRenderer::layoutIfNeeded() const {
    rebuildParagraphIfNeeded();
    if (!_paragraph || !_needsLayout) return;
    _paragraph->layout(_maxWidth);
    _needsLayout = false;
    invalidateCursorCache();
    invalidateSelectionCache();
}

// TODO: Consider caching rendered text to a texture for cursor blink optimization.
// When only cursor visibility changes, we could skip _paragraph->paint() and just
// redraw the cached text image + cursor. This would reduce draw calls during blink.
void TextRenderer::render(SkCanvas* canvas, float x, float y, bool showCursor) const {
    rebuildParagraphIfNeeded();
    if (!_paragraph) return;
    layoutIfNeeded();
    
    // Apply scale transform for high-DPI rendering
    // This allows all coordinates (x, y, font sizes, etc.) to be in logical pixels
    // while rendering at physical pixel resolution
    canvas->save();
    if (_scale != 1.0f) {
        canvas->scale(_scale, _scale);
    }
    
    // 1. Draw selection highlight (behind text)
    if (hasSelection()) {
        drawSelection(canvas, x, y);
    }
    
    // 2. Draw text
    _paragraph->paint(canvas, x, y);
    
    // 3. Draw cursor (on top of text)
    // Only show cursor when there's no selection (typical text editor behavior)
    if (showCursor && !hasSelection()) {
        drawCursor(canvas, x, y);
    }
    
    canvas->restore();
}

// === Layout metrics ===

float TextRenderer::getHeight() const {
    layoutIfNeeded();
    return _paragraph ? _paragraph->getHeight() : 0;
}

float TextRenderer::getWidth() const {
    layoutIfNeeded();
    return _paragraph ? _paragraph->getMaxWidth() : 0;
}

float TextRenderer::getMaxIntrinsicWidth() const {
    layoutIfNeeded();
    return _paragraph ? _paragraph->getMaxIntrinsicWidth() : 0;
}

float TextRenderer::getMinIntrinsicWidth() const {
    layoutIfNeeded();
    return _paragraph ? _paragraph->getMinIntrinsicWidth() : 0;
}

int TextRenderer::getLineCount() const {
    layoutIfNeeded();
    return _paragraph ? static_cast<int>(_paragraph->lineNumber()) : 0;
}

int TextRenderer::getTextLength() const {
    return static_cast<int>(_text.length());
}

// === Text Content ===

std::string TextRenderer::getText() const {
    return toUtf8(_text);
}

std::string TextRenderer::getSelectedText() const {
    if (!hasSelection()) return "";
    auto [start, end] = getSelection();
    if (start < 0 || end <= start) return "";
    std::u16string slice = _text.substr(static_cast<size_t>(start),
                                        static_cast<size_t>(end - start));
    return toUtf8(slice);
}

// === Text Editing ===

void TextRenderer::insertText(const std::string& newText) {
    std::u16string newText16 = toUtf16(newText);
    if (newText16.empty()) return;
    
    // Delete selection if any
    if (hasSelection()) {
        deleteSelection();
    }
    
    insertTextAt(newText16, _selectionFocus);
    
    // Move cursor to end of inserted text
    _selectionAnchor = _selectionFocus = _selectionFocus + static_cast<int>(newText16.length());
    clampCursorPosition();
    invalidateCursorCache();
}

void TextRenderer::deleteBackward() {
    if (hasSelection()) {
        deleteSelection();
        return;
    }
    
    if (_selectionFocus <= 0) return;
    
    // Delete the grapheme cluster before the cursor
    int deleteStart = _selectionFocus - 1;
    int deleteEnd = _selectionFocus;
    if (getGraphemeClusterRangeAt(_selectionFocus - 1, &deleteStart, &deleteEnd)) {
        deleteRange(deleteStart, deleteEnd);
        _selectionAnchor = _selectionFocus = deleteStart;
    } else {
        deleteRange(deleteStart, _selectionFocus);
        _selectionAnchor = _selectionFocus = deleteStart;
    }
    clampCursorPosition();
    invalidateCursorCache();
}

void TextRenderer::deleteForward() {
    if (hasSelection()) {
        deleteSelection();
        return;
    }
    
    if (_selectionFocus >= static_cast<int>(_text.length())) return;
    
    // Delete the grapheme cluster after the cursor
    int deleteStart = _selectionFocus;
    int deleteEnd = _selectionFocus + 1;
    if (getGraphemeClusterRangeAt(_selectionFocus, &deleteStart, &deleteEnd)) {
        deleteRange(deleteStart, deleteEnd);
        _selectionAnchor = _selectionFocus = deleteStart;
    } else {
        deleteRange(_selectionFocus, _selectionFocus + 1);
    }
    clampCursorPosition();
    invalidateCursorCache();
}

void TextRenderer::deleteWordBackward() {
    if (hasSelection()) {
        deleteSelection();
        return;
    }
    
    if (_selectionFocus <= 0) return;
    
    // Find word boundary before cursor
    auto boundary = getWordBoundary(_selectionFocus - 1);
    int deleteStart = boundary ? boundary->first : 0;
    
    deleteRange(deleteStart, _selectionFocus);
    _selectionAnchor = _selectionFocus = deleteStart;
    clampCursorPosition();
    invalidateCursorCache();
}

void TextRenderer::deleteWordForward() {
    if (hasSelection()) {
        deleteSelection();
        return;
    }
    
    if (_selectionFocus >= static_cast<int>(_text.length())) return;
    
    // Find word boundary after cursor
    auto boundary = getWordBoundary(_selectionFocus);
    int wordEnd = boundary ? boundary->second : static_cast<int>(_text.length());
    
    deleteRange(_selectionFocus, wordEnd);
    clampCursorPosition();
    invalidateCursorCache();
}

void TextRenderer::deleteSelection() {
    if (!hasSelection()) return;
    
    auto [start, end] = getSelection();
    deleteRange(start, end);
    _selectionAnchor = _selectionFocus = start;
    clampCursorPosition();
    invalidateCursorCache();
    invalidateSelectionCache();
}

void TextRenderer::insertStyledText(const StyledSpan& span) {
    std::u16string spanText = toUtf16(span.text);
    if (spanText.empty()) return;
    
    // Delete selection if any
    if (hasSelection()) {
        deleteSelection();
    }
    
    int position = _selectionFocus;
    int len = static_cast<int>(spanText.length());
    
    // Insert into plain text
    _text.insert(position, spanText);
    
    // Adjust existing style runs for the insertion
    adjustStyleRunsForInsert(position, len);
    
    // Add new style run for the inserted text
    _styleRuns.push_back({position, position + len, span.style});
    mergeAdjacentStyleRuns();
    
    _needsRebuildParagraph = true;
    
    // Move cursor to end of inserted text
    _selectionAnchor = _selectionFocus = position + len;
    clampCursorPosition();
    invalidateCursorCache();
}

void TextRenderer::applyStyleToSelection(const TextStyle& style) {
    if (!hasSelection()) return;
    
    auto [start, end] = getSelection();
    
    // Remove or split existing style runs that overlap with the selection
    std::vector<StyleRun> newRuns;
    for (const auto& run : _styleRuns) {
        if (run.end <= start || run.start >= end) {
            // No overlap - keep as is
            newRuns.push_back(run);
        } else if (run.start >= start && run.end <= end) {
            // Completely inside selection - discard (will be replaced)
        } else if (run.start < start && run.end > end) {
            // Selection is inside this run - split into two
            newRuns.push_back({run.start, start, run.style});
            newRuns.push_back({end, run.end, run.style});
        } else if (run.start < start) {
            // Overlaps on the left
            newRuns.push_back({run.start, start, run.style});
        } else {
            // Overlaps on the right
            newRuns.push_back({end, run.end, run.style});
        }
    }
    
    // Add the new style run for the selection
    newRuns.push_back({start, end, style});
    
    _styleRuns = std::move(newRuns);
    mergeAdjacentStyleRuns();
    _needsRebuildParagraph = true;
}

TextStyle TextRenderer::getStyleAtCursor() const {
    // When cursor is at a boundary, prefer the style to the left (the preceding character)
    // This matches typical text editor behavior where typing continues in the same style
    int position = _selectionFocus;
    if (position > 0) {
        return getStyleAtPosition(position - 1);
    }
    return getStyleAtPosition(position);
}

// === Internal editing helpers ===

void TextRenderer::insertTextAt(const std::u16string& newText, int position) {
    int len = static_cast<int>(newText.length());
    
    // Get style for inserted text (inherit from position to the left)
    TextStyle insertStyle = getStyleAtPosition(position > 0 ? position - 1 : position);
    
    // Insert into plain text
    _text.insert(position, newText);
    
    // Adjust style runs
    adjustStyleRunsForInsert(position, len);
    
    // If we inserted into an existing run, it's already extended
    // If we inserted at a boundary or in empty text, we need to ensure coverage
    bool covered = false;
    for (const auto& run : _styleRuns) {
        if (run.start <= position && run.end >= position + len) {
            covered = true;
            break;
        }
    }
    
    if (!covered && !newText.empty()) {
        // Need to add a style run for the inserted text
        // Find where to insert it
        StyleRun newRun{position, position + len, insertStyle};
        
        // Insert in sorted order
        auto it = std::lower_bound(_styleRuns.begin(), _styleRuns.end(), newRun,
            [](const StyleRun& a, const StyleRun& b) { return a.start < b.start; });
        _styleRuns.insert(it, newRun);
    }
    
    mergeAdjacentStyleRuns();
    _needsRebuildParagraph = true;
}

void TextRenderer::deleteRange(int start, int end) {
    if (start >= end || start < 0 || end > static_cast<int>(_text.length())) return;
    
    // Delete from plain text
    _text.erase(start, end - start);
    
    // Adjust style runs
    adjustStyleRunsForDelete(start, end);
    
    mergeAdjacentStyleRuns();
    _needsRebuildParagraph = true;
    invalidateSelectionCache();
}

void TextRenderer::adjustStyleRunsForInsert(int position, int length) {
    for (auto& run : _styleRuns) {
        if (run.start >= position) {
            // Run is entirely after insert point - shift it
            run.start += length;
            run.end += length;
        } else if (run.end > position) {
            // Insert point is inside this run - extend it
            run.end += length;
        }
        // Runs entirely before insert point are unchanged
    }
}

void TextRenderer::adjustStyleRunsForDelete(int start, int end) {
    int len = end - start;
    std::vector<StyleRun> newRuns;
    
    for (auto& run : _styleRuns) {
        if (run.end <= start) {
            // Run entirely before deletion - keep as is
            newRuns.push_back(run);
        } else if (run.start >= end) {
            // Run entirely after deletion - shift back
            run.start -= len;
            run.end -= len;
            newRuns.push_back(run);
        } else {
            // Run overlaps with deletion
            if (run.start < start && run.end > end) {
                // Deletion is entirely within this run - shrink it
                run.end -= len;
                newRuns.push_back(run);
            } else if (run.start < start) {
                // Run starts before deletion, ends within - truncate
                run.end = start;
                if (run.start < run.end) {
                    newRuns.push_back(run);
                }
            } else if (run.end > end) {
                // Run starts within deletion, ends after - shift and truncate
                run.start = start;
                run.end -= len;
                if (run.start < run.end) {
                    newRuns.push_back(run);
                }
            }
            // Else run is entirely within deletion - discard it
        }
    }
    
    _styleRuns = std::move(newRuns);
}

TextStyle TextRenderer::getStyleAtPosition(int position) const {
    for (const auto& run : _styleRuns) {
        if (position >= run.start && position < run.end) {
            return run.style;
        }
    }
    return _defaultStyle;
}

bool TextRenderer::getGraphemeClusterRangeAt(int position, int* start, int* end) const {
    rebuildParagraphIfNeeded();
    if (!_paragraph || position < 0) return false;
    layoutIfNeeded();
    Paragraph::GlyphInfo info;
    if (!_paragraph->getGlyphInfoAtUTF16Offset(static_cast<size_t>(position), &info)) {
        return false;
    }
    if (start) {
        *start = static_cast<int>(info.fGraphemeClusterTextRange.start);
    }
    if (end) {
        *end = static_cast<int>(info.fGraphemeClusterTextRange.end);
    }
    return true;
}

void TextRenderer::mergeAdjacentStyleRuns() {
    if (_styleRuns.size() < 2) return;
    
    // Sort by start position
    std::sort(_styleRuns.begin(), _styleRuns.end(),
        [](const StyleRun& a, const StyleRun& b) { return a.start < b.start; });
    
    std::vector<StyleRun> merged;
    merged.push_back(_styleRuns[0]);
    
    for (size_t i = 1; i < _styleRuns.size(); ++i) {
        StyleRun& last = merged.back();
        const StyleRun& current = _styleRuns[i];
        
        // Merge if adjacent and same style
        if (last.end == current.start && last.style == current.style) {
            last.end = current.end;
        } else if (current.start < last.end) {
            // Overlapping runs - the later run (current) takes precedence for the overlap
            if (current.end > last.end) {
                // Current extends beyond last - truncate last and add current
                last.end = current.start;
                if (last.start < last.end) {
                    merged.push_back(current);
                } else {
                    merged.back() = current;
                }
            } else if (current.end <= last.end) {
                // Current is entirely inside last - split last around current
                int oldLastEnd = last.end;
                TextStyle oldLastStyle = last.style;
                
                // Truncate the first part
                last.end = current.start;
                
                // Add the new run (current)
                if (last.start < last.end) {
                    merged.push_back(current);
                } else {
                    // First part is empty, replace with current
                    merged.back() = current;
                }
                
                // Add the remainder of the old run after current (if any)
                if (current.end < oldLastEnd) {
                    merged.push_back({current.end, oldLastEnd, oldLastStyle});
                }
            }
        } else {
            merged.push_back(current);
        }
    }
    
    _styleRuns = std::move(merged);
}

void TextRenderer::clampCursorPosition() {
    int textLen = static_cast<int>(_text.length());
    _selectionAnchor = std::clamp(_selectionAnchor, 0, textLen);
    _selectionFocus = std::clamp(_selectionFocus, 0, textLen);
}

// === Cursor Navigation ===

void TextRenderer::moveCursorLeft(bool extendSelection) {
    if (!extendSelection && hasSelection()) {
        // Collapse selection to start
        auto [start, end] = getSelection();
        _selectionAnchor = _selectionFocus = start;
        invalidateCursorCache();
        invalidateSelectionCache();
        return;
    }
    
    if (_selectionFocus > 0) {
        int clusterStart = _selectionFocus - 1;
        int clusterEnd = _selectionFocus;
        if (getGraphemeClusterRangeAt(_selectionFocus - 1, &clusterStart, &clusterEnd)) {
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

void TextRenderer::moveCursorRight(bool extendSelection) {
    if (!extendSelection && hasSelection()) {
        // Collapse selection to end
        auto [start, end] = getSelection();
        _selectionAnchor = _selectionFocus = end;
        invalidateCursorCache();
        invalidateSelectionCache();
        return;
    }
    
    if (_selectionFocus < static_cast<int>(_text.length())) {
        int clusterStart = _selectionFocus;
        int clusterEnd = _selectionFocus + 1;
        if (getGraphemeClusterRangeAt(_selectionFocus, &clusterStart, &clusterEnd)) {
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

void TextRenderer::moveCursorUp(bool extendSelection) {
    rebuildParagraphIfNeeded();
    if (!_paragraph) return;
    layoutIfNeeded();
    
    // Get current line
    int currentLine = getLineIndexForPosition(_selectionFocus);
    if (currentLine <= 0) {
        // Already on first line, move to start
        _selectionFocus = 0;
    } else {
        // Move to same x position on previous line
        std::vector<LineMetrics> lineMetrics;
        _paragraph->getLineMetrics(lineMetrics);
        
        float currentX = getXPositionForCursor();
        
        // Find position at same X on previous line
        float prevLineY = lineMetrics[currentLine - 1].fBaseline - lineMetrics[currentLine - 1].fAscent / 2;
        int pos = 0;
        bool downstream = true;
        if (getGlyphPositionWithAffinityAtCoordinate(currentX, prevLineY, &pos, &downstream)) {
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

void TextRenderer::moveCursorDown(bool extendSelection) {
    rebuildParagraphIfNeeded();
    if (!_paragraph) return;
    layoutIfNeeded();
    
    std::vector<LineMetrics> lineMetrics;
    _paragraph->getLineMetrics(lineMetrics);
    
    int currentLine = getLineIndexForPosition(_selectionFocus);
    if (currentLine >= static_cast<int>(lineMetrics.size()) - 1) {
        // Already on last line, move to end
        _selectionFocus = static_cast<int>(_text.length());
    } else {
        // Move to same x position on next line
        float currentX = getXPositionForCursor();
        
        // Find position at same X on next line
        float nextLineY = lineMetrics[currentLine + 1].fBaseline - lineMetrics[currentLine + 1].fAscent / 2;
        int pos = 0;
        bool downstream = true;
        if (getGlyphPositionWithAffinityAtCoordinate(currentX, nextLineY, &pos, &downstream)) {
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

void TextRenderer::moveCursorToWordStart(bool extendSelection) {
    if (_selectionFocus <= 0) return;
    
    auto boundary = getWordBoundary(_selectionFocus - 1);
    if (boundary) {
        _selectionFocus = boundary->first;
    }
    
    if (!extendSelection) {
        _selectionAnchor = _selectionFocus;
    }
    invalidateCursorCache();
    if (extendSelection) invalidateSelectionCache();
}

void TextRenderer::moveCursorToWordEnd(bool extendSelection) {
    if (_selectionFocus >= static_cast<int>(_text.length())) return;
    
    auto boundary = getWordBoundary(_selectionFocus);
    if (boundary) {
        _selectionFocus = boundary->second;
    }
    
    if (!extendSelection) {
        _selectionAnchor = _selectionFocus;
    }
    invalidateCursorCache();
    if (extendSelection) invalidateSelectionCache();
}

void TextRenderer::moveCursorToLineStart(bool extendSelection) {
    auto boundary = getLineBoundary(_selectionFocus);
    if (boundary) {
        _selectionFocus = boundary->first;
    }
    
    if (!extendSelection) {
        _selectionAnchor = _selectionFocus;
    }
    invalidateCursorCache();
    if (extendSelection) invalidateSelectionCache();
}

void TextRenderer::moveCursorToLineEnd(bool extendSelection) {
    auto boundary = getLineBoundary(_selectionFocus);
    if (boundary) {
        int end = boundary->second;
        // Don't include the newline character if there is one
        if (end > boundary->first && end <= static_cast<int>(_text.length()) && 
            _text[end - 1] == u'\n') {
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

void TextRenderer::moveCursorToDocumentStart(bool extendSelection) {
    _selectionFocus = 0;
    
    if (!extendSelection) {
        _selectionAnchor = _selectionFocus;
    }
    invalidateCursorCache();
    if (extendSelection) invalidateSelectionCache();
}

void TextRenderer::moveCursorToDocumentEnd(bool extendSelection) {
    _selectionFocus = static_cast<int>(_text.length());
    
    if (!extendSelection) {
        _selectionAnchor = _selectionFocus;
    }
    invalidateCursorCache();
    if (extendSelection) invalidateSelectionCache();
}

void TextRenderer::selectAll() {
    _selectionAnchor = 0;
    _selectionFocus = static_cast<int>(_text.length());
    invalidateCursorCache();
    invalidateSelectionCache();
}

int TextRenderer::getLineIndexForPosition(int position) const {
    rebuildParagraphIfNeeded();
    if (!_paragraph) return 0;
    layoutIfNeeded();
    
    std::vector<LineMetrics> lineMetrics;
    _paragraph->getLineMetrics(lineMetrics);
    
    for (size_t i = 0; i < lineMetrics.size(); ++i) {
        if (position >= static_cast<int>(lineMetrics[i].fStartIndex) &&
            position < static_cast<int>(lineMetrics[i].fEndIndex)) {
            return static_cast<int>(i);
        }
    }
    
    return lineMetrics.empty() ? 0 : static_cast<int>(lineMetrics.size() - 1);
}

float TextRenderer::getXPositionForCursor() const {
    SkRect rect = getCursorRect();
    return rect.isEmpty() ? 0 : rect.left();
}

// === Cursor ===

void TextRenderer::setCursorPosition(int position) {
    _selectionAnchor = position;
    _selectionFocus = position;
    clampCursorPosition();
    _cursorAffinityDownstream = true;
    invalidateCursorCache();
    invalidateSelectionCache();
}

int TextRenderer::getCursorPosition() const {
    return _selectionFocus;
}

void TextRenderer::setCursorPositionAtCoordinate(float x, float y) {
    int pos = 0;
    bool downstream = true;
    if (getGlyphPositionWithAffinityAtCoordinate(x, y, &pos, &downstream)) {
        _selectionAnchor = pos;
        _selectionFocus = pos;
        _cursorAffinityDownstream = downstream;
        clampCursorPosition();
        invalidateCursorCache();
        invalidateSelectionCache();
    }
}

// === Selection ===

void TextRenderer::setSelection(int start, int end) {
    _selectionAnchor = start;
    _selectionFocus = end;
    clampCursorPosition();
    _cursorAffinityDownstream = true;
    invalidateCursorCache();
    invalidateSelectionCache();
}

void TextRenderer::clearSelection() {
    _selectionAnchor = _selectionFocus;
    invalidateSelectionCache();
}

bool TextRenderer::hasSelection() const {
    return _selectionAnchor != _selectionFocus;
}

std::pair<int, int> TextRenderer::getSelection() const {
    return {std::min(_selectionAnchor, _selectionFocus),
            std::max(_selectionAnchor, _selectionFocus)};
}

void TextRenderer::setWordSelectionAtCoordinate(float x, float y) {
    auto pos = getGlyphPositionAtCoordinate(x, y);
    if (pos) {
        auto boundary = getWordBoundary(*pos);
        if (boundary) {
            setSelection(boundary->first, boundary->second);
        }
    }
}

void TextRenderer::setLineSelectionAtCoordinate(float x, float y) {
    auto pos = getGlyphPositionAtCoordinate(x, y);
    if (pos) {
        auto boundary = getLineBoundary(*pos);
        if (boundary) {
            setSelection(boundary->first, boundary->second);
        }
    }
}

void TextRenderer::beginSelectionAtCoordinate(float x, float y) {
    int pos = 0;
    bool downstream = true;
    if (getGlyphPositionWithAffinityAtCoordinate(x, y, &pos, &downstream)) {
        _selectionAnchor = pos;
        _selectionFocus = pos;
        _cursorAffinityDownstream = downstream;
        invalidateCursorCache();
        invalidateSelectionCache();
    }
}

void TextRenderer::extendSelectionToCoordinate(float x, float y) {
    int pos = 0;
    bool downstream = true;
    if (getGlyphPositionWithAffinityAtCoordinate(x, y, &pos, &downstream)) {
        _selectionFocus = pos;
        _cursorAffinityDownstream = downstream;
        // Anchor stays fixed, only focus moves
        invalidateCursorCache();
        invalidateSelectionCache();
    }
}

// === Colors ===

void TextRenderer::setCursorColor(Color color) {
    _cursorColor = color;
}

Color TextRenderer::getCursorColor() const {
    return _cursorColor;
}

void TextRenderer::setSelectionColor(Color color) {
    _selectionColor = color;
}

Color TextRenderer::getSelectionColor() const {
    return _selectionColor;
}

void TextRenderer::setScale(float scale) {
    _scale = scale;
}

float TextRenderer::getScale() const {
    return _scale;
}

// === Query methods ===

std::optional<int> TextRenderer::getGlyphPositionAtCoordinate(float x, float y) const {
    int pos = 0;
    bool downstream = true;
    if (!getGlyphPositionWithAffinityAtCoordinate(x, y, &pos, &downstream)) {
        return std::nullopt;
    }
    return pos;
}

bool TextRenderer::getGlyphPositionWithAffinityAtCoordinate(float x, float y, int* position, bool* downstream) const {
    rebuildParagraphIfNeeded();
    if (!_paragraph) return false;
    layoutIfNeeded();
    auto pos = _paragraph->getGlyphPositionAtCoordinate(x, y);
    if (position) {
        *position = static_cast<int>(pos.position);
    }
    if (downstream) {
        *downstream = (pos.affinity == skia::textlayout::Affinity::kDownstream);
    }
    return true;
}

std::vector<SkRect> TextRenderer::getRectsForRange(int start, int end) const {
    std::vector<SkRect> result;
    rebuildParagraphIfNeeded();
    if (!_paragraph || start >= end) return result;
    layoutIfNeeded();
    
    auto boxes = _paragraph->getRectsForRange(
        start, end,
        RectHeightStyle::kTight,
        RectWidthStyle::kTight
    );
    
    for (const auto& box : boxes) {
        result.push_back(box.rect);
    }
    return result;
}

std::optional<std::pair<int, int>> TextRenderer::getWordBoundary(int position) const {
    rebuildParagraphIfNeeded();
    if (!_paragraph) return std::nullopt;
    layoutIfNeeded();
    
    auto range = _paragraph->getWordBoundary(position);
    return std::make_pair(static_cast<int>(range.start), static_cast<int>(range.end));
}

std::optional<std::pair<int, int>> TextRenderer::getLineBoundary(int position) const {
    rebuildParagraphIfNeeded();
    if (!_paragraph) return std::nullopt;
    layoutIfNeeded();
    
    // Get line metrics to find which line contains this position
    std::vector<LineMetrics> lineMetrics;
    _paragraph->getLineMetrics(lineMetrics);
    
    for (const auto& line : lineMetrics) {
        if (position >= static_cast<int>(line.fStartIndex) && 
            position < static_cast<int>(line.fEndIndex)) {
            return std::make_pair(static_cast<int>(line.fStartIndex), 
                                  static_cast<int>(line.fEndIndex));
        }
    }
    
    // If position is at the very end, return last line
    if (!lineMetrics.empty()) {
        const auto& lastLine = lineMetrics.back();
        return std::make_pair(static_cast<int>(lastLine.fStartIndex), 
                              static_cast<int>(lastLine.fEndIndex));
    }
    
    return std::nullopt;
}

// === Private rendering helpers ===

void TextRenderer::invalidateCursorCache() const {
    _cachedCursorRect.reset();
}

void TextRenderer::invalidateSelectionCache() const {
    _cachedSelectionRects.reset();
}

float TextRenderer::getDefaultCursorHeight() const {
    // Derive fallback height from default font size (1.2x line height multiplier is standard)
    return _defaultStyle.fontSize > 0.0f ? _defaultStyle.fontSize * 1.2f : 20.0f;
}

void TextRenderer::drawSelection(SkCanvas* canvas, float x, float y) const {
    auto [start, end] = getSelection();
    
    // Check if cache is valid
    if (!_cachedSelectionRects || 
        _cachedSelectionStart != start || 
        _cachedSelectionEnd != end) {
        // Rebuild cache
        _cachedSelectionRects = getRectsForRange(start, end);
        _cachedSelectionStart = start;
        _cachedSelectionEnd = end;
    }
    
    SkPaint paint;
    paint.setColor(_selectionColor.toARGB());
    paint.setStyle(SkPaint::kFill_Style);
    
    for (const auto& rect : *_cachedSelectionRects) {
        // Offset rect by text position
        SkRect offsetRect = rect.makeOffset(x, y);
        canvas->drawRect(offsetRect, paint);
    }
}

void TextRenderer::drawCursor(SkCanvas* canvas, float x, float y) const {
    SkRect cursorRect = getCursorRect();
    if (cursorRect.isEmpty()) return;
    
    // Offset by text position
    cursorRect.offset(x, y);
    
    SkPaint paint;
    paint.setColor(_cursorColor.toARGB());
    paint.setStyle(SkPaint::kFill_Style);
    
    canvas->drawRect(cursorRect, paint);
}

SkRect TextRenderer::getCursorRect() const {
    // Check if cache is valid
    if (_cachedCursorRect && 
        _cachedCursorPosition == _selectionFocus && 
        _cachedCursorAffinity == _cursorAffinityDownstream) {
        return *_cachedCursorRect;
    }
    
    // Cache miss - compute cursor rect
    rebuildParagraphIfNeeded();
    if (!_paragraph) {
        _cachedCursorRect = SkRect::MakeEmpty();
        return *_cachedCursorRect;
    }
    layoutIfNeeded();

    // If cursor is right after a newline, place it at the start of the next line.
    if (_selectionFocus > 0 && _selectionFocus <= static_cast<int>(_text.length()) &&
        _text[_selectionFocus - 1] == u'\n') {
        std::vector<LineMetrics> lineMetrics;
        _paragraph->getLineMetrics(lineMetrics);
        int lineIndex = getLineIndexForPosition(_selectionFocus);
        if (!lineMetrics.empty() && lineIndex >= 0 && lineIndex < static_cast<int>(lineMetrics.size())) {
            const auto& line = lineMetrics[static_cast<size_t>(lineIndex)];
            float top = static_cast<float>(line.fBaseline - line.fAscent);
            float height = static_cast<float>(line.fAscent + line.fDescent);
            if (height <= 0) {
                height = static_cast<float>(line.fHeight);
            }
            float left = static_cast<float>(line.fLeft);
            _cachedCursorRect = SkRect::MakeXYWH(left, top, kCursorWidth, height);
            _cachedCursorPosition = _selectionFocus;
            _cachedCursorAffinity = _cursorAffinityDownstream;
            return *_cachedCursorRect;
        }
    }
    
    int textLen = static_cast<int>(_text.length());
    if (textLen > 0) {
        int anchorIndex = _selectionFocus;
        bool useLeadingEdge = _cursorAffinityDownstream;
        if (_cursorAffinityDownstream) {
            if (anchorIndex >= textLen) {
                anchorIndex = textLen - 1;
                useLeadingEdge = false; // end of text -> use trailing edge
            }
        } else {
            anchorIndex = _selectionFocus - 1;
            useLeadingEdge = false;
            if (anchorIndex < 0) {
                anchorIndex = 0;
                useLeadingEdge = true; // start of text -> use leading edge
            }
        }

        Paragraph::GlyphInfo info;
        if (_paragraph->getGlyphInfoAtUTF16Offset(static_cast<size_t>(anchorIndex), &info)) {
            auto boxes = _paragraph->getRectsForRange(
                info.fGraphemeClusterTextRange.start,
                info.fGraphemeClusterTextRange.end,
                RectHeightStyle::kTight,
                RectWidthStyle::kTight
            );
            if (!boxes.empty()) {
                const auto& box = boxes[0];
                const SkRect& rect = box.rect;
                const bool isRtl = box.direction == skia::textlayout::TextDirection::kRtl;
                float cursorX = useLeadingEdge
                    ? (isRtl ? rect.right() : rect.left())
                    : (isRtl ? rect.left() : rect.right());
                _cachedCursorRect = SkRect::MakeXYWH(cursorX, rect.top(), kCursorWidth, rect.height());
                _cachedCursorPosition = _selectionFocus;
                _cachedCursorAffinity = _cursorAffinityDownstream;
                return *_cachedCursorRect;
            }
        }
    }

    // Fallback to previous logic if glyph info isn't available
    if (_selectionFocus > 0) {
        int clusterStart = _selectionFocus - 1;
        int clusterEnd = _selectionFocus;
        if (getGraphemeClusterRangeAt(_selectionFocus - 1, &clusterStart, &clusterEnd)) {
            auto boxes = _paragraph->getRectsForRange(
                clusterStart, clusterEnd,
                RectHeightStyle::kTight,
                RectWidthStyle::kTight
            );
            if (!boxes.empty()) {
                const auto& box = boxes[0].rect;
                _cachedCursorRect = SkRect::MakeXYWH(box.right(), box.top(), kCursorWidth, box.height());
                _cachedCursorPosition = _selectionFocus;
                _cachedCursorAffinity = _cursorAffinityDownstream;
                return *_cachedCursorRect;
            }
        }

        auto boxes = _paragraph->getRectsForRange(
            _selectionFocus - 1, _selectionFocus,
            RectHeightStyle::kTight,
            RectWidthStyle::kTight
        );
        if (!boxes.empty()) {
            const auto& box = boxes[0].rect;
            _cachedCursorRect = SkRect::MakeXYWH(box.right(), box.top(), kCursorWidth, box.height());
            _cachedCursorPosition = _selectionFocus;
            _cachedCursorAffinity = _cursorAffinityDownstream;
            return *_cachedCursorRect;
        }
    }

    // Cursor at position 0 - use the first character's height and left edge
    if (_selectionFocus == 0 && !_text.empty()) {
        int clusterStart = 0;
        int clusterEnd = 1;
        if (getGraphemeClusterRangeAt(0, &clusterStart, &clusterEnd)) {
            auto boxes = _paragraph->getRectsForRange(
                clusterStart, clusterEnd,
                RectHeightStyle::kTight,
                RectWidthStyle::kTight
            );
            if (!boxes.empty()) {
                const auto& box = boxes[0].rect;
                _cachedCursorRect = SkRect::MakeXYWH(box.left(), box.top(), kCursorWidth, box.height());
                _cachedCursorPosition = _selectionFocus;
                _cachedCursorAffinity = _cursorAffinityDownstream;
                return *_cachedCursorRect;
            }
        }

        auto boxes = _paragraph->getRectsForRange(
            0, 1,
            RectHeightStyle::kTight,
            RectWidthStyle::kTight
        );
        if (!boxes.empty()) {
            const auto& box = boxes[0].rect;
            _cachedCursorRect = SkRect::MakeXYWH(box.left(), box.top(), kCursorWidth, box.height());
            _cachedCursorPosition = _selectionFocus;
            _cachedCursorAffinity = _cursorAffinityDownstream;
            return *_cachedCursorRect;
        }
    }
    
    // Empty text - derive height from default font size
    float height = _paragraph->getHeight();
    if (height <= 0) {
        height = getDefaultCursorHeight();
    }
    _cachedCursorRect = SkRect::MakeXYWH(0, 0, kCursorWidth, height);
    _cachedCursorPosition = _selectionFocus;
    _cachedCursorAffinity = _cursorAffinityDownstream;
    return *_cachedCursorRect;
}

} // namespace core
