#pragma once

#include "core/types.hpp"
#include "include/core/SkRect.h"
#include <memory>
#include <optional>
#include <vector>
#include <cstdint>

namespace skia::textlayout { class Paragraph; }

namespace core {

class TextDocument;

/**
 * TextLayout - Manages Skia Paragraph, layout computation, metrics, and hit testing.
 * 
 * Two-stage invalidation design:
 * - Document changes (version++) → _needsRebuild = true → rebuildParagraph()
 * - Width changes → _needsLayout = true → _paragraph->layout(width)
 * 
 * Width-only changes skip expensive paragraph construction.
 */
class TextLayout {
public:
    TextLayout();
    ~TextLayout();
    
    // Non-copyable, but movable
    TextLayout(const TextLayout&) = delete;
    TextLayout& operator=(const TextLayout&) = delete;
    TextLayout(TextLayout&&) noexcept;
    TextLayout& operator=(TextLayout&&) noexcept;
    
    // === Width (only invalidates layout, not paragraph) ===
    void setMaxWidth(float width);
    float maxWidth() const { return _maxWidth; }
    
    // === Paragraph style (invalidates paragraph rebuild) ===
    void setTextAlignment(TextAlignment alignment);
    TextAlignment textAlignment() const { return _alignment; }
    void setMaxLines(int maxLines);
    int maxLines() const { return _maxLines; }
    void setEllipsis(const std::string& ellipsis);
    void setLineHeight(float height);
    float lineHeight() const { return _lineHeight; }
    void setStrutStyle(const TextStrutStyle& strut);
    void clearStrutStyle();
    const TextStrutStyle& strutStyle() const { return _strutStyle; }
    
    // === Update from document (rebuilds paragraph if version changed) ===
    void updateParagraph(const TextDocument& doc);
    
    // === Layout (runs paragraph->layout() if needed) ===
    void layoutIfNeeded();
    
    // === Combined convenience ===
    void update(const TextDocument& doc);  // updateParagraph + layoutIfNeeded
    
    // === State queries ===
    bool needsUpdate(const TextDocument& doc) const;
    bool needsLayout() const { return _needsLayout; }
    bool isValid() const;  // True if paragraph exists and layout is current
    uint64_t documentVersion() const { return _documentVersion; }
    
    // Layout revision: increments on ANY change (rebuild OR width change)
    // Use this for cache invalidation, not _documentVersion
    uint64_t layoutRevision() const { return _layoutRevision; }
    
    // === Metrics (require valid layout) ===
    float getHeight() const;
    float getWidth() const;
    float getMaxIntrinsicWidth() const;
    float getMinIntrinsicWidth() const;
    int getLineCount() const;
    float getDefaultLineHeight(float defaultFontSize) const;  // For empty text cursor height
    
    // === Hit testing / queries (require valid layout) ===
    std::optional<int> getPositionAtCoordinate(float x, float y) const;
    bool getPositionWithAffinityAtCoordinate(float x, float y, int* pos, bool* downstream) const;
    std::vector<SkRect> getRectsForRange(int start, int end) const;
    std::optional<std::pair<int, int>> getWordBoundary(int position) const;
    std::optional<std::pair<int, int>> getLineBoundary(int position) const;
    int getLineIndexForPosition(int position) const;
    float getXPositionForPosition(int position, bool downstream) const;
    bool getGraphemeClusterRangeAt(int position, int* start, int* end) const;
    
    // === For rendering ===
    skia::textlayout::Paragraph* paragraph() const { return _paragraph.get(); }
    
private:
    void rebuildParagraph(const TextDocument& doc);
    void invalidateParagraph();  // Sets _needsRebuild = true
    void invalidateLayout();     // Sets _needsLayout = true
    
    mutable std::unique_ptr<skia::textlayout::Paragraph> _paragraph;
    
    // Version tracking
    uint64_t _documentVersion = 0;  // Last seen doc version
    uint64_t _layoutRevision = 0;   // Increments on ANY layout change (rebuild or re-layout)
    
    // Layout parameters (changes to these require paragraph rebuild)
    TextAlignment _alignment = TextAlignment::Left;
    int _maxLines = 0;
    std::u16string _ellipsis;
    float _lineHeight = 0;
    TextStrutStyle _strutStyle;
    
    // Cached default style (for metrics when text is empty)
    TextStyle _defaultStyle;
    int _textLength = 0;
    
    // Width (changes only require re-layout, not rebuild)
    float _maxWidth = 0;
    float _layoutWidth = -1;  // Last width used for layout
    
    // Dirty flags
    mutable bool _needsRebuild = true;
    mutable bool _needsLayout = true;
};

} // namespace core
