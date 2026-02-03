#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>

#include "core/TextRenderer.h"
#include "core/FontManager.h"

#include "include/core/SkSurface.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
#include "include/gpu/GrDirectContext.h"
#include "include/gpu/gl/GrGLInterface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLMakeWebGLInterface.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/GrBackendSurface.h"

using namespace core;
using namespace emscripten;

// Global state for the PoC
static sk_sp<GrDirectContext> grContext;
static sk_sp<SkSurface> surface;
static std::unique_ptr<TextRenderer> textRenderer;
static EMSCRIPTEN_WEBGL_CONTEXT_HANDLE webglContext = 0;

// Helper to construct TextStyle from parameters
static TextStyle makeTextStyle(const std::string& fontFamily, float fontSize, uint32_t color,
                               int fontWeight, bool italic, bool underline,
                               float letterSpacing, float wordSpacing,
                               uint32_t backgroundColor, bool hasBackground,
                               uint32_t shadowColor, float shadowOffsetX, float shadowOffsetY,
                               float shadowBlurSigma, bool hasShadow) {
    TextStyle style;
    style.fontFamily = fontFamily;
    style.fontSize = fontSize;
    style.color = Color::fromARGB(color);
    style.fontWeight = fontWeight;
    style.italic = italic;
    style.underline = underline;
    style.letterSpacing = letterSpacing;
    style.wordSpacing = wordSpacing;
    style.hasBackground = hasBackground;
    style.backgroundColor = Color::fromARGB(backgroundColor);
    style.hasShadow = hasShadow;
    style.shadow.color = Color::fromARGB(shadowColor);
    style.shadow.offsetX = shadowOffsetX;
    style.shadow.offsetY = shadowOffsetY;
    style.shadow.blurSigma = shadowBlurSigma;
    return style;
}

// Helper to construct StyledSpan from parameters
static StyledSpan makeStyledSpan(const std::string& text, const std::string& fontFamily, float fontSize,
                                 uint32_t color, int fontWeight, bool italic, bool underline,
                                 float letterSpacing, float wordSpacing,
                                 uint32_t backgroundColor, bool hasBackground,
                                 uint32_t shadowColor, float shadowOffsetX, float shadowOffsetY,
                                 float shadowBlurSigma, bool hasShadow) {
    StyledSpan span;
    span.text = text;
    span.style = makeTextStyle(fontFamily, fontSize, color, fontWeight, italic, underline,
                               letterSpacing, wordSpacing, backgroundColor, hasBackground,
                               shadowColor, shadowOffsetX, shadowOffsetY, shadowBlurSigma, hasShadow);
    return span;
}

bool initSkia(int width, int height) {
    // Create WebGL2 context using Emscripten APIs
    EmscriptenWebGLContextAttributes attrs;
    emscripten_webgl_init_context_attributes(&attrs);
    attrs.majorVersion = 2;  // WebGL 2.0
    attrs.minorVersion = 0;
    attrs.depth = true;
    attrs.stencil = true;
    attrs.antialias = true;
    attrs.premultipliedAlpha = true;
    attrs.preserveDrawingBuffer = true;
    attrs.powerPreference = EM_WEBGL_POWER_PREFERENCE_HIGH_PERFORMANCE;
    
    webglContext = emscripten_webgl_create_context("#canvas", &attrs);
    if (webglContext <= 0) {
        printf("Failed to create WebGL2 context, error: %d\n", webglContext);
        return false;
    }
    
    EMSCRIPTEN_RESULT res = emscripten_webgl_make_context_current(webglContext);
    if (res != EMSCRIPTEN_RESULT_SUCCESS) {
        printf("Failed to make WebGL context current, error: %d\n", res);
        return false;
    }
    
    printf("WebGL2 context created successfully\n");
    
    // Create the WebGL interface
    auto interface = GrGLInterfaces::MakeWebGL();
    if (!interface) {
        printf("Failed to create WebGL interface\n");
        return false;
    }
    
    // Create the Skia GPU context
    grContext = GrDirectContexts::MakeGL(interface);
    if (!grContext) {
        printf("Failed to create GrDirectContext\n");
        return false;
    }
    
    // Create the render target from the default framebuffer
    GrGLFramebufferInfo fbInfo;
    fbInfo.fFBOID = 0;  // Default framebuffer
    fbInfo.fFormat = GL_RGBA8;
    
    auto backendRT = GrBackendRenderTargets::MakeGL(width, height, 0, 0, fbInfo);
    
    // Create the surface
    surface = SkSurfaces::WrapBackendRenderTarget(
        grContext.get(),
        backendRT,
        kBottomLeft_GrSurfaceOrigin,
        kRGBA_8888_SkColorType,
        nullptr,
        nullptr
    );
    
    if (!surface) {
        printf("Failed to create SkSurface\n");
        return false;
    }
    
    printf("Skia initialized successfully (WebGL): %dx%d\n", width, height);
    return true;
}

