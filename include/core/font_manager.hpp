#pragma once

#include "include/core/SkFontMgr.h"
#include "include/core/SkTypeface.h"
#include "include/core/SkData.h"
#include "modules/skparagraph/include/FontCollection.h"
#include <string>
#include <unordered_map>

namespace core {

class FontManager {
public:
    static FontManager& instance();
    
    // Called at startup with font bytes loaded by platform code
    void registerFont(const std::string& name, sk_sp<SkData> data);
    
    // Get a registered typeface by name
    sk_sp<SkTypeface> getTypeface(const std::string& name);
    
    // Get the font collection for SkParagraph
    sk_sp<skia::textlayout::FontCollection> getFontCollection();
    
    // Clear all registered fonts (useful for cleanup)
    void clear();

private:
    FontManager() = default;
    ~FontManager() = default;
    
    // Prevent copying
    FontManager(const FontManager&) = delete;
    FontManager& operator=(const FontManager&) = delete;
    
    std::unordered_map<std::string, sk_sp<SkData>> _fontData;
    std::unordered_map<std::string, sk_sp<SkTypeface>> _typefaces;
    sk_sp<skia::textlayout::FontCollection> _fontCollection;
    bool _fontCollectionDirty = true;
};

} // namespace core
