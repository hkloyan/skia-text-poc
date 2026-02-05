#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>

#include "core/text_editor.hpp"
#include "core/font_manager.hpp"

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

// Global state
static sk_sp<GrDirectContext> grContext;
static sk_sp<SkSurface> surface;
static std::unique_ptr<TextEditor> textEditor;
static EMSCRIPTEN_WEBGL_CONTEXT_HANDLE webglContext = 0;

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
        printf("Failed to create WebGL2 context, error: %ld\n", (long)webglContext);
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
    textEditor.reset();
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

void createTextEditor() {
    textEditor = std::make_unique<TextEditor>();
}

// Backward compatibility alias
void createTextRenderer() {
    createTextEditor();
}

void setScale(float scale) {
    if (textEditor) {
        textEditor->setScale(scale);
    }
}

void setText(const std::string& text, const std::string& fontFamily,
             float fontSize, uint32_t color, int fontWeight, bool italic, bool underline,
             float letterSpacing, float wordSpacing, uint32_t backgroundColor, bool hasBackground,
             uint32_t shadowColor, float shadowOffsetX, float shadowOffsetY, float shadowBlurSigma, bool hasShadow) {
    if (textEditor) {
        textEditor->setText(text, TextEditor::makeStyle(fontFamily, fontSize, color, fontWeight, italic, underline,
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

void beginRichText() {
    if (textEditor) textEditor->beginRichText();
}

void addStyledSpan(const std::string& text, const std::string& fontFamily,
                   float fontSize, uint32_t color, int fontWeight,
                   bool italic, bool underline, float letterSpacing, float wordSpacing,
                   uint32_t backgroundColor, bool hasBackground,
                   uint32_t shadowColor, float shadowOffsetX, float shadowOffsetY, float shadowBlurSigma, bool hasShadow) {
    if (textEditor) {
        textEditor->addStyledSpan(TextEditor::makeSpan(text, fontFamily, fontSize, color, fontWeight, italic, underline,
                                                       letterSpacing, wordSpacing, backgroundColor, hasBackground,
                                                       shadowColor, shadowOffsetX, shadowOffsetY, shadowBlurSigma, hasShadow));
    }
}

// Simple rich text span (basic style only)
void addStyledSpanSimple(const std::string& text, const std::string& fontFamily,
                         float fontSize, uint32_t color, int fontWeight, bool italic, bool underline) {
    addStyledSpan(text, fontFamily, fontSize, color, fontWeight, italic, underline,
                  0.0f, 0.0f, 0x00000000, false, 0xFF000000, 0.0f, 0.0f, 0.0f, false);
}

void endRichText() {
    if (textEditor) textEditor->endRichText();
}

void setMaxWidth(float maxWidth) {
    if (textEditor) {
        textEditor->setMaxWidth(maxWidth);
    }
}

void layoutIfNeeded() {
    if (textEditor) {
        textEditor->layoutIfNeeded();
    }
}

void render(float x, float y, bool showCursor) {
    if (surface && textEditor) {
        SkCanvas* canvas = surface->getCanvas();
        canvas->clear(SK_ColorWHITE);
        textEditor->render(canvas, x, y, showCursor);
        grContext->flush();
    }
}

// === Layout metrics ===

float getHeight() {
    return textEditor ? textEditor->getHeight() : 0;
}

float getWidth() {
    return textEditor ? textEditor->getWidth() : 0;
}

int getLineCount() {
    return textEditor ? textEditor->getLineCount() : 0;
}

float getMaxIntrinsicWidth() {
    return textEditor ? textEditor->getMaxIntrinsicWidth() : 0;
}

float getMinIntrinsicWidth() {
    return textEditor ? textEditor->getMinIntrinsicWidth() : 0;
}

int getTextLength() {
    return textEditor ? textEditor->getTextLength() : 0;
}

// === Text Content ===

std::string getText() {
    return textEditor ? textEditor->getText() : "";
}

std::string getSelectedText() {
    return textEditor ? textEditor->getSelectedText() : "";
}

// === Text Editing ===

void insertText(const std::string& text) {
    if (textEditor) textEditor->insertText(text);
}

void deleteBackward() {
    if (textEditor) textEditor->deleteBackward();
}

void deleteForward() {
    if (textEditor) textEditor->deleteForward();
}

void deleteWordBackward() {
    if (textEditor) textEditor->deleteWordBackward();
}

void deleteWordForward() {
    if (textEditor) textEditor->deleteWordForward();
}

void deleteSelection() {
    if (textEditor) textEditor->deleteSelection();
}

void insertStyledText(const std::string& text, const std::string& fontFamily, float fontSize,
                      uint32_t color, int fontWeight, bool italic, bool underline,
                      float letterSpacing, float wordSpacing, uint32_t backgroundColor, bool hasBackground,
                      uint32_t shadowColor, float shadowOffsetX, float shadowOffsetY, float shadowBlurSigma, bool hasShadow) {
    if (textEditor) {
        textEditor->insertStyledText(TextEditor::makeSpan(text, fontFamily, fontSize, color, fontWeight, italic, underline,
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
    if (textEditor) {
        textEditor->applyStyleToSelection(TextEditor::makeStyle(fontFamily, fontSize, color, fontWeight, italic, underline,
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
    if (!textEditor) return val::null();
    TextStyle style = textEditor->getStyleAtCursor();
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
    if (textEditor) textEditor->moveCursorLeft(extendSelection);
}

void moveCursorRight(bool extendSelection) {
    if (textEditor) textEditor->moveCursorRight(extendSelection);
}

void moveCursorUp(bool extendSelection) {
    if (textEditor) textEditor->moveCursorUp(extendSelection);
}

void moveCursorDown(bool extendSelection) {
    if (textEditor) textEditor->moveCursorDown(extendSelection);
}

void moveCursorToWordStart(bool extendSelection) {
    if (textEditor) textEditor->moveCursorToWordStart(extendSelection);
}

void moveCursorToWordEnd(bool extendSelection) {
    if (textEditor) textEditor->moveCursorToWordEnd(extendSelection);
}

void moveCursorToLineStart(bool extendSelection) {
    if (textEditor) textEditor->moveCursorToLineStart(extendSelection);
}

void moveCursorToLineEnd(bool extendSelection) {
    if (textEditor) textEditor->moveCursorToLineEnd(extendSelection);
}

void moveCursorToDocumentStart(bool extendSelection) {
    if (textEditor) textEditor->moveCursorToDocumentStart(extendSelection);
}

void moveCursorToDocumentEnd(bool extendSelection) {
    if (textEditor) textEditor->moveCursorToDocumentEnd(extendSelection);
}

void selectAll() {
    if (textEditor) textEditor->selectAll();
}

// === Cursor ===

void setCursorPosition(int position) {
    if (textEditor) textEditor->setCursorPosition(position);
}

int getCursorPosition() {
    return textEditor ? textEditor->getCursorPosition() : 0;
}

void setCursorPositionAtCoordinate(float x, float y) {
    if (textEditor) textEditor->setCursorPositionAtCoordinate(x, y);
}

// === Selection ===

void setSelection(int start, int end) {
    if (textEditor) textEditor->setSelection(start, end);
}

void clearSelection() {
    if (textEditor) textEditor->clearSelection();
}

bool hasSelection() {
    return textEditor ? textEditor->hasSelection() : false;
}

val getSelection() {
    if (!textEditor) return val::null();
    auto [start, end] = textEditor->getSelection();
    val result = val::object();
    result.set("start", start);
    result.set("end", end);
    return result;
}

void setWordSelectionAtCoordinate(float x, float y) {
    if (textEditor) textEditor->setWordSelectionAtCoordinate(x, y);
}

void setLineSelectionAtCoordinate(float x, float y) {
    if (textEditor) textEditor->setLineSelectionAtCoordinate(x, y);
}

void beginSelectionAtCoordinate(float x, float y) {
    if (textEditor) textEditor->beginSelectionAtCoordinate(x, y);
}

void extendSelectionToCoordinate(float x, float y) {
    if (textEditor) textEditor->extendSelectionToCoordinate(x, y);
}

// === Colors ===

void setCursorColor(uint32_t color) {
    if (textEditor) textEditor->setCursorColor(Color::fromARGB(color));
}

uint32_t getCursorColor() {
    return textEditor ? textEditor->getCursorColor().toARGB() : 0xFF000000;
}

void setSelectionColor(uint32_t color) {
    if (textEditor) textEditor->setSelectionColor(Color::fromARGB(color));
}

uint32_t getSelectionColor() {
    return textEditor ? textEditor->getSelectionColor().toARGB() : 0x400000FF;
}

// === Paragraph-level style ===

void setTextAlignment(int alignment) {
    if (textEditor) {
        textEditor->setTextAlignment(static_cast<TextAlignment>(alignment));
    }
}

void setMaxLines(int maxLines) {
    if (textEditor) {
        textEditor->setMaxLines(maxLines);
    }
}

void setEllipsis(const std::string& ellipsis) {
    if (textEditor) {
        textEditor->setEllipsis(ellipsis);
    }
}

void setLineHeight(float height) {
    if (textEditor) {
        textEditor->setLineHeight(height);
    }
}

void setStrutStyle(const std::string& fontFamily, float fontSize, float height,
                   float leading, bool forceHeight, bool heightOverride, bool halfLeading) {
    if (textEditor) {
        TextStrutStyle strut;
        strut.enabled = true;
        strut.fontFamily = fontFamily;
        strut.fontSize = fontSize;
        strut.height = height;
        strut.leading = leading;
        strut.forceHeight = forceHeight;
        strut.heightOverride = heightOverride;
        strut.halfLeading = halfLeading;
        textEditor->setStrutStyle(strut);
    }
}

void clearStrutStyle() {
    if (textEditor) {
        textEditor->clearStrutStyle();
    }
}

// === Query methods ===

int getGlyphPositionAtCoordinate(float x, float y) {
    if (!textEditor) return -1;
    auto pos = textEditor->getGlyphPositionAtCoordinate(x, y);
    return pos.value_or(-1);
}

val getRectsForRange(int start, int end) {
    val result = val::array();
    if (!textEditor) return result;
    
    auto rects = textEditor->getRectsForRange(start, end);
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
    if (!textEditor) return val::null();
    auto boundary = textEditor->getWordBoundary(position);
    if (!boundary) return val::null();
    val result = val::object();
    result.set("start", boundary->first);
    result.set("end", boundary->second);
    return result;
}

val getLineBoundary(int position) {
    if (!textEditor) return val::null();
    auto boundary = textEditor->getLineBoundary(position);
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
    function("createTextEditor", &createTextEditor);
    function("createTextRenderer", &createTextRenderer);  // Backward compat alias
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