void destroySkia() {
    textRenderer.reset();
    surface.reset();
    grContext.reset();
    if (webglContext > 0) {
        emscripten_webgl_destroy_context(webglContext);
        webglContext = 0;
    }
    printf("Skia resources released\n");
}

void registerFont(const std::string& name, uintptr_t dataPtr, size_t dataSize) {
    // Data is passed directly from JavaScript via HEAPU8
    const uint8_t* data = reinterpret_cast<const uint8_t*>(dataPtr);
    auto skData = SkData::MakeWithCopy(data, dataSize);
    FontManager::instance().registerFont(name, skData);
}

void createTextRenderer() {
    textRenderer = std::make_unique<TextRenderer>();
}

void setScale(float scale) {
    if (textRenderer) {
        textRenderer->setScale(scale);
    }
}

void setText(const std::string& text, const std::string& fontFamily,
             float fontSize, uint32_t color, int fontWeight, bool italic, bool underline,
             float letterSpacing, float wordSpacing, uint32_t backgroundColor, bool hasBackground,
             uint32_t shadowColor, float shadowOffsetX, float shadowOffsetY, float shadowBlurSigma, bool hasShadow) {
    if (textRenderer) {
        textRenderer->setText(text, makeTextStyle(fontFamily, fontSize, color, fontWeight, italic, underline,
                                                  letterSpacing, wordSpacing, backgroundColor, hasBackground,
                                                  shadowColor, shadowOffsetX, shadowOffsetY, shadowBlurSigma, hasShadow));
    }
}

// Simple text API (basic style only)
void setTextSimple(const std::string& text, const std::string& fontFamily,
                   float fontSize, uint32_t color, int fontWeight, bool italic, bool underline) {
    setText(text, fontFamily, fontSize, color, fontWeight, italic, underline,
            0.0f, 0.0f, 0x00000000, false, 0xFF000000, 0.0f, 0.0f, 0.0f, false);
}

// Add a styled span for rich text
static std::vector<StyledSpan> richTextSpans;

void beginRichText() {
    richTextSpans.clear();
}

void addStyledSpan(const std::string& text, const std::string& fontFamily,
                   float fontSize, uint32_t color, int fontWeight,
                   bool italic, bool underline, float letterSpacing, float wordSpacing,
                   uint32_t backgroundColor, bool hasBackground,
                   uint32_t shadowColor, float shadowOffsetX, float shadowOffsetY, float shadowBlurSigma, bool hasShadow) {
    richTextSpans.push_back(makeStyledSpan(text, fontFamily, fontSize, color, fontWeight, italic, underline,
                                           letterSpacing, wordSpacing, backgroundColor, hasBackground,
                                           shadowColor, shadowOffsetX, shadowOffsetY, shadowBlurSigma, hasShadow));
}

// Simple rich text span (basic style only)
void addStyledSpanSimple(const std::string& text, const std::string& fontFamily,
                         float fontSize, uint32_t color, int fontWeight, bool italic, bool underline) {
    addStyledSpan(text, fontFamily, fontSize, color, fontWeight, italic, underline,
                  0.0f, 0.0f, 0x00000000, false, 0xFF000000, 0.0f, 0.0f, 0.0f, false);
}

