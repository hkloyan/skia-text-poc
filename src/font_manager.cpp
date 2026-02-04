#include "core/font_manager.hpp"
#include "modules/skparagraph/include/TypefaceFontProvider.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkStream.h"
#include "include/ports/SkFontMgr_data.h"

// Platform-specific font managers for system font fallback
#if defined(__APPLE__)
#include "include/ports/SkFontMgr_mac_ct.h"
#endif

namespace core {

FontManager& FontManager::instance() {
    static FontManager instance;
    return instance;
}

void FontManager::registerFont(const std::string& name, sk_sp<SkData> data) {
    // Store the font data
    _fontData[name] = data;
    
    // Create a font manager to extract typefaces
    std::vector<sk_sp<SkData>> dataVec = { data };
    auto fontMgr = SkFontMgr_New_Custom_Data(SkSpan<sk_sp<SkData>>(dataVec.data(), dataVec.size()));
    
    if (!fontMgr) {
        printf("  Failed to create font manager\n");
        return;
    }
    
    if (fontMgr->countFamilies() > 0) {
        auto styleSet = fontMgr->createStyleSet(0);
        if (styleSet && styleSet->count() > 0) {
            auto typeface = sk_sp<SkTypeface>(styleSet->createTypeface(0));
            if (typeface) {
                _typefaces[name] = typeface;
                _fontCollectionDirty = true;
            }
        }
    }
}

sk_sp<SkTypeface> FontManager::getTypeface(const std::string& name) {
    auto it = _typefaces.find(name);
    return it != _typefaces.end() ? it->second : nullptr;
}

sk_sp<skia::textlayout::FontCollection> FontManager::getFontCollection() {
    if (!_fontCollection || _fontCollectionDirty) {
        _fontCollection = sk_make_sp<skia::textlayout::FontCollection>();
        
        // Register custom fonts (Roboto, Playfair, etc.) as the asset font manager
        auto fontProvider = sk_make_sp<skia::textlayout::TypefaceFontProvider>();
        for (const auto& [name, typeface] : _typefaces) {
            fontProvider->registerTypeface(typeface, SkString(name.c_str()));
        }
        _fontCollection->setAssetFontManager(fontProvider);
        
#if defined(__APPLE__)
        // iOS: Use CoreText for fallback - only way to render Apple Color Emoji (emjc format)
        auto systemFontMgr = SkFontMgr_New_CoreText(nullptr);
        _fontCollection->setDefaultFontManager(systemFontMgr);
#else
        // Web: Use custom fonts (Apple Color Emoji loaded via Local Font Access, or Noto)
        _fontCollection->setDefaultFontManager(fontProvider);
#endif
        
        // Enable font fallback - when a glyph isn't found in the primary font,
        // Skia will search the default font manager (system fonts on Apple, 
        // bundled fonts on other platforms)
        _fontCollection->enableFontFallback();
        
        _fontCollectionDirty = false;
    }
    return _fontCollection;
}

void FontManager::clear() {
    _typefaces.clear();
    _fontData.clear();
    _fontCollection.reset();
    _fontCollectionDirty = true;
}

} // namespace core
