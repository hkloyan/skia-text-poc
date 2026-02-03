#include "TextRenderer.h"
#include "FontManager.h"
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

// === TextStyleBuilder implementation ===

TextStyleBuilder::TextStyleBuilder() = default;

TextStyleBuilder::TextStyleBuilder(const ::TextStyle& base)
    : fontFamily_(base.fontFamily)
    , fontSize_(base.fontSize)
    , color_(base.color)
    , fontWeight_(base.fontWeight)
    , italic_(base.italic)
    , underline_(base.underline)
    , letterSpacing_(base.letterSpacing)
    , wordSpacing_(base.wordSpacing)
    , hasBackground_(base.hasBackground)
    , backgroundColor_(base.backgroundColor)
    , hasShadow_(base.hasShadow)
    , shadow_(base.shadow)
{}

TextStyleBuilder& TextStyleBuilder::fontFamily(const std::string& family) {
    fontFamily_ = family;
    return *this;
}

TextStyleBuilder& TextStyleBuilder::fontSize(float size) {
    fontSize_ = size;
    return *this;
}

TextStyleBuilder& TextStyleBuilder::color(Color c) {
    color_ = c;
    return *this;
}

TextStyleBuilder& TextStyleBuilder::fontWeight(int weight) {
    fontWeight_ = weight;
    return *this;
}

TextStyleBuilder& TextStyleBuilder::italic(bool value) {
    italic_ = value;
    return *this;
}

TextStyleBuilder& TextStyleBuilder::underline(bool value) {
    underline_ = value;
    return *this;
}

TextStyleBuilder& TextStyleBuilder::letterSpacing(float spacing) {
    letterSpacing_ = spacing;
    return *this;
}

TextStyleBuilder& TextStyleBuilder::wordSpacing(float spacing) {
    wordSpacing_ = spacing;
    return *this;
}

TextStyleBuilder& TextStyleBuilder::background(Color c) {
    hasBackground_ = true;
    backgroundColor_ = c;
    return *this;
}

TextStyleBuilder& TextStyleBuilder::noBackground() {
    hasBackground_ = false;
    backgroundColor_ = Color::transparent();
    return *this;
}

TextStyleBuilder& TextStyleBuilder::shadow(Color c, float offsetX, float offsetY, float blurSigma) {
    hasShadow_ = true;
    shadow_.color = c;
    shadow_.offsetX = offsetX;
    shadow_.offsetY = offsetY;
    shadow_.blurSigma = blurSigma;
    return *this;
}

TextStyleBuilder& TextStyleBuilder::noShadow() {
    hasShadow_ = false;
    shadow_ = TextShadowStyle{};
    return *this;
}

::TextStyle TextStyleBuilder::build() const {
    ::TextStyle style;
    style.fontFamily = fontFamily_;
    style.fontSize = fontSize_;
    style.color = color_;
    style.fontWeight = fontWeight_;
    style.italic = italic_;
    style.underline = underline_;
    style.letterSpacing = letterSpacing_;
    style.wordSpacing = wordSpacing_;
    style.hasBackground = hasBackground_;
    style.backgroundColor = backgroundColor_;
    style.hasShadow = hasShadow_;
    style.shadow = shadow_;
    return style;
}

// === TextRenderer implementation ===

TextRenderer::TextRenderer() = default;
TextRenderer::~TextRenderer() = default;