void endRichText() {
    if (textRenderer && !richTextSpans.empty()) {
        textRenderer->setRichText(richTextSpans);
    }
}

void setMaxWidth(float maxWidth) {
    if (textRenderer) {
        textRenderer->setMaxWidth(maxWidth);
    }
}

void layoutIfNeeded() {
    if (textRenderer) {
        textRenderer->layoutIfNeeded();
    }
}

void render(float x, float y, bool showCursor) {
    if (surface && textRenderer) {
        SkCanvas* canvas = surface->getCanvas();
        canvas->clear(SK_ColorWHITE);
        textRenderer->render(canvas, x, y, showCursor);
        grContext->flush();
    }
}

// === Layout metrics ===

float getHeight() {
    return textRenderer ? textRenderer->getHeight() : 0;
}

float getWidth() {
    return textRenderer ? textRenderer->getWidth() : 0;
}

int getLineCount() {
    return textRenderer ? textRenderer->getLineCount() : 0;
}

float getMaxIntrinsicWidth() {
    return textRenderer ? textRenderer->getMaxIntrinsicWidth() : 0;
}

float getMinIntrinsicWidth() {
    return textRenderer ? textRenderer->getMinIntrinsicWidth() : 0;
}

int getTextLength() {
    return textRenderer ? textRenderer->getTextLength() : 0;
}

// === Text Content ===

std::string getText() {
    return textRenderer ? textRenderer->getText() : "";
}

std::string getSelectedText() {
    return textRenderer ? textRenderer->getSelectedText() : "";
}

// === Text Editing ===

void insertText(const std::string& text) {
    if (textRenderer) textRenderer->insertText(text);
}

void deleteBackward() {
    if (textRenderer) textRenderer->deleteBackward();
}

void deleteForward() {
    if (textRenderer) textRenderer->deleteForward();
}

void deleteWordBackward() {
    if (textRenderer) textRenderer->deleteWordBackward();
}

void deleteWordForward() {
    if (textRenderer) textRenderer->deleteWordForward();
}

void deleteSelection() {
    if (textRenderer) textRenderer->deleteSelection();
}

void insertStyledText(const std::string& text, const std::string& fontFamily, float fontSize,
                      uint32_t color, int fontWeight, bool italic, bool underline,
                      float letterSpacing, float wordSpacing, uint32_t backgroundColor, bool hasBackground,
                      uint32_t shadowColor, float shadowOffsetX, float shadowOffsetY, float shadowBlurSigma, bool hasShadow) {
    if (textRenderer) {
        textRenderer->insertStyledText(makeStyledSpan(text, fontFamily, fontSize, color, fontWeight, italic, underline,
                                                      letterSpacing, wordSpacing, backgroundColor, hasBackground,
                                                      shadowColor, shadowOffsetX, shadowOffsetY, shadowBlurSigma, hasShadow));
    }
}

// Simple styled insert (basic style only)
void insertStyledTextSimple(const std::string& text, const std::string& fontFamily, float fontSize,
                            uint32_t color, int fontWeight, bool italic, bool underline) {
    insertStyledText(text, fontFamily, fontSize, color, fontWeight, italic, underline,
                     0.0f, 0.0f, 0x00000000, false, 0xFF000000, 0.0f, 0.0f, 0.0f, false);
}

void applyStyleToSelection(const std::string& fontFamily, float fontSize,
                           uint32_t color, int fontWeight, bool italic, bool underline,
                           float letterSpacing, float wordSpacing, uint32_t backgroundColor, bool hasBackground,
                           uint32_t shadowColor, float shadowOffsetX, float shadowOffsetY, float shadowBlurSigma, bool hasShadow) {
    if (textRenderer) {
        textRenderer->applyStyleToSelection(makeTextStyle(fontFamily, fontSize, color, fontWeight, italic, underline,
                                                          letterSpacing, wordSpacing, backgroundColor, hasBackground,
                                                          shadowColor, shadowOffsetX, shadowOffsetY, shadowBlurSigma, hasShadow));
    }
}

