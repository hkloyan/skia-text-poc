#include "core/types.hpp"

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

} // namespace core
