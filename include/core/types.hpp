#pragma once

#include <string>
#include <cstdint>

namespace core {

// === Color ===
// Type-safe color representation (ARGB format)
struct Color {
    uint8_t a = 255;
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    
    constexpr Color() = default;
    constexpr Color(uint8_t a, uint8_t r, uint8_t g, uint8_t b) : a(a), r(r), g(g), b(b) {}
    
    // Create from ARGB uint32_t (0xAARRGGBB)
    static constexpr Color fromARGB(uint32_t argb) {
        return Color(
            static_cast<uint8_t>((argb >> 24) & 0xFF),
            static_cast<uint8_t>((argb >> 16) & 0xFF),
            static_cast<uint8_t>((argb >> 8) & 0xFF),
            static_cast<uint8_t>(argb & 0xFF)
        );
    }
    
    // Create from RGB with full alpha
    static constexpr Color fromRGB(uint8_t r, uint8_t g, uint8_t b) {
        return Color(255, r, g, b);
    }
    
    // Convert to ARGB uint32_t
    constexpr uint32_t toARGB() const {
        return (static_cast<uint32_t>(a) << 24) |
               (static_cast<uint32_t>(r) << 16) |
               (static_cast<uint32_t>(g) << 8) |
               static_cast<uint32_t>(b);
    }
    
    // Common colors
    static constexpr Color black() { return Color(255, 0, 0, 0); }
    static constexpr Color white() { return Color(255, 255, 255, 255); }
    static constexpr Color transparent() { return Color(0, 0, 0, 0); }
    static constexpr Color red() { return Color(255, 255, 0, 0); }
    static constexpr Color green() { return Color(255, 0, 255, 0); }
    static constexpr Color blue() { return Color(255, 0, 0, 255); }
    
    constexpr bool operator==(const Color& other) const {
        return a == other.a && r == other.r && g == other.g && b == other.b;
    }
    constexpr bool operator!=(const Color& other) const { return !(*this == other); }
};

// === Enums ===
enum class TextAlignment {
    Left = 0,
    Right = 1,
    Center = 2,
    Justify = 3,
    Start = 4,
    End = 5
};

// === Style structs ===
struct TextShadowStyle {
    Color color = Color::black();
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float blurSigma = 0.0f;
    
    bool operator==(const TextShadowStyle& other) const {
        return color == other.color &&
               offsetX == other.offsetX &&
               offsetY == other.offsetY &&
               blurSigma == other.blurSigma;
    }
    bool operator!=(const TextShadowStyle& other) const { return !(*this == other); }
};

struct TextStrutStyle {
    bool enabled = false;
    std::string fontFamily = "Roboto";
    float fontSize = 0.0f;
    float height = 0.0f;
    float leading = 0.0f;
    bool forceHeight = false;
    bool heightOverride = false;
    bool halfLeading = false;
    
    bool operator==(const TextStrutStyle& other) const {
        return enabled == other.enabled &&
               fontFamily == other.fontFamily &&
               fontSize == other.fontSize &&
               height == other.height &&
               leading == other.leading &&
               forceHeight == other.forceHeight &&
               heightOverride == other.heightOverride &&
               halfLeading == other.halfLeading;
    }
    bool operator!=(const TextStrutStyle& other) const { return !(*this == other); }
};

struct TextStyle;

// Builder for TextStyle - enables fluent configuration
class TextStyleBuilder {
public:
    TextStyleBuilder();
    explicit TextStyleBuilder(const TextStyle& base);
    
    TextStyleBuilder& fontFamily(const std::string& family);
    TextStyleBuilder& fontSize(float size);
    TextStyleBuilder& color(Color c);
    TextStyleBuilder& fontWeight(int weight);
    TextStyleBuilder& italic(bool value = true);
    TextStyleBuilder& underline(bool value = true);
    TextStyleBuilder& letterSpacing(float spacing);
    TextStyleBuilder& wordSpacing(float spacing);
    TextStyleBuilder& background(Color c);
    TextStyleBuilder& noBackground();
    TextStyleBuilder& shadow(Color c, float offsetX, float offsetY, float blurSigma);
    TextStyleBuilder& noShadow();
    
    TextStyle build() const;
    
private:
    std::string _fontFamily = "Roboto";
    float _fontSize = 16.0f;
    Color _color = Color::black();
    int _fontWeight = 400;
    bool _italic = false;
    bool _underline = false;
    float _letterSpacing = 0.0f;
    float _wordSpacing = 0.0f;
    bool _hasBackground = false;
    Color _backgroundColor = Color::transparent();
    bool _hasShadow = false;
    TextShadowStyle _shadow;
};

struct TextStyle {
    std::string fontFamily = "Roboto";
    float fontSize = 16.0f;
    Color color = Color::black();
    int fontWeight = 400;         // 400 = normal, 700 = bold
    bool italic = false;
    bool underline = false;
    float letterSpacing = 0.0f;
    float wordSpacing = 0.0f;
    bool hasBackground = false;
    Color backgroundColor = Color::transparent();
    bool hasShadow = false;
    TextShadowStyle shadow;
    
    bool operator==(const TextStyle& other) const {
        return fontFamily == other.fontFamily &&
               fontSize == other.fontSize &&
               color == other.color &&
               fontWeight == other.fontWeight &&
               italic == other.italic &&
               underline == other.underline &&
               letterSpacing == other.letterSpacing &&
               wordSpacing == other.wordSpacing &&
               hasBackground == other.hasBackground &&
               backgroundColor == other.backgroundColor &&
               hasShadow == other.hasShadow &&
               shadow == other.shadow;
    }
    bool operator!=(const TextStyle& other) const { return !(*this == other); }
    
    // Start building from this style
    TextStyleBuilder toBuilder() const { return TextStyleBuilder(*this); }
    
    // Static builder factory
    static TextStyleBuilder builder() { return TextStyleBuilder(); }
};

// === Span types ===
struct StyledSpan {
    std::string text;
    TextStyle style;
};

// Internal representation for editable rich text
struct StyleRun {
    int start;      // inclusive, in UTF-16 code units
    int end;        // exclusive, in UTF-16 code units
    TextStyle style;
};

} // namespace core