// Helper to convert our TextStyle to Skia's TextStyle
static skia::textlayout::TextStyle toSkiaStyle(const ::TextStyle& style, float lineHeight) {
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

void TextRenderer::setText(const std::string& text, const ::TextStyle& style) {
    // Single style is just a rich text with one span
    setRichText({{text, style}});
}

void TextRenderer::setRichText(const std::vector<StyledSpan>& spans) {
    if (spans.empty()) {
        text_.clear();
        styleRuns_.clear();
        paragraph_.reset();
        needsLayout_ = false;
        needsRebuildParagraph_ = false;
        selectionAnchor_ = selectionFocus_ = 0;
        cursorAffinityDownstream_ = true;
        invalidateCursorCache();
        invalidateSelectionCache();
        return;
    }
    
    // Store internally for editing
    text_.clear();
    styleRuns_.clear();
    defaultStyle_ = spans[0].style;
    
    int currentPos = 0;
    for (const auto& span : spans) {
        std::u16string spanText = toUtf16(span.text);
        int spanLen = static_cast<int>(spanText.length());
        if (spanLen > 0) {
            text_ += spanText;
            styleRuns_.push_back({currentPos, currentPos + spanLen, span.style});
            currentPos += spanLen;
        }
    }
    
    mergeAdjacentStyleRuns();
    needsRebuildParagraph_ = true;
    clampCursorPosition();
    invalidateCursorCache();
    invalidateSelectionCache();
}

void TextRenderer::setTextAlignment(TextAlignment alignment) {
    textAlignment_ = alignment;
    needsRebuildParagraph_ = true;
}

void TextRenderer::setMaxLines(int maxLines) {
    maxLines_ = maxLines;
    needsRebuildParagraph_ = true;
}

void TextRenderer::setEllipsis(const std::string& ellipsis) {
    ellipsis_ = toUtf16(ellipsis);
    needsRebuildParagraph_ = true;
}

void TextRenderer::setLineHeight(float height) {
    lineHeight_ = height > 0.0f ? height : 0.0f;
    needsRebuildParagraph_ = true;
}

void TextRenderer::setStrutStyle(const TextStrutStyle& strutStyle) {
    strutStyle_ = strutStyle;
    needsRebuildParagraph_ = true;
}

void TextRenderer::clearStrutStyle() {
    strutStyle_.enabled = false;
    needsRebuildParagraph_ = true;
}

void TextRenderer::rebuildParagraph() const {
    // Get font collection
    auto fontCollection = FontManager::instance().getFontCollection();
    
    // Configure paragraph style
    ParagraphStyle paragraphStyle;
    paragraphStyle.setTextAlign(toSkiaAlignment(textAlignment_));
    if (maxLines_ > 0) {
        paragraphStyle.setMaxLines(static_cast<size_t>(maxLines_));
    }
    if (!ellipsis_.empty()) {
        paragraphStyle.setEllipsis(ellipsis_);
    }
    if (strutStyle_.enabled) {
        StrutStyle strut;
        strut.setStrutEnabled(true);
        if (!strutStyle_.fontFamily.empty()) {
            strut.setFontFamilies({SkString(strutStyle_.fontFamily.c_str())});
        }
        if (strutStyle_.fontSize > 0.0f) {
            strut.setFontSize(strutStyle_.fontSize);
        } else if (defaultStyle_.fontSize > 0.0f) {
            strut.setFontSize(defaultStyle_.fontSize);
        }
        if (strutStyle_.height > 0.0f) {
            strut.setHeight(strutStyle_.height);
        }
        if (strutStyle_.leading > 0.0f) {
            strut.setLeading(strutStyle_.leading);
        }
        strut.setForceStrutHeight(strutStyle_.forceHeight);
        strut.setHeightOverride(strutStyle_.heightOverride);
        strut.setHalfLeading(strutStyle_.halfLeading);
        paragraphStyle.setStrutStyle(strut);
    }
    
    // Use default style
    skia::textlayout::TextStyle defaultSkStyle = toSkiaStyle(defaultStyle_, lineHeight_);
    paragraphStyle.setTextStyle(defaultSkStyle);
    
    // Build paragraph
    auto builder = ParagraphBuilder::make(paragraphStyle, fontCollection);
    if (!builder) {
        return;
    }
    
    if (text_.empty()) {
        // Empty text - just build empty paragraph
        paragraph_ = builder->Build();
    } else if (styleRuns_.empty()) {
        // No style runs - use default style
        builder->pushStyle(defaultSkStyle);
        builder->addText(text_);
        builder->pop();
        paragraph_ = builder->Build();
    } else {
        // Add each style run
        for (const auto& run : styleRuns_) {
            if (run.start < run.end && run.start >= 0 && run.end <= static_cast<int>(text_.length())) {
                skia::textlayout::TextStyle skStyle = toSkiaStyle(run.style, lineHeight_);
                builder->pushStyle(skStyle);
                std::u16string runText = text_.substr(run.start, run.end - run.start);
                builder->addText(runText);
                builder->pop();
            }
        }
        paragraph_ = builder->Build();
    }
    
    if (paragraph_) {
        needsLayout_ = true;
    }
    needsRebuildParagraph_ = false;
    
    // Invalidate caches since paragraph changed
    invalidateCursorCache();
    invalidateSelectionCache();
}

void TextRenderer::rebuildParagraphIfNeeded() const {
    if (needsRebuildParagraph_) {
        rebuildParagraph();
    }
}

void TextRenderer::setMaxWidth(float maxWidth) {
    maxWidth_ = maxWidth;
    needsLayout_ = true;
    invalidateCursorCache();
    invalidateSelectionCache();
}

void TextRenderer::layoutIfNeeded() const {
    rebuildParagraphIfNeeded();
    if (!paragraph_ || !needsLayout_) return;
    paragraph_->layout(maxWidth_);
    needsLayout_ = false;
    invalidateCursorCache();
    invalidateSelectionCache();
}

// TODO: Consider caching rendered text to a texture for cursor blink optimization.
// When only cursor visibility changes, we could skip paragraph_->paint() and just
// redraw the cached text image + cursor. This would reduce draw calls during blink.
void TextRenderer::render(SkCanvas* canvas, float x, float y, bool showCursor) const {
    rebuildParagraphIfNeeded();
    if (!paragraph_) return;
    layoutIfNeeded();
    
    // Apply scale transform for high-DPI rendering
    // This allows all coordinates (x, y, font sizes, etc.) to be in logical pixels
    // while rendering at physical pixel resolution
    canvas->save();
    if (scale_ != 1.0f) {
        canvas->scale(scale_, scale_);
    }
    
    // 1. Draw selection highlight (behind text)
    if (hasSelection()) {
        drawSelection(canvas, x, y);
    }
    
    // 2. Draw text
    paragraph_->paint(canvas, x, y);
    
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
    return paragraph_ ? paragraph_->getHeight() : 0;
}

float TextRenderer::getWidth() const {
    layoutIfNeeded();
    return paragraph_ ? paragraph_->getMaxWidth() : 0;
}

float TextRenderer::getMaxIntrinsicWidth() const {
    layoutIfNeeded();
    return paragraph_ ? paragraph_->getMaxIntrinsicWidth() : 0;
}

float TextRenderer::getMinIntrinsicWidth() const {
    layoutIfNeeded();
    return paragraph_ ? paragraph_->getMinIntrinsicWidth() : 0;
}

int TextRenderer::getLineCount() const {
    layoutIfNeeded();
    return paragraph_ ? static_cast<int>(paragraph_->lineNumber()) : 0;
}

int TextRenderer::getTextLength() const {
    return static_cast<int>(text_.length());
}

// === Text Content ===

std::string TextRenderer::getText() const {
    return toUtf8(text_);
}

std::string TextRenderer::getSelectedText() const {
    if (!hasSelection()) return "";
    auto [start, end] = getSelection();
    if (start < 0 || end <= start) return "";
    std::u16string slice = text_.substr(static_cast<size_t>(start),
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
    
    insertTextAt(newText16, selectionFocus_);
    
    // Move cursor to end of inserted text
    selectionAnchor_ = selectionFocus_ = selectionFocus_ + static_cast<int>(newText16.length());
    clampCursorPosition();
    invalidateCursorCache();
}

void TextRenderer::deleteBackward() {
    if (hasSelection()) {
        deleteSelection();
        return;
    }
    
    if (selectionFocus_ <= 0) return;
    
    // Delete the grapheme cluster before the cursor
    int deleteStart = selectionFocus_ - 1;
    int deleteEnd = selectionFocus_;
    if (getGraphemeClusterRangeAt(selectionFocus_ - 1, &deleteStart, &deleteEnd)) {
        deleteRange(deleteStart, deleteEnd);
        selectionAnchor_ = selectionFocus_ = deleteStart;
    } else {
        deleteRange(deleteStart, selectionFocus_);
        selectionAnchor_ = selectionFocus_ = deleteStart;
    }
    clampCursorPosition();
    invalidateCursorCache();
}

void TextRenderer::deleteForward() {
    if (hasSelection()) {
        deleteSelection();
        return;
    }
    
    if (selectionFocus_ >= static_cast<int>(text_.length())) return;
    
    // Delete the grapheme cluster after the cursor
    int deleteStart = selectionFocus_;
    int deleteEnd = selectionFocus_ + 1;
    if (getGraphemeClusterRangeAt(selectionFocus_, &deleteStart, &deleteEnd)) {
        deleteRange(deleteStart, deleteEnd);
        selectionAnchor_ = selectionFocus_ = deleteStart;
    } else {
        deleteRange(selectionFocus_, selectionFocus_ + 1);
    }
    clampCursorPosition();
    invalidateCursorCache();
}

void TextRenderer::deleteWordBackward() {
    if (hasSelection()) {
        deleteSelection();
        return;
    }
    
    if (selectionFocus_ <= 0) return;
    
    // Find word boundary before cursor
    auto boundary = getWordBoundary(selectionFocus_ - 1);
    int deleteStart = boundary ? boundary->first : 0;
    
    deleteRange(deleteStart, selectionFocus_);
    selectionAnchor_ = selectionFocus_ = deleteStart;
    clampCursorPosition();
    invalidateCursorCache();
}

void TextRenderer::deleteWordForward() {
    if (hasSelection()) {
        deleteSelection();
        return;
    }
    
    if (selectionFocus_ >= static_cast<int>(text_.length())) return;
    
    // Find word boundary after cursor
    auto boundary = getWordBoundary(selectionFocus_);
    int wordEnd = boundary ? boundary->second : static_cast<int>(text_.length());
    
    deleteRange(selectionFocus_, wordEnd);
    clampCursorPosition();
    invalidateCursorCache();
}

void TextRenderer::deleteSelection() {
    if (!hasSelection()) return;
    
    auto [start, end] = getSelection();
    deleteRange(start, end);
    selectionAnchor_ = selectionFocus_ = start;
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
    
    int position = selectionFocus_;
    int len = static_cast<int>(spanText.length());
    
    // Insert into plain text
    text_.insert(position, spanText);
    
    // Adjust existing style runs for the insertion
    adjustStyleRunsForInsert(position, len);
    
    // Add new style run for the inserted text
    styleRuns_.push_back({position, position + len, span.style});
    mergeAdjacentStyleRuns();
    
    needsRebuildParagraph_ = true;
    
    // Move cursor to end of inserted text
    selectionAnchor_ = selectionFocus_ = position + len;
    clampCursorPosition();
    invalidateCursorCache();
}

void TextRenderer::applyStyleToSelection(const ::TextStyle& style) {
    if (!hasSelection()) return;
    
    auto [start, end] = getSelection();
    
    // Remove or split existing style runs that overlap with the selection
    std::vector<StyleRun> newRuns;
    for (const auto& run : styleRuns_) {
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
    
    styleRuns_ = std::move(newRuns);
    mergeAdjacentStyleRuns();
    needsRebuildParagraph_ = true;
}

::TextStyle TextRenderer::getStyleAtCursor() const {
    // When cursor is at a boundary, prefer the style to the left (the preceding character)
    // This matches typical text editor behavior where typing continues in the same style
    int position = selectionFocus_;
    if (position > 0) {
        return getStyleAtPosition(position - 1);
    }
    return getStyleAtPosition(position);
}

// === Internal editing helpers ===

void TextRenderer::insertTextAt(const std::u16string& newText, int position) {
    int len = static_cast<int>(newText.length());
    
    // Get style for inserted text (inherit from position to the left)
    ::TextStyle insertStyle = getStyleAtPosition(position > 0 ? position - 1 : position);
    
    // Insert into plain text
    text_.insert(position, newText);
    
    // Adjust style runs
    adjustStyleRunsForInsert(position, len);
    
    // If we inserted into an existing run, it's already extended
    // If we inserted at a boundary or in empty text, we need to ensure coverage
    bool covered = false;
    for (const auto& run : styleRuns_) {
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
        auto it = std::lower_bound(styleRuns_.begin(), styleRuns_.end(), newRun,
            [](const StyleRun& a, const StyleRun& b) { return a.start < b.start; });
        styleRuns_.insert(it, newRun);
    }
    
    mergeAdjacentStyleRuns();
    needsRebuildParagraph_ = true;
}

void TextRenderer::deleteRange(int start, int end) {
    if (start >= end || start < 0 || end > static_cast<int>(text_.length())) return;
    
    // Delete from plain text
    text_.erase(start, end - start);
    
    // Adjust style runs
    adjustStyleRunsForDelete(start, end);
    
    mergeAdjacentStyleRuns();
    needsRebuildParagraph_ = true;
    invalidateSelectionCache();
}

void TextRenderer::adjustStyleRunsForInsert(int position, int length) {
    for (auto& run : styleRuns_) {
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
    
    for (auto& run : styleRuns_) {
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
    
    styleRuns_ = std::move(newRuns);
}

::TextStyle TextRenderer::getStyleAtPosition(int position) const {
    for (const auto& run : styleRuns_) {
        if (position >= run.start && position < run.end) {
            return run.style;
        }
    }
    return defaultStyle_;
}

bool TextRenderer::getGraphemeClusterRangeAt(int position, int* start, int* end) const {
    rebuildParagraphIfNeeded();
    if (!paragraph_ || position < 0) return false;
    layoutIfNeeded();
    Paragraph::GlyphInfo info;
    if (!paragraph_->getGlyphInfoAtUTF16Offset(static_cast<size_t>(position), &info)) {
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
    if (styleRuns_.size() < 2) return;
    
    // Sort by start position
    std::sort(styleRuns_.begin(), styleRuns_.end(),
        [](const StyleRun& a, const StyleRun& b) { return a.start < b.start; });
    
    std::vector<StyleRun> merged;
    merged.push_back(styleRuns_[0]);
    
    for (size_t i = 1; i < styleRuns_.size(); ++i) {
        StyleRun& last = merged.back();
        const StyleRun& current = styleRuns_[i];
        
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
                ::TextStyle oldLastStyle = last.style;
                
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
    
    styleRuns_ = std::move(merged);
}

void TextRenderer::clampCursorPosition() {
    int textLen = static_cast<int>(text_.length());
    selectionAnchor_ = std::clamp(selectionAnchor_, 0, textLen);
    selectionFocus_ = std::clamp(selectionFocus_, 0, textLen);
}

// === Cursor Navigation ===

void TextRenderer::moveCursorLeft(bool extendSelection) {
    if (!extendSelection && hasSelection()) {
        // Collapse selection to start
        auto [start, end] = getSelection();
        selectionAnchor_ = selectionFocus_ = start;
        invalidateCursorCache();
        invalidateSelectionCache();
        return;
    }
    
    if (selectionFocus_ > 0) {
        int clusterStart = selectionFocus_ - 1;
        int clusterEnd = selectionFocus_;
        if (getGraphemeClusterRangeAt(selectionFocus_ - 1, &clusterStart, &clusterEnd)) {
            selectionFocus_ = clusterStart;
        } else {
            selectionFocus_--;
        }
        if (!extendSelection) {
            selectionAnchor_ = selectionFocus_;
        }
        invalidateCursorCache();
        if (extendSelection) invalidateSelectionCache();
    }
}

void TextRenderer::moveCursorRight(bool extendSelection) {
    if (!extendSelection && hasSelection()) {
        // Collapse selection to end
        auto [start, end] = getSelection();
        selectionAnchor_ = selectionFocus_ = end;
        invalidateCursorCache();
        invalidateSelectionCache();
        return;
    }
    
    if (selectionFocus_ < static_cast<int>(text_.length())) {
        int clusterStart = selectionFocus_;
        int clusterEnd = selectionFocus_ + 1;
        if (getGraphemeClusterRangeAt(selectionFocus_, &clusterStart, &clusterEnd)) {
            selectionFocus_ = clusterEnd;
        } else {
            selectionFocus_++;
        }
        if (!extendSelection) {
            selectionAnchor_ = selectionFocus_;
        }
        invalidateCursorCache();
        if (extendSelection) invalidateSelectionCache();
    }
}

void TextRenderer::moveCursorUp(bool extendSelection) {
    rebuildParagraphIfNeeded();
    if (!paragraph_) return;
    layoutIfNeeded();
    
    // Get current line
    int currentLine = getLineIndexForPosition(selectionFocus_);
    if (currentLine <= 0) {
        // Already on first line, move to start
        selectionFocus_ = 0;
    } else {
        // Move to same x position on previous line
        std::vector<LineMetrics> lineMetrics;
        paragraph_->getLineMetrics(lineMetrics);
        
        float currentX = getXPositionForCursor();
        
        // Find position at same X on previous line
        float prevLineY = lineMetrics[currentLine - 1].fBaseline - lineMetrics[currentLine - 1].fAscent / 2;
        int pos = 0;
        bool downstream = true;
        if (getGlyphPositionWithAffinityAtCoordinate(currentX, prevLineY, &pos, &downstream)) {
            selectionFocus_ = pos;
            cursorAffinityDownstream_ = downstream;
        }
    }
    
    if (!extendSelection) {
        selectionAnchor_ = selectionFocus_;
    }
    clampCursorPosition();
    invalidateCursorCache();
    if (extendSelection) invalidateSelectionCache();
}

void TextRenderer::moveCursorDown(bool extendSelection) {
    rebuildParagraphIfNeeded();
    if (!paragraph_) return;
    layoutIfNeeded();
    
    std::vector<LineMetrics> lineMetrics;
    paragraph_->getLineMetrics(lineMetrics);
    
    int currentLine = getLineIndexForPosition(selectionFocus_);
    if (currentLine >= static_cast<int>(lineMetrics.size()) - 1) {
        // Already on last line, move to end
        selectionFocus_ = static_cast<int>(text_.length());
    } else {
        // Move to same x position on next line
        float currentX = getXPositionForCursor();
        
        // Find position at same X on next line
        float nextLineY = lineMetrics[currentLine + 1].fBaseline - lineMetrics[currentLine + 1].fAscent / 2;
        int pos = 0;
        bool downstream = true;
        if (getGlyphPositionWithAffinityAtCoordinate(currentX, nextLineY, &pos, &downstream)) {
            selectionFocus_ = pos;
            cursorAffinityDownstream_ = downstream;
        }
    }
    
    if (!extendSelection) {
        selectionAnchor_ = selectionFocus_;
    }
    clampCursorPosition();
    invalidateCursorCache();
    if (extendSelection) invalidateSelectionCache();
}

void TextRenderer::moveCursorToWordStart(bool extendSelection) {
    if (selectionFocus_ <= 0) return;
    
    auto boundary = getWordBoundary(selectionFocus_ - 1);
    if (boundary) {
        selectionFocus_ = boundary->first;
    }
    
    if (!extendSelection) {
        selectionAnchor_ = selectionFocus_;
    }
    invalidateCursorCache();
    if (extendSelection) invalidateSelectionCache();
}

void TextRenderer::moveCursorToWordEnd(bool extendSelection) {
    if (selectionFocus_ >= static_cast<int>(text_.length())) return;
    
    auto boundary = getWordBoundary(selectionFocus_);
    if (boundary) {
        selectionFocus_ = boundary->second;
    }
    
    if (!extendSelection) {
        selectionAnchor_ = selectionFocus_;
    }
    invalidateCursorCache();
    if (extendSelection) invalidateSelectionCache();
}

void TextRenderer::moveCursorToLineStart(bool extendSelection) {
    auto boundary = getLineBoundary(selectionFocus_);
    if (boundary) {
        selectionFocus_ = boundary->first;
    }
    
    if (!extendSelection) {
        selectionAnchor_ = selectionFocus_;
    }
    invalidateCursorCache();
    if (extendSelection) invalidateSelectionCache();
}

void TextRenderer::moveCursorToLineEnd(bool extendSelection) {
    auto boundary = getLineBoundary(selectionFocus_);
    if (boundary) {
        int end = boundary->second;
        // Don't include the newline character if there is one
        if (end > boundary->first && end <= static_cast<int>(text_.length()) && 
            text_[end - 1] == u'\n') {
            end--;
        }
        selectionFocus_ = end;
    }
    
    if (!extendSelection) {
        selectionAnchor_ = selectionFocus_;
    }
    invalidateCursorCache();
    if (extendSelection) invalidateSelectionCache();
}

void TextRenderer::moveCursorToDocumentStart(bool extendSelection) {
    selectionFocus_ = 0;
    
    if (!extendSelection) {
        selectionAnchor_ = selectionFocus_;
    }
    invalidateCursorCache();
    if (extendSelection) invalidateSelectionCache();
}

void TextRenderer::moveCursorToDocumentEnd(bool extendSelection) {
    selectionFocus_ = static_cast<int>(text_.length());
    
    if (!extendSelection) {
        selectionAnchor_ = selectionFocus_;
    }
    invalidateCursorCache();
    if (extendSelection) invalidateSelectionCache();
}

void TextRenderer::selectAll() {
    selectionAnchor_ = 0;
    selectionFocus_ = static_cast<int>(text_.length());
    invalidateCursorCache();
    invalidateSelectionCache();
}

int TextRenderer::getLineIndexForPosition(int position) const {
    rebuildParagraphIfNeeded();
    if (!paragraph_) return 0;
    layoutIfNeeded();
    
    std::vector<LineMetrics> lineMetrics;
    paragraph_->getLineMetrics(lineMetrics);
    
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
    selectionAnchor_ = position;
    selectionFocus_ = position;
    clampCursorPosition();
    cursorAffinityDownstream_ = true;
    invalidateCursorCache();
    invalidateSelectionCache();
}

int TextRenderer::getCursorPosition() const {
    return selectionFocus_;
}

void TextRenderer::setCursorPositionAtCoordinate(float x, float y) {
    int pos = 0;
    bool downstream = true;
    if (getGlyphPositionWithAffinityAtCoordinate(x, y, &pos, &downstream)) {
        selectionAnchor_ = pos;
        selectionFocus_ = pos;
        cursorAffinityDownstream_ = downstream;
        clampCursorPosition();
        invalidateCursorCache();
        invalidateSelectionCache();
    }
}

// === Selection ===

void TextRenderer::setSelection(int start, int end) {
    selectionAnchor_ = start;
    selectionFocus_ = end;
    clampCursorPosition();
    cursorAffinityDownstream_ = true;
    invalidateCursorCache();
    invalidateSelectionCache();
}

void TextRenderer::clearSelection() {
    selectionAnchor_ = selectionFocus_;
    invalidateSelectionCache();
}

bool TextRenderer::hasSelection() const {
    return selectionAnchor_ != selectionFocus_;
}

std::pair<int, int> TextRenderer::getSelection() const {
    return {std::min(selectionAnchor_, selectionFocus_),
            std::max(selectionAnchor_, selectionFocus_)};
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
        selectionAnchor_ = pos;
        selectionFocus_ = pos;
        cursorAffinityDownstream_ = downstream;
        invalidateCursorCache();
        invalidateSelectionCache();
    }
}

void TextRenderer::extendSelectionToCoordinate(float x, float y) {
    int pos = 0;
    bool downstream = true;
    if (getGlyphPositionWithAffinityAtCoordinate(x, y, &pos, &downstream)) {
        selectionFocus_ = pos;
        cursorAffinityDownstream_ = downstream;
        // Anchor stays fixed, only focus moves
        invalidateCursorCache();
        invalidateSelectionCache();
    }
}

// === Colors ===

void TextRenderer::setCursorColor(Color color) {
    cursorColor_ = color;
}

Color TextRenderer::getCursorColor() const {
    return cursorColor_;
}

void TextRenderer::setSelectionColor(Color color) {
    selectionColor_ = color;
}

Color TextRenderer::getSelectionColor() const {
    return selectionColor_;
}

void TextRenderer::setScale(float scale) {
    scale_ = scale;
}

float TextRenderer::getScale() const {
    return scale_;
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
    if (!paragraph_) return false;
    layoutIfNeeded();
    auto pos = paragraph_->getGlyphPositionAtCoordinate(x, y);
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
    if (!paragraph_ || start >= end) return result;
    layoutIfNeeded();
    
    auto boxes = paragraph_->getRectsForRange(
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
    if (!paragraph_) return std::nullopt;
    layoutIfNeeded();
    
    auto range = paragraph_->getWordBoundary(position);
    return std::make_pair(static_cast<int>(range.start), static_cast<int>(range.end));
}

std::optional<std::pair<int, int>> TextRenderer::getLineBoundary(int position) const {
    rebuildParagraphIfNeeded();
    if (!paragraph_) return std::nullopt;
    layoutIfNeeded();
    
    // Get line metrics to find which line contains this position
    std::vector<LineMetrics> lineMetrics;
    paragraph_->getLineMetrics(lineMetrics);
    
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
    cachedCursorRect_.reset();
}

void TextRenderer::invalidateSelectionCache() const {
    cachedSelectionRects_.reset();
}

float TextRenderer::getDefaultCursorHeight() const {
    // Derive fallback height from default font size (1.2x line height multiplier is standard)
    return defaultStyle_.fontSize > 0.0f ? defaultStyle_.fontSize * 1.2f : 20.0f;
}

void TextRenderer::drawSelection(SkCanvas* canvas, float x, float y) const {
    auto [start, end] = getSelection();
    
    // Check if cache is valid
    if (!cachedSelectionRects_ || 
        cachedSelectionStart_ != start || 
        cachedSelectionEnd_ != end) {
        // Rebuild cache
        cachedSelectionRects_ = getRectsForRange(start, end);
        cachedSelectionStart_ = start;
        cachedSelectionEnd_ = end;
    }
    
    SkPaint paint;
    paint.setColor(selectionColor_.toARGB());
    paint.setStyle(SkPaint::kFill_Style);
    
    for (const auto& rect : *cachedSelectionRects_) {
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
    paint.setColor(cursorColor_.toARGB());
    paint.setStyle(SkPaint::kFill_Style);
    
    canvas->drawRect(cursorRect, paint);
}

SkRect TextRenderer::getCursorRect() const {
    // Check if cache is valid
    if (cachedCursorRect_ && 
        cachedCursorPosition_ == selectionFocus_ && 
        cachedCursorAffinity_ == cursorAffinityDownstream_) {
        return *cachedCursorRect_;
    }
    
    // Cache miss - compute cursor rect
    rebuildParagraphIfNeeded();
    if (!paragraph_) {
        cachedCursorRect_ = SkRect::MakeEmpty();
        return *cachedCursorRect_;
    }
    layoutIfNeeded();

    // If cursor is right after a newline, place it at the start of the next line.
    if (selectionFocus_ > 0 && selectionFocus_ <= static_cast<int>(text_.length()) &&
        text_[selectionFocus_ - 1] == u'\n') {
        std::vector<LineMetrics> lineMetrics;
        paragraph_->getLineMetrics(lineMetrics);
        int lineIndex = getLineIndexForPosition(selectionFocus_);
        if (!lineMetrics.empty() && lineIndex >= 0 && lineIndex < static_cast<int>(lineMetrics.size())) {
            const auto& line = lineMetrics[static_cast<size_t>(lineIndex)];
            float top = static_cast<float>(line.fBaseline - line.fAscent);
            float height = static_cast<float>(line.fAscent + line.fDescent);
            if (height <= 0) {
                height = static_cast<float>(line.fHeight);
            }
            float left = static_cast<float>(line.fLeft);
            cachedCursorRect_ = SkRect::MakeXYWH(left, top, kCursorWidth, height);
            cachedCursorPosition_ = selectionFocus_;
            cachedCursorAffinity_ = cursorAffinityDownstream_;
            return *cachedCursorRect_;
        }
    }
    
    int textLen = static_cast<int>(text_.length());
    if (textLen > 0) {
        int anchorIndex = selectionFocus_;
        bool useLeadingEdge = cursorAffinityDownstream_;
        if (cursorAffinityDownstream_) {
            if (anchorIndex >= textLen) {
                anchorIndex = textLen - 1;
                useLeadingEdge = false; // end of text -> use trailing edge
            }
        } else {
            anchorIndex = selectionFocus_ - 1;
            useLeadingEdge = false;
            if (anchorIndex < 0) {
                anchorIndex = 0;
                useLeadingEdge = true; // start of text -> use leading edge
            }
        }

        Paragraph::GlyphInfo info;
        if (paragraph_->getGlyphInfoAtUTF16Offset(static_cast<size_t>(anchorIndex), &info)) {
            auto boxes = paragraph_->getRectsForRange(
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
                cachedCursorRect_ = SkRect::MakeXYWH(cursorX, rect.top(), kCursorWidth, rect.height());
                cachedCursorPosition_ = selectionFocus_;
                cachedCursorAffinity_ = cursorAffinityDownstream_;
                return *cachedCursorRect_;
            }
        }
    }

    // Fallback to previous logic if glyph info isn't available
    if (selectionFocus_ > 0) {
        int clusterStart = selectionFocus_ - 1;
        int clusterEnd = selectionFocus_;
        if (getGraphemeClusterRangeAt(selectionFocus_ - 1, &clusterStart, &clusterEnd)) {
            auto boxes = paragraph_->getRectsForRange(
                clusterStart, clusterEnd,
                RectHeightStyle::kTight,
                RectWidthStyle::kTight
            );
            if (!boxes.empty()) {
                const auto& box = boxes[0].rect;
                cachedCursorRect_ = SkRect::MakeXYWH(box.right(), box.top(), kCursorWidth, box.height());
                cachedCursorPosition_ = selectionFocus_;
                cachedCursorAffinity_ = cursorAffinityDownstream_;
                return *cachedCursorRect_;
            }
        }

        auto boxes = paragraph_->getRectsForRange(
            selectionFocus_ - 1, selectionFocus_,
            RectHeightStyle::kTight,
            RectWidthStyle::kTight
        );
        if (!boxes.empty()) {
            const auto& box = boxes[0].rect;
            cachedCursorRect_ = SkRect::MakeXYWH(box.right(), box.top(), kCursorWidth, box.height());
            cachedCursorPosition_ = selectionFocus_;
            cachedCursorAffinity_ = cursorAffinityDownstream_;
            return *cachedCursorRect_;
        }
    }

    // Cursor at position 0 - use the first character's height and left edge
    if (selectionFocus_ == 0 && !text_.empty()) {
        int clusterStart = 0;
        int clusterEnd = 1;
        if (getGraphemeClusterRangeAt(0, &clusterStart, &clusterEnd)) {
            auto boxes = paragraph_->getRectsForRange(
                clusterStart, clusterEnd,
                RectHeightStyle::kTight,
                RectWidthStyle::kTight
            );
            if (!boxes.empty()) {
                const auto& box = boxes[0].rect;
                cachedCursorRect_ = SkRect::MakeXYWH(box.left(), box.top(), kCursorWidth, box.height());
                cachedCursorPosition_ = selectionFocus_;
                cachedCursorAffinity_ = cursorAffinityDownstream_;
                return *cachedCursorRect_;
            }
        }

        auto boxes = paragraph_->getRectsForRange(
            0, 1,
            RectHeightStyle::kTight,
            RectWidthStyle::kTight
        );
        if (!boxes.empty()) {
            const auto& box = boxes[0].rect;
            cachedCursorRect_ = SkRect::MakeXYWH(box.left(), box.top(), kCursorWidth, box.height());
            cachedCursorPosition_ = selectionFocus_;
            cachedCursorAffinity_ = cursorAffinityDownstream_;
            return *cachedCursorRect_;
        }
    }
    
    // Empty text - derive height from default font size
    float height = paragraph_->getHeight();
    if (height <= 0) {
        height = getDefaultCursorHeight();
    }
    cachedCursorRect_ = SkRect::MakeXYWH(0, 0, kCursorWidth, height);
    cachedCursorPosition_ = selectionFocus_;
    cachedCursorAffinity_ = cursorAffinityDownstream_;
    return *cachedCursorRect_;
}
