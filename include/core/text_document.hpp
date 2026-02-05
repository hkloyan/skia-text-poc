#pragma once

#include "core/types.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace core {

/**
 * TextDocument - Pure data storage for text and styles.
 * 
 * This class manages the text content and style runs without any Skia dependencies.
 * It uses a version counter that increments on any mutation, allowing consumers
 * to detect when they need to rebuild/relayout.
 * 
 * Text is stored internally as UTF-16 for efficient integration with Skia's
 * text shaping, but UTF-8 convenience methods are provided.
 */
class TextDocument {
public:
    TextDocument() = default;
    ~TextDocument() = default;
    
    // === Accessors ===
    const std::u16string& text() const { return _text; }
    const std::vector<StyleRun>& styleRuns() const { return _styleRuns; }
    const TextStyle& defaultStyle() const { return _defaultStyle; }
    uint64_t version() const { return _version; }
    int length() const { return static_cast<int>(_text.length()); }
    bool isEmpty() const { return _text.empty(); }
    
    // UTF-8 convenience (uses TextEncoding internally)
    std::string getText() const;
    std::string getTextInRange(int start, int end) const;
    
    // === Mutations (all increment _version) ===
    void clear();
    void setText(const std::string& text, const TextStyle& style);
    void setRichText(const std::vector<StyledSpan>& spans);
    void insertAt(int position, const std::u16string& text);
    void insertAt(int position, const std::string& utf8Text);  // convenience
    void deleteRange(int start, int end);
    void applyStyle(int start, int end, const TextStyle& style);
    
    // === Queries ===
    TextStyle getStyleAt(int position) const;
    
private:
    void incrementVersion() { ++_version; }
    void adjustStyleRunsForInsert(int position, int length);
    void adjustStyleRunsForDelete(int start, int end);
    void mergeAdjacentStyleRuns();
    void ensureStyleCoverage(int position, int length, const TextStyle& style);
    
    std::u16string _text;
    std::vector<StyleRun> _styleRuns;
    TextStyle _defaultStyle;
    uint64_t _version = 0;
};

} // namespace core
