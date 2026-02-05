#include "core/text_document.hpp"
#include "core/text_encoding.hpp"
#include <algorithm>

namespace core {

// === UTF-8 convenience ===

std::string TextDocument::getText() const {
    return TextEncoding::toUtf8(_text);
}

std::string TextDocument::getTextInRange(int start, int end) const {
    if (start < 0 || end <= start || start >= static_cast<int>(_text.length())) {
        return "";
    }
    end = std::min(end, static_cast<int>(_text.length()));
    std::u16string slice = _text.substr(static_cast<size_t>(start),
                                        static_cast<size_t>(end - start));
    return TextEncoding::toUtf8(slice);
}

// === Mutations ===

void TextDocument::clear() {
    _text.clear();
    _styleRuns.clear();
    incrementVersion();
}

void TextDocument::setText(const std::string& text, const TextStyle& style) {
    setRichText({{text, style}});
}

void TextDocument::setRichText(const std::vector<StyledSpan>& spans) {
    _text.clear();
    _styleRuns.clear();
    
    if (spans.empty()) {
        incrementVersion();
        return;
    }
    
    _defaultStyle = spans[0].style;
    
    int currentPos = 0;
    for (const auto& span : spans) {
        std::u16string spanText = TextEncoding::toUtf16(span.text);
        int spanLen = static_cast<int>(spanText.length());
        if (spanLen > 0) {
            _text += spanText;
            _styleRuns.push_back({currentPos, currentPos + spanLen, span.style});
            currentPos += spanLen;
        }
    }
    
    mergeAdjacentStyleRuns();
    incrementVersion();
}

void TextDocument::insertAt(int position, const std::u16string& text) {
    if (text.empty()) return;
    
    int len = static_cast<int>(text.length());
    
    // Get style for inserted text (inherit from position to the left)
    TextStyle insertStyle = getStyleAt(position > 0 ? position - 1 : position);
    
    // Insert into plain text
    _text.insert(position, text);
    
    // Adjust style runs
    adjustStyleRunsForInsert(position, len);
    
    // Ensure the inserted text has style coverage
    ensureStyleCoverage(position, len, insertStyle);
    
    mergeAdjacentStyleRuns();
    incrementVersion();
}

void TextDocument::insertAt(int position, const std::string& utf8Text) {
    insertAt(position, TextEncoding::toUtf16(utf8Text));
}

void TextDocument::deleteRange(int start, int end) {
    if (start >= end || start < 0 || end > static_cast<int>(_text.length())) return;
    
    // Delete from plain text
    _text.erase(start, end - start);
    
    // Adjust style runs
    adjustStyleRunsForDelete(start, end);
    
    mergeAdjacentStyleRuns();
    incrementVersion();
}

void TextDocument::applyStyle(int start, int end, const TextStyle& style) {
    if (start >= end || start < 0 || end > static_cast<int>(_text.length())) return;
    
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
    incrementVersion();
}

// === Queries ===

TextStyle TextDocument::getStyleAt(int position) const {
    for (const auto& run : _styleRuns) {
        if (position >= run.start && position < run.end) {
            return run.style;
        }
    }
    return _defaultStyle;
}

// === Private helpers ===

void TextDocument::adjustStyleRunsForInsert(int position, int length) {
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

void TextDocument::adjustStyleRunsForDelete(int start, int end) {
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

void TextDocument::mergeAdjacentStyleRuns() {
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

void TextDocument::ensureStyleCoverage(int position, int length, const TextStyle& style) {
    // Check if the inserted range is already covered by a style run
    bool covered = false;
    for (const auto& run : _styleRuns) {
        if (run.start <= position && run.end >= position + length) {
            covered = true;
            break;
        }
    }
    
    if (!covered && length > 0) {
        // Need to add a style run for the inserted text
        StyleRun newRun{position, position + length, style};
        
        // Insert in sorted order
        auto it = std::lower_bound(_styleRuns.begin(), _styleRuns.end(), newRun,
            [](const StyleRun& a, const StyleRun& b) { return a.start < b.start; });
        _styleRuns.insert(it, newRun);
    }
}

} // namespace core
