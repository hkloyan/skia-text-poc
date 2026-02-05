#include "core/text_drawing.hpp"
#include "core/text_layout.hpp"
#include "modules/skparagraph/include/Paragraph.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"

using namespace skia::textlayout;

namespace core {

void TextDrawing::drawText(
    SkCanvas* canvas,
    const TextLayout& layout,
    float x, float y)
{
    if (layout.paragraph()) {
        layout.paragraph()->paint(canvas, x, y);
    }
}

void TextDrawing::drawSelection(
    SkCanvas* canvas,
    const TextLayout& layout,
    float x, float y,
    int start, int end,
    const SelectionStyle& style)
{
    auto rects = layout.getRectsForRange(start, end);
    if (rects.empty()) return;
    
    SkPaint paint;
    paint.setColor(style.color.toARGB());
    paint.setStyle(SkPaint::kFill_Style);
    
    for (const auto& rect : rects) {
        SkRect offsetRect = rect.makeOffset(x, y);
        canvas->drawRect(offsetRect, paint);
    }
}

void TextDrawing::drawCursor(
    SkCanvas* canvas,
    float x, float y,
    const SkRect& cursorRect,
    const CursorStyle& style)
{
    if (cursorRect.isEmpty()) return;
    
    SkRect offsetRect = cursorRect.makeOffset(x, y);
    
    SkPaint paint;
    paint.setColor(style.color.toARGB());
    paint.setStyle(SkPaint::kFill_Style);
    
    canvas->drawRect(offsetRect, paint);
}

SkRect TextDrawing::computeCursorRect(
    const TextLayout& layout,
    const std::u16string& text,
    int position, bool downstream,
    float cursorWidth,
    float defaultFontSize)
{
    auto* paragraph = layout.paragraph();
    if (!paragraph) {
        return SkRect::MakeEmpty();
    }
    
    int textLen = static_cast<int>(text.length());
    
    // If cursor is right after a newline, place it at the start of the next line
    if (position > 0 && position <= textLen && text[position - 1] == u'\n') {
        std::vector<LineMetrics> lineMetrics;
        paragraph->getLineMetrics(lineMetrics);
        int lineIndex = layout.getLineIndexForPosition(position);
        if (!lineMetrics.empty() && lineIndex >= 0 && lineIndex < static_cast<int>(lineMetrics.size())) {
            const auto& line = lineMetrics[static_cast<size_t>(lineIndex)];
            float top = static_cast<float>(line.fBaseline - line.fAscent);
            float height = static_cast<float>(line.fAscent + line.fDescent);
            if (height <= 0) {
                height = static_cast<float>(line.fHeight);
            }
            float left = static_cast<float>(line.fLeft);
            return SkRect::MakeXYWH(left, top, cursorWidth, height);
        }
    }
    
    if (textLen > 0) {
        int anchorIndex = position;
        bool useLeadingEdge = downstream;
        
        if (downstream) {
            if (anchorIndex >= textLen) {
                anchorIndex = textLen - 1;
                useLeadingEdge = false;  // end of text -> use trailing edge
            }
        } else {
            anchorIndex = position - 1;
            useLeadingEdge = false;
            if (anchorIndex < 0) {
                anchorIndex = 0;
                useLeadingEdge = true;  // start of text -> use leading edge
            }
        }
        
        Paragraph::GlyphInfo info;
        if (paragraph->getGlyphInfoAtUTF16Offset(static_cast<size_t>(anchorIndex), &info)) {
            auto boxes = paragraph->getRectsForRange(
                info.fGraphemeClusterTextRange.start,
                info.fGraphemeClusterTextRange.end,
                RectHeightStyle::kTight,
                RectWidthStyle::kTight
            );
            if (!boxes.empty()) {
                const auto& box = boxes[0];
                const SkRect& rect = box.rect;
                const bool isRtl = box.direction == TextDirection::kRtl;
                float cursorX = useLeadingEdge
                    ? (isRtl ? rect.right() : rect.left())
                    : (isRtl ? rect.left() : rect.right());
                return SkRect::MakeXYWH(cursorX, rect.top(), cursorWidth, rect.height());
            }
        }
    }
    
    // Fallback: cursor at position 0 with empty or no glyph info
    if (position == 0 && !text.empty()) {
        int clusterStart = 0;
        int clusterEnd = 1;
        if (layout.getGraphemeClusterRangeAt(0, &clusterStart, &clusterEnd)) {
            auto boxes = paragraph->getRectsForRange(
                clusterStart, clusterEnd,
                RectHeightStyle::kTight,
                RectWidthStyle::kTight
            );
            if (!boxes.empty()) {
                const auto& box = boxes[0].rect;
                return SkRect::MakeXYWH(box.left(), box.top(), cursorWidth, box.height());
            }
        }
        
        auto boxes = paragraph->getRectsForRange(
            0, 1,
            RectHeightStyle::kTight,
            RectWidthStyle::kTight
        );
        if (!boxes.empty()) {
            const auto& box = boxes[0].rect;
            return SkRect::MakeXYWH(box.left(), box.top(), cursorWidth, box.height());
        }
    }
    
    // Fallback for position > 0
    if (position > 0) {
        int clusterStart = position - 1;
        int clusterEnd = position;
        if (layout.getGraphemeClusterRangeAt(position - 1, &clusterStart, &clusterEnd)) {
            auto boxes = paragraph->getRectsForRange(
                clusterStart, clusterEnd,
                RectHeightStyle::kTight,
                RectWidthStyle::kTight
            );
            if (!boxes.empty()) {
                const auto& box = boxes[0].rect;
                return SkRect::MakeXYWH(box.right(), box.top(), cursorWidth, box.height());
            }
        }
        
        auto boxes = paragraph->getRectsForRange(
            position - 1, position,
            RectHeightStyle::kTight,
            RectWidthStyle::kTight
        );
        if (!boxes.empty()) {
            const auto& box = boxes[0].rect;
            return SkRect::MakeXYWH(box.right(), box.top(), cursorWidth, box.height());
        }
    }
    
    // Empty text - derive height from default font size
    float height = paragraph->getHeight();
    if (height <= 0) {
        height = layout.getDefaultLineHeight(defaultFontSize);
    }
    return SkRect::MakeXYWH(0, 0, cursorWidth, height);
}

} // namespace core
