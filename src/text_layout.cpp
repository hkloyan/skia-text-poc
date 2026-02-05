#include "core/text_layout.hpp"
#include "core/text_document.hpp"
#include "core/text_encoding.hpp"
#include "core/font_manager.hpp"
#include "modules/skparagraph/include/Paragraph.h"
#include "modules/skparagraph/include/ParagraphBuilder.h"
#include "modules/skparagraph/include/ParagraphStyle.h"
#include "modules/skparagraph/include/TextStyle.h"
#include "modules/skparagraph/include/TextShadow.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPoint.h"

using namespace skia::textlayout;

namespace core {

// === Constants ===

// Standard line height multiplier for computing fallback cursor height when no metrics available
static constexpr float kDefaultLineHeightMultiplier = 1.2f;

// Fallback line height when font size is unavailable (pixels)
static constexpr float kFallbackLineHeight = 20.0f;

// === Helper functions ===

static skia::textlayout::TextStyle toSkiaStyle(const TextStyle& style, float lineHeight) {
    skia::textlayout::TextStyle skStyle;
    
#if defined(__APPLE__)
    // iOS: CoreText handles emoji fallback automatically
    skStyle.setFontFamilies({SkString(style.fontFamily.c_str())});
#else
    // Web: Explicit emoji font fallback
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

// === TextLayout implementation ===

TextLayout::TextLayout() = default;

TextLayout::~TextLayout() = default;

TextLayout::TextLayout(TextLayout&&) noexcept = default;
TextLayout& TextLayout::operator=(TextLayout&&) noexcept = default;

// === Width ===

void TextLayout::setMaxWidth(float width) {
    if (_maxWidth != width) {
        _maxWidth = width;
        invalidateLayout();  // Only layout, NOT paragraph rebuild
    }
}

// === Paragraph style ===

void TextLayout::setTextAlignment(TextAlignment alignment) {
    if (_alignment != alignment) {
        _alignment = alignment;
        invalidateParagraph();
    }
}

void TextLayout::setMaxLines(int maxLines) {
    if (_maxLines != maxLines) {
        _maxLines = maxLines;
        invalidateParagraph();
    }
}

void TextLayout::setEllipsis(const std::string& ellipsis) {
    std::u16string ellipsis16 = TextEncoding::toUtf16(ellipsis);
    if (_ellipsis != ellipsis16) {
        _ellipsis = ellipsis16;
        invalidateParagraph();
    }
}

void TextLayout::setLineHeight(float height) {
    float newHeight = height > 0.0f ? height : 0.0f;
    if (_lineHeight != newHeight) {
        _lineHeight = newHeight;
        invalidateParagraph();
    }
}

void TextLayout::setStrutStyle(const TextStrutStyle& strut) {
    if (_strutStyle != strut) {
        _strutStyle = strut;
        invalidateParagraph();
    }
}

void TextLayout::clearStrutStyle() {
    if (_strutStyle.enabled) {
        _strutStyle.enabled = false;
        invalidateParagraph();
    }
}

// === Update from document ===

void TextLayout::updateParagraph(const TextDocument& doc) {
    if (doc.version() != _documentVersion || _needsRebuild) {
        rebuildParagraph(doc);
        _documentVersion = doc.version();
        _needsRebuild = false;
        _needsLayout = true;  // New paragraph needs layout
    }
}

// === Layout ===

void TextLayout::layoutIfNeeded() {
    if (_needsLayout && _paragraph) {
        _paragraph->layout(_maxWidth);
        _layoutWidth = _maxWidth;
        _needsLayout = false;
        ++_layoutRevision;  // Increment on ANY layout change
    }
}

void TextLayout::update(const TextDocument& doc) {
    updateParagraph(doc);
    layoutIfNeeded();
}

// === State queries ===

bool TextLayout::needsUpdate(const TextDocument& doc) const {
    return doc.version() != _documentVersion || _needsRebuild || _needsLayout;
}

bool TextLayout::isValid() const {
    return _paragraph && !_needsLayout && !_needsRebuild;
}

// === Metrics ===

float TextLayout::getHeight() const {
    if (_textLength == 0) return 0;
    return _paragraph ? _paragraph->getHeight() : 0;
}

float TextLayout::getWidth() const {
    if (_textLength == 0) return 0;
    return _paragraph ? _paragraph->getMaxWidth() : 0;
}

float TextLayout::getMaxIntrinsicWidth() const {
    if (_textLength == 0) return 0;
    return _paragraph ? _paragraph->getMaxIntrinsicWidth() : 0;
}

float TextLayout::getMinIntrinsicWidth() const {
    if (_textLength == 0) return 0;
    return _paragraph ? _paragraph->getMinIntrinsicWidth() : 0;
}

int TextLayout::getLineCount() const {
    if (_textLength == 0) return 0;
    return _paragraph ? static_cast<int>(_paragraph->lineNumber()) : 0;
}

float TextLayout::getDefaultLineHeight(float defaultFontSize) const {
    return defaultFontSize > 0.0f ? defaultFontSize * kDefaultLineHeightMultiplier : kFallbackLineHeight;
}

// === Hit testing / queries ===

std::optional<int> TextLayout::getPositionAtCoordinate(float x, float y) const {
    int pos = 0;
    bool downstream = true;
    if (!getPositionWithAffinityAtCoordinate(x, y, &pos, &downstream)) {
        return std::nullopt;
    }
    return pos;
}

bool TextLayout::getPositionWithAffinityAtCoordinate(float x, float y, int* position, bool* downstream) const {
    if (!_paragraph) return false;
    
    auto pos = _paragraph->getGlyphPositionAtCoordinate(x, y);
    if (position) {
        *position = static_cast<int>(pos.position);
    }
    if (downstream) {
        *downstream = (pos.affinity == Affinity::kDownstream);
    }
    return true;
}

std::vector<SkRect> TextLayout::getRectsForRange(int start, int end) const {
    std::vector<SkRect> result;
    if (!_paragraph || start >= end) return result;
    
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

std::optional<std::pair<int, int>> TextLayout::getWordBoundary(int position) const {
    if (!_paragraph) return std::nullopt;
    
    auto range = _paragraph->getWordBoundary(position);
    return std::make_pair(static_cast<int>(range.start), static_cast<int>(range.end));
}

std::optional<std::pair<int, int>> TextLayout::getLineBoundary(int position) const {
    if (!_paragraph) return std::nullopt;
    
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

int TextLayout::getLineIndexForPosition(int position) const {
    if (!_paragraph) return 0;
    
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

float TextLayout::getXPositionForPosition(int position, bool downstream) const {
    if (!_paragraph) return 0;
    
    // Get the rect for the character at position
    Paragraph::GlyphInfo info;
    if (_paragraph->getGlyphInfoAtUTF16Offset(static_cast<size_t>(position), &info)) {
        auto boxes = _paragraph->getRectsForRange(
            info.fGraphemeClusterTextRange.start,
            info.fGraphemeClusterTextRange.end,
            RectHeightStyle::kTight,
            RectWidthStyle::kTight
        );
        if (!boxes.empty()) {
            const auto& box = boxes[0];
            const bool isRtl = box.direction == TextDirection::kRtl;
            return downstream
                ? (isRtl ? box.rect.right() : box.rect.left())
                : (isRtl ? box.rect.left() : box.rect.right());
        }
    }
    
    return 0;
}

bool TextLayout::getGraphemeClusterRangeAt(int position, int* start, int* end) const {
    if (!_paragraph || position < 0) return false;
    
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

// === Private methods ===

void TextLayout::rebuildParagraph(const TextDocument& doc) {
    _textLength = doc.length();
    auto fontCollection = FontManager::instance().getFontCollection();
    
    // Configure paragraph style
    ParagraphStyle paragraphStyle;
    paragraphStyle.setTextAlign(toSkiaAlignment(_alignment));
    if (_maxLines > 0) {
        paragraphStyle.setMaxLines(static_cast<size_t>(_maxLines));
    }
    if (!_ellipsis.empty()) {
        paragraphStyle.setEllipsis(_ellipsis);
    }
    
    // Store default style for metrics
    _defaultStyle = doc.defaultStyle();
    
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
    
    const auto& text = doc.text();
    const auto& styleRuns = doc.styleRuns();
    
    if (text.empty()) {
        // Empty text - just build empty paragraph
        _paragraph = builder->Build();
    } else if (styleRuns.empty()) {
        // No style runs - use default style
        builder->pushStyle(defaultSkStyle);
        builder->addText(text);
        builder->pop();
        _paragraph = builder->Build();
    } else {
        // Add each style run
        for (const auto& run : styleRuns) {
            if (run.start < run.end && run.start >= 0 && run.end <= static_cast<int>(text.length())) {
                skia::textlayout::TextStyle skStyle = toSkiaStyle(run.style, _lineHeight);
                builder->pushStyle(skStyle);
                std::u16string runText = text.substr(run.start, run.end - run.start);
                builder->addText(runText);
                builder->pop();
            }
        }
        _paragraph = builder->Build();
    }
}

void TextLayout::invalidateParagraph() {
    _needsRebuild = true;
    _needsLayout = true;
}

void TextLayout::invalidateLayout() {
    _needsLayout = true;
}

} // namespace core