// Simple style apply (basic style only)
void applyStyleToSelectionSimple(const std::string& fontFamily, float fontSize,
                                 uint32_t color, int fontWeight, bool italic, bool underline) {
    applyStyleToSelection(fontFamily, fontSize, color, fontWeight, italic, underline,
                          0.0f, 0.0f, 0x00000000, false, 0xFF000000, 0.0f, 0.0f, 0.0f, false);
}

val getStyleAtCursor() {
    if (!textRenderer) return val::null();
    TextStyle style = textRenderer->getStyleAtCursor();
    val result = val::object();
    result.set("fontFamily", style.fontFamily);
    result.set("fontSize", style.fontSize);
    result.set("color", style.color.toARGB());
    result.set("fontWeight", style.fontWeight);
    result.set("italic", style.italic);
    result.set("underline", style.underline);
    result.set("letterSpacing", style.letterSpacing);
    result.set("wordSpacing", style.wordSpacing);
    result.set("hasBackground", style.hasBackground);
    result.set("backgroundColor", style.backgroundColor.toARGB());
    result.set("hasShadow", style.hasShadow);
    result.set("shadowColor", style.shadow.color.toARGB());
    result.set("shadowOffsetX", style.shadow.offsetX);
    result.set("shadowOffsetY", style.shadow.offsetY);
    result.set("shadowBlurSigma", style.shadow.blurSigma);
    return result;
}

// === Cursor Navigation ===

void moveCursorLeft(bool extendSelection) {
    if (textRenderer) textRenderer->moveCursorLeft(extendSelection);
}

void moveCursorRight(bool extendSelection) {
    if (textRenderer) textRenderer->moveCursorRight(extendSelection);
}

void moveCursorUp(bool extendSelection) {
    if (textRenderer) textRenderer->moveCursorUp(extendSelection);
}

void moveCursorDown(bool extendSelection) {
    if (textRenderer) textRenderer->moveCursorDown(extendSelection);
}

void moveCursorToWordStart(bool extendSelection) {
    if (textRenderer) textRenderer->moveCursorToWordStart(extendSelection);
}

void moveCursorToWordEnd(bool extendSelection) {
    if (textRenderer) textRenderer->moveCursorToWordEnd(extendSelection);
}

void moveCursorToLineStart(bool extendSelection) {
    if (textRenderer) textRenderer->moveCursorToLineStart(extendSelection);
}

void moveCursorToLineEnd(bool extendSelection) {
    if (textRenderer) textRenderer->moveCursorToLineEnd(extendSelection);
}

void moveCursorToDocumentStart(bool extendSelection) {
    if (textRenderer) textRenderer->moveCursorToDocumentStart(extendSelection);
}

void moveCursorToDocumentEnd(bool extendSelection) {
    if (textRenderer) textRenderer->moveCursorToDocumentEnd(extendSelection);
}

void selectAll() {
    if (textRenderer) textRenderer->selectAll();
}

// === Cursor ===

void setCursorPosition(int position) {
    if (textRenderer) textRenderer->setCursorPosition(position);
}

int getCursorPosition() {
    return textRenderer ? textRenderer->getCursorPosition() : 0;
}

void setCursorPositionAtCoordinate(float x, float y) {
    if (textRenderer) textRenderer->setCursorPositionAtCoordinate(x, y);
}

// === Selection ===

void setSelection(int start, int end) {
    if (textRenderer) textRenderer->setSelection(start, end);
}

void clearSelection() {
    if (textRenderer) textRenderer->clearSelection();
}

bool hasSelection() {
    return textRenderer ? textRenderer->hasSelection() : false;
}

val getSelection() {
    if (!textRenderer) return val::null();
    auto [start, end] = textRenderer->getSelection();
    val result = val::object();
    result.set("start", start);
    result.set("end", end);
    return result;
}

void setWordSelectionAtCoordinate(float x, float y) {
    if (textRenderer) textRenderer->setWordSelectionAtCoordinate(x, y);
}

void setLineSelectionAtCoordinate(float x, float y) {
    if (textRenderer) textRenderer->setLineSelectionAtCoordinate(x, y);
}

void beginSelectionAtCoordinate(float x, float y) {
    if (textRenderer) textRenderer->beginSelectionAtCoordinate(x, y);
}

void extendSelectionToCoordinate(float x, float y) {
    if (textRenderer) textRenderer->extendSelectionToCoordinate(x, y);
}

// === Colors ===

void setCursorColor(uint32_t color) {
    if (textRenderer) textRenderer->setCursorColor(Color::fromARGB(color));
}

uint32_t getCursorColor() {
    return textRenderer ? textRenderer->getCursorColor().toARGB() : 0xFF000000;
}

void setSelectionColor(uint32_t color) {
    if (textRenderer) textRenderer->setSelectionColor(Color::fromARGB(color));
}

uint32_t getSelectionColor() {
    return textRenderer ? textRenderer->getSelectionColor().toARGB() : 0x400000FF;
}

// === Paragraph-level style ===

void setTextAlignment(int alignment) {
    if (textRenderer) {
        textRenderer->setTextAlignment(static_cast<TextAlignment>(alignment));
    }
}

void setMaxLines(int maxLines) {
    if (textRenderer) {
        textRenderer->setMaxLines(maxLines);
    }
}

void setEllipsis(const std::string& ellipsis) {
    if (textRenderer) {
        textRenderer->setEllipsis(ellipsis);
    }
}

void setLineHeight(float height) {
    if (textRenderer) {
        textRenderer->setLineHeight(height);
    }
}

void setStrutStyle(const std::string& fontFamily, float fontSize, float height,
                   float leading, bool forceHeight, bool heightOverride, bool halfLeading) {
    if (textRenderer) {
        TextStrutStyle strut;
        strut.enabled = true;
        strut.fontFamily = fontFamily;
        strut.fontSize = fontSize;
        strut.height = height;
        strut.leading = leading;
        strut.forceHeight = forceHeight;
        strut.heightOverride = heightOverride;
        strut.halfLeading = halfLeading;
        textRenderer->setStrutStyle(strut);
    }
}

void clearStrutStyle() {
    if (textRenderer) {
        textRenderer->clearStrutStyle();
    }
}

// === Query methods ===

int getGlyphPositionAtCoordinate(float x, float y) {
    if (!textRenderer) return -1;
    auto pos = textRenderer->getGlyphPositionAtCoordinate(x, y);
    return pos.value_or(-1);
}

val getRectsForRange(int start, int end) {
    val result = val::array();
    if (!textRenderer) return result;
    
    auto rects = textRenderer->getRectsForRange(start, end);
    for (size_t i = 0; i < rects.size(); ++i) {
        val rect = val::object();
        rect.set("left", rects[i].left());
        rect.set("top", rects[i].top());
        rect.set("right", rects[i].right());
        rect.set("bottom", rects[i].bottom());
        result.set(i, rect);
    }
    return result;
}

val getWordBoundary(int position) {
    if (!textRenderer) return val::null();
    auto boundary = textRenderer->getWordBoundary(position);
    if (!boundary) return val::null();
    val result = val::object();
    result.set("start", boundary->first);
    result.set("end", boundary->second);
    return result;
}

val getLineBoundary(int position) {
    if (!textRenderer) return val::null();
    auto boundary = textRenderer->getLineBoundary(position);
    if (!boundary) return val::null();
    val result = val::object();
    result.set("start", boundary->first);
    result.set("end", boundary->second);
    return result;
}

EMSCRIPTEN_BINDINGS(skia_text) {
    function("initSkia", &initSkia);
    function("destroySkia", &destroySkia);
    function("registerFont", &registerFont, allow_raw_pointers());
    function("createTextRenderer", &createTextRenderer);
    function("setScale", &setScale);
    function("setText", &setText);
    function("setTextSimple", &setTextSimple);
    function("beginRichText", &beginRichText);
    function("addStyledSpan", &addStyledSpan);
    function("addStyledSpanSimple", &addStyledSpanSimple);
    function("endRichText", &endRichText);
    function("setMaxWidth", &setMaxWidth);
    function("layoutIfNeeded", &layoutIfNeeded);
    function("render", &render);
    
    // Layout metrics
    function("getHeight", &getHeight);
    function("getWidth", &getWidth);
    function("getLineCount", &getLineCount);
    function("getMaxIntrinsicWidth", &getMaxIntrinsicWidth);
    function("getMinIntrinsicWidth", &getMinIntrinsicWidth);
    function("getTextLength", &getTextLength);
    
    // Text content
    function("getText", &getText);
    function("getSelectedText", &getSelectedText);
    
    // Text editing
    function("insertText", &insertText);
    function("insertStyledText", &insertStyledText);
    function("insertStyledTextSimple", &insertStyledTextSimple);
    function("deleteBackward", &deleteBackward);
    function("deleteForward", &deleteForward);
    function("deleteWordBackward", &deleteWordBackward);
    function("deleteWordForward", &deleteWordForward);
    function("deleteSelection", &deleteSelection);
    
    // Text styling
    function("applyStyleToSelection", &applyStyleToSelection);
    function("applyStyleToSelectionSimple", &applyStyleToSelectionSimple);
    function("getStyleAtCursor", &getStyleAtCursor);
    
    // Cursor navigation
    function("moveCursorLeft", &moveCursorLeft);
    function("moveCursorRight", &moveCursorRight);
    function("moveCursorUp", &moveCursorUp);
    function("moveCursorDown", &moveCursorDown);
    function("moveCursorToWordStart", &moveCursorToWordStart);
    function("moveCursorToWordEnd", &moveCursorToWordEnd);
    function("moveCursorToLineStart", &moveCursorToLineStart);
    function("moveCursorToLineEnd", &moveCursorToLineEnd);
    function("moveCursorToDocumentStart", &moveCursorToDocumentStart);
    function("moveCursorToDocumentEnd", &moveCursorToDocumentEnd);
    function("selectAll", &selectAll);
    
    // Cursor
    function("setCursorPosition", &setCursorPosition);
    function("getCursorPosition", &getCursorPosition);
    function("setCursorPositionAtCoordinate", &setCursorPositionAtCoordinate);
    
    // Selection
    function("setSelection", &setSelection);
    function("clearSelection", &clearSelection);
    function("hasSelection", &hasSelection);
    function("getSelection", &getSelection);
    function("setWordSelectionAtCoordinate", &setWordSelectionAtCoordinate);
    function("setLineSelectionAtCoordinate", &setLineSelectionAtCoordinate);
    function("beginSelectionAtCoordinate", &beginSelectionAtCoordinate);
    function("extendSelectionToCoordinate", &extendSelectionToCoordinate);
    
    // Colors
    function("setCursorColor", &setCursorColor);
    function("getCursorColor", &getCursorColor);
    function("setSelectionColor", &setSelectionColor);
    function("getSelectionColor", &getSelectionColor);

    // Paragraph-level style
    function("setTextAlignment", &setTextAlignment);
    function("setMaxLines", &setMaxLines);
    function("setEllipsis", &setEllipsis);
    function("setLineHeight", &setLineHeight);
    function("setStrutStyle", &setStrutStyle);
    function("clearStrutStyle", &clearStrutStyle);
    
    // Query methods
    function("getGlyphPositionAtCoordinate", &getGlyphPositionAtCoordinate);
    function("getRectsForRange", &getRectsForRange);
    function("getWordBoundary", &getWordBoundary);
    function("getLineBoundary", &getLineBoundary);
}
