#import "SkiaRenderer.hh"
#include "core/TextRenderer.h"
#include "core/FontManager.h"

#include "include/core/SkCanvas.h"
#include "include/core/SkSurface.h"
#include "include/core/SkColorSpace.h"
#include "include/gpu/GrDirectContext.h"
#include "include/gpu/ganesh/mtl/GrMtlDirectContext.h"
#include "include/gpu/ganesh/mtl/GrMtlBackendContext.h"
#include "include/gpu/ganesh/mtl/GrMtlTypes.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/mtl/GrMtlBackendSurface.h"
#include "include/gpu/GrBackendSurface.h"
#include "include/ports/SkFontMgr_data.h"

using namespace core;

// Helper to construct TextStyle from ObjC parameters
static TextStyle makeTextStyle(NSString* fontFamily, float fontSize, uint32_t color,
                               int fontWeight, BOOL italic, BOOL underline,
                               float letterSpacing, float wordSpacing,
                               uint32_t backgroundColor, BOOL hasBackground,
                               uint32_t shadowColor, float shadowOffsetX, float shadowOffsetY,
                               float shadowBlurSigma, BOOL hasShadow) {
    TextStyle style;
    style.fontFamily = [fontFamily UTF8String];
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

// Helper to construct StyledSpan from ObjC parameters
static StyledSpan makeStyledSpan(NSString* text, NSString* fontFamily, float fontSize, uint32_t color,
                                  int fontWeight, BOOL italic, BOOL underline,
                                  float letterSpacing, float wordSpacing,
                                  uint32_t backgroundColor, BOOL hasBackground,
                                  uint32_t shadowColor, float shadowOffsetX, float shadowOffsetY,
                                  float shadowBlurSigma, BOOL hasShadow) {
    StyledSpan span;
    span.text = [text UTF8String];
    span.style = makeTextStyle(fontFamily, fontSize, color, fontWeight, italic, underline,
                               letterSpacing, wordSpacing, backgroundColor, hasBackground,
                               shadowColor, shadowOffsetX, shadowOffsetY, shadowBlurSigma, hasShadow);
    return span;
}

@implementation SkiaRenderer {
    id<MTLDevice> _device;
    id<MTLCommandQueue> _commandQueue;
    sk_sp<GrDirectContext> _grContext;
    std::unique_ptr<TextRenderer> _textRenderer;
    std::vector<StyledSpan> _richTextSpans;
    float _renderX;
    float _renderY;
    BOOL _showCursor;
}

- (instancetype)initWithMetalDevice:(id<MTLDevice>)device {
    self = [super init];
    if (self) {
        _device = device;
        _commandQueue = [device newCommandQueue];
        
        // Create Skia GPU context with Metal backend
        GrMtlBackendContext backendContext = {};
        backendContext.fDevice.retain((__bridge void*)_device);
        backendContext.fQueue.retain((__bridge void*)_commandQueue);
        _grContext = GrDirectContexts::MakeMetal(backendContext);
        
        if (_grContext) {
            NSLog(@"Skia GrDirectContext created successfully");
        } else {
            NSLog(@"ERROR: Failed to create Skia GrDirectContext");
        }
        
        _textRenderer = std::make_unique<TextRenderer>();
        _renderX = 50.0f;  // Default position
        _renderY = 50.0f;
    }
    return self;
}

- (void)setRenderPosition:(float)x y:(float)y {
    _renderX = x;
    _renderY = y;
}

- (void)loadFonts {
    // Helper to load a font
    void (^loadFont)(NSString*, NSString*) = ^(NSString* name, NSString* filename) {
        NSString* fontPath = [[NSBundle mainBundle] pathForResource:filename 
                                                             ofType:@"ttf" 
                                                        inDirectory:@"fonts"];
        if (!fontPath) {
            fontPath = [[NSBundle mainBundle] pathForResource:filename ofType:@"ttf"];
        }
        
        if (fontPath) {
            NSData* fontData = [NSData dataWithContentsOfFile:fontPath];
            auto skData = SkData::MakeWithCopy(fontData.bytes, fontData.length);
            FontManager::instance().registerFont([name UTF8String], skData);
            NSLog(@"Registered font: %@ (%lu bytes)", name, (unsigned long)fontData.length);
        } else {
            NSLog(@"Failed to find %@.ttf in bundle", filename);
        }
    };
    
    // Load fonts
    // Emoji is handled via CoreText fallback (iOS uses 'emjc' format which FreeType can't decode)
    loadFont(@"Roboto", @"Roboto-Regular");
    loadFont(@"Playfair", @"PlayfairDisplay-Regular");
    loadFont(@"Playfair-Italic", @"PlayfairDisplay-Italic");
}

- (void)setText:(NSString*)text
     fontFamily:(NSString*)fontFamily
       fontSize:(float)fontSize
          color:(uint32_t)color {
    [self setText:text
       fontFamily:fontFamily
         fontSize:fontSize
            color:color
       fontWeight:400
           italic:NO
        underline:NO
    letterSpacing:0.0f
       wordSpacing:0.0f
    backgroundColor:0x00000000
      hasBackground:NO
       shadowColor:0xFF000000
      shadowOffsetX:0.0f
      shadowOffsetY:0.0f
    shadowBlurSigma:0.0f
         hasShadow:NO];
}

- (void)setText:(NSString*)text
     fontFamily:(NSString*)fontFamily
       fontSize:(float)fontSize
          color:(uint32_t)color
     fontWeight:(int)fontWeight
         italic:(BOOL)italic
      underline:(BOOL)underline {
    [self setText:text
       fontFamily:fontFamily
         fontSize:fontSize
            color:color
       fontWeight:fontWeight
           italic:italic
        underline:underline
    letterSpacing:0.0f
       wordSpacing:0.0f
    backgroundColor:0x00000000
      hasBackground:NO
       shadowColor:0xFF000000
      shadowOffsetX:0.0f
      shadowOffsetY:0.0f
    shadowBlurSigma:0.0f
         hasShadow:NO];
}

- (void)setText:(NSString*)text 
     fontFamily:(NSString*)fontFamily 
       fontSize:(float)fontSize 
          color:(uint32_t)color
     fontWeight:(int)fontWeight
         italic:(BOOL)italic
      underline:(BOOL)underline
  letterSpacing:(float)letterSpacing
     wordSpacing:(float)wordSpacing
  backgroundColor:(uint32_t)backgroundColor
    hasBackground:(BOOL)hasBackground
     shadowColor:(uint32_t)shadowColor
    shadowOffsetX:(float)shadowOffsetX
    shadowOffsetY:(float)shadowOffsetY
  shadowBlurSigma:(float)shadowBlurSigma
       hasShadow:(BOOL)hasShadow {
    TextStyle style = makeTextStyle(fontFamily, fontSize, color, fontWeight, italic, underline,
                                    letterSpacing, wordSpacing, backgroundColor, hasBackground,
                                    shadowColor, shadowOffsetX, shadowOffsetY, shadowBlurSigma, hasShadow);
    _textRenderer->setText([text UTF8String], style);
}

- (void)beginRichText {
    _richTextSpans.clear();
}

- (void)addStyledSpan:(NSString*)text
           fontFamily:(NSString*)fontFamily
             fontSize:(float)fontSize
                color:(uint32_t)color
           fontWeight:(int)fontWeight
               italic:(BOOL)italic
            underline:(BOOL)underline {
    [self addStyledSpan:text
             fontFamily:fontFamily
               fontSize:fontSize
                  color:color
             fontWeight:fontWeight
                 italic:italic
              underline:underline
         letterSpacing:0.0f
            wordSpacing:0.0f
       backgroundColor:0x00000000
         hasBackground:NO
          shadowColor:0xFF000000
         shadowOffsetX:0.0f
         shadowOffsetY:0.0f
       shadowBlurSigma:0.0f
            hasShadow:NO];
}

- (void)addStyledSpan:(NSString*)text
           fontFamily:(NSString*)fontFamily
             fontSize:(float)fontSize
                color:(uint32_t)color
           fontWeight:(int)fontWeight
               italic:(BOOL)italic
            underline:(BOOL)underline
       letterSpacing:(float)letterSpacing
          wordSpacing:(float)wordSpacing
     backgroundColor:(uint32_t)backgroundColor
       hasBackground:(BOOL)hasBackground
        shadowColor:(uint32_t)shadowColor
       shadowOffsetX:(float)shadowOffsetX
       shadowOffsetY:(float)shadowOffsetY
     shadowBlurSigma:(float)shadowBlurSigma
          hasShadow:(BOOL)hasShadow {
    _richTextSpans.push_back(makeStyledSpan(text, fontFamily, fontSize, color, fontWeight, italic, underline,
                                            letterSpacing, wordSpacing, backgroundColor, hasBackground,
                                            shadowColor, shadowOffsetX, shadowOffsetY, shadowBlurSigma, hasShadow));
}

- (void)endRichText {
    if (!_richTextSpans.empty()) {
        _textRenderer->setRichText(_richTextSpans);
    }
}

- (void)setMaxWidth:(float)maxWidth {
    _textRenderer->setMaxWidth(maxWidth);
}

- (void)layoutIfNeeded {
    _textRenderer->layoutIfNeeded();
}

- (float)getHeight {
    return _textRenderer->getHeight();
}

- (float)getWidth {
    return _textRenderer->getWidth();
}

- (int)getLineCount {
    return _textRenderer->getLineCount();
}

- (float)getMaxIntrinsicWidth {
    return _textRenderer->getMaxIntrinsicWidth();
}

- (float)getMinIntrinsicWidth {
    return _textRenderer->getMinIntrinsicWidth();
}

- (int)getTextLength {
    return _textRenderer->getTextLength();
}

- (void)setShowCursor:(BOOL)show {
    _showCursor = show;
}

- (void)setScale:(float)scale {
    _textRenderer->setScale(scale);
}

#pragma mark - Text Content

- (NSString*)getText {
    return [NSString stringWithUTF8String:_textRenderer->getText().c_str()];
}

- (NSString*)getSelectedText {
    return [NSString stringWithUTF8String:_textRenderer->getSelectedText().c_str()];
}

#pragma mark - Text Editing

- (void)insertText:(NSString*)text {
    _textRenderer->insertText([text UTF8String]);
}

- (void)deleteBackward {
    _textRenderer->deleteBackward();
}

- (void)deleteForward {
    _textRenderer->deleteForward();
}

- (void)deleteWordBackward {
    _textRenderer->deleteWordBackward();
}

- (void)deleteWordForward {
    _textRenderer->deleteWordForward();
}

- (void)deleteSelection {
    _textRenderer->deleteSelection();
}

- (void)insertStyledText:(NSString*)text
              fontFamily:(NSString*)fontFamily
                fontSize:(float)fontSize
                   color:(uint32_t)color
              fontWeight:(int)fontWeight
                  italic:(BOOL)italic
               underline:(BOOL)underline {
    [self insertStyledText:text
                fontFamily:fontFamily
                  fontSize:fontSize
                     color:color
                fontWeight:fontWeight
                    italic:italic
                 underline:underline
            letterSpacing:0.0f
               wordSpacing:0.0f
          backgroundColor:0x00000000
            hasBackground:NO
             shadowColor:0xFF000000
            shadowOffsetX:0.0f
            shadowOffsetY:0.0f
          shadowBlurSigma:0.0f
               hasShadow:NO];
}

- (void)insertStyledText:(NSString*)text
              fontFamily:(NSString*)fontFamily
                fontSize:(float)fontSize
                   color:(uint32_t)color
              fontWeight:(int)fontWeight
                  italic:(BOOL)italic
               underline:(BOOL)underline
          letterSpacing:(float)letterSpacing
             wordSpacing:(float)wordSpacing
        backgroundColor:(uint32_t)backgroundColor
          hasBackground:(BOOL)hasBackground
           shadowColor:(uint32_t)shadowColor
          shadowOffsetX:(float)shadowOffsetX
          shadowOffsetY:(float)shadowOffsetY
        shadowBlurSigma:(float)shadowBlurSigma
             hasShadow:(BOOL)hasShadow {
    _textRenderer->insertStyledText(makeStyledSpan(text, fontFamily, fontSize, color, fontWeight, italic, underline,
                                                   letterSpacing, wordSpacing, backgroundColor, hasBackground,
                                                   shadowColor, shadowOffsetX, shadowOffsetY, shadowBlurSigma, hasShadow));
}

#pragma mark - Text Styling

- (void)applyStyleToSelectionWithFontFamily:(NSString*)fontFamily
                                   fontSize:(float)fontSize
                                      color:(uint32_t)color
                                 fontWeight:(int)fontWeight
                                     italic:(BOOL)italic
                                  underline:(BOOL)underline {
    [self applyStyleToSelectionWithFontFamily:fontFamily
                                     fontSize:fontSize
                                        color:color
                                   fontWeight:fontWeight
                                       italic:italic
                                    underline:underline
                               letterSpacing:0.0f
                                  wordSpacing:0.0f
                             backgroundColor:0x00000000
                               hasBackground:NO
                                shadowColor:0xFF000000
                               shadowOffsetX:0.0f
                               shadowOffsetY:0.0f
                             shadowBlurSigma:0.0f
                                  hasShadow:NO];
}

- (void)applyStyleToSelectionWithFontFamily:(NSString*)fontFamily
                                   fontSize:(float)fontSize
                                      color:(uint32_t)color
                                 fontWeight:(int)fontWeight
                                     italic:(BOOL)italic
                                  underline:(BOOL)underline
                             letterSpacing:(float)letterSpacing
                                wordSpacing:(float)wordSpacing
                           backgroundColor:(uint32_t)backgroundColor
                             hasBackground:(BOOL)hasBackground
                              shadowColor:(uint32_t)shadowColor
                             shadowOffsetX:(float)shadowOffsetX
                             shadowOffsetY:(float)shadowOffsetY
                           shadowBlurSigma:(float)shadowBlurSigma
                                hasShadow:(BOOL)hasShadow {
    TextStyle style = makeTextStyle(fontFamily, fontSize, color, fontWeight, italic, underline,
                                    letterSpacing, wordSpacing, backgroundColor, hasBackground,
                                    shadowColor, shadowOffsetX, shadowOffsetY, shadowBlurSigma, hasShadow);
    _textRenderer->applyStyleToSelection(style);
}

- (NSDictionary*)getStyleAtCursor {
    TextStyle style = _textRenderer->getStyleAtCursor();
    return @{
        @"fontFamily": [NSString stringWithUTF8String:style.fontFamily.c_str()],
        @"fontSize": @(style.fontSize),
        @"color": @(style.color.toARGB()),
        @"fontWeight": @(style.fontWeight),
        @"italic": @(style.italic),
        @"underline": @(style.underline),
        @"letterSpacing": @(style.letterSpacing),
        @"wordSpacing": @(style.wordSpacing),
        @"hasBackground": @(style.hasBackground),
        @"backgroundColor": @(style.backgroundColor.toARGB()),
        @"hasShadow": @(style.hasShadow),
        @"shadowColor": @(style.shadow.color.toARGB()),
        @"shadowOffsetX": @(style.shadow.offsetX),
        @"shadowOffsetY": @(style.shadow.offsetY),
        @"shadowBlurSigma": @(style.shadow.blurSigma)
    };
}

#pragma mark - Paragraph Style

- (void)setTextAlignment:(SkiaTextAlignment)alignment {
    _textRenderer->setTextAlignment(static_cast<TextAlignment>(alignment));
}

- (void)setMaxLines:(int)maxLines {
    _textRenderer->setMaxLines(maxLines);
}

- (void)setEllipsis:(NSString*)ellipsis {
    _textRenderer->setEllipsis([ellipsis UTF8String]);
}

- (void)setLineHeight:(float)height {
    _textRenderer->setLineHeight(height);
}

- (void)setStrutStyleWithFontFamily:(NSString*)fontFamily
                           fontSize:(float)fontSize
                             height:(float)height
                            leading:(float)leading
                        forceHeight:(BOOL)forceHeight
                     heightOverride:(BOOL)heightOverride
                       halfLeading:(BOOL)halfLeading {
    TextStrutStyle strut;
    strut.enabled = true;
    strut.fontFamily = [fontFamily UTF8String];
    strut.fontSize = fontSize;
    strut.height = height;
    strut.leading = leading;
    strut.forceHeight = forceHeight;
    strut.heightOverride = heightOverride;
    strut.halfLeading = halfLeading;
    _textRenderer->setStrutStyle(strut);
}

- (void)clearStrutStyle {
    _textRenderer->clearStrutStyle();
}

#pragma mark - Cursor Navigation

- (void)moveCursorLeft:(BOOL)extendSelection {
    _textRenderer->moveCursorLeft(extendSelection);
}

- (void)moveCursorRight:(BOOL)extendSelection {
    _textRenderer->moveCursorRight(extendSelection);
}

- (void)moveCursorUp:(BOOL)extendSelection {
    _textRenderer->moveCursorUp(extendSelection);
}

- (void)moveCursorDown:(BOOL)extendSelection {
    _textRenderer->moveCursorDown(extendSelection);
}

- (void)moveCursorToWordStart:(BOOL)extendSelection {
    _textRenderer->moveCursorToWordStart(extendSelection);
}

- (void)moveCursorToWordEnd:(BOOL)extendSelection {
    _textRenderer->moveCursorToWordEnd(extendSelection);
}

- (void)moveCursorToLineStart:(BOOL)extendSelection {
    _textRenderer->moveCursorToLineStart(extendSelection);
}

- (void)moveCursorToLineEnd:(BOOL)extendSelection {
    _textRenderer->moveCursorToLineEnd(extendSelection);
}

- (void)moveCursorToDocumentStart:(BOOL)extendSelection {
    _textRenderer->moveCursorToDocumentStart(extendSelection);
}

- (void)moveCursorToDocumentEnd:(BOOL)extendSelection {
    _textRenderer->moveCursorToDocumentEnd(extendSelection);
}

- (void)selectAll {
    _textRenderer->selectAll();
}

#pragma mark - Cursor

- (void)setCursorPosition:(int)position {
    _textRenderer->setCursorPosition(position);
}

- (int)getCursorPosition {
    return _textRenderer->getCursorPosition();
}

- (void)setCursorPositionAtCoordinate:(float)x y:(float)y {
    _textRenderer->setCursorPositionAtCoordinate(x, y);
}

#pragma mark - Selection

- (void)setSelectionStart:(int)start end:(int)end {
    _textRenderer->setSelection(start, end);
}

- (void)clearSelection {
    _textRenderer->clearSelection();
}

- (BOOL)hasSelection {
    return _textRenderer->hasSelection();
}

- (int)getSelectionStart {
    auto [start, end] = _textRenderer->getSelection();
    return start;
}

- (int)getSelectionEnd {
    auto [start, end] = _textRenderer->getSelection();
    return end;
}

- (void)setWordSelectionAtCoordinate:(float)x y:(float)y {
    _textRenderer->setWordSelectionAtCoordinate(x, y);
}

- (void)setLineSelectionAtCoordinate:(float)x y:(float)y {
    _textRenderer->setLineSelectionAtCoordinate(x, y);
}

- (void)beginSelectionAtCoordinate:(float)x y:(float)y {
    _textRenderer->beginSelectionAtCoordinate(x, y);
}

- (void)extendSelectionToCoordinate:(float)x y:(float)y {
    _textRenderer->extendSelectionToCoordinate(x, y);
}

#pragma mark - Colors

- (void)setCursorColor:(uint32_t)color {
    _textRenderer->setCursorColor(Color::fromARGB(color));
}

- (uint32_t)getCursorColor {
    return _textRenderer->getCursorColor().toARGB();
}

- (void)setSelectionColor:(uint32_t)color {
    _textRenderer->setSelectionColor(Color::fromARGB(color));
}

- (uint32_t)getSelectionColor {
    return _textRenderer->getSelectionColor().toARGB();
}

#pragma mark - Query

- (int)getGlyphPositionAtCoordinate:(float)x y:(float)y {
    auto pos = _textRenderer->getGlyphPositionAtCoordinate(x, y);
    return pos.value_or(-1);
}

#pragma mark - MTKViewDelegate

- (void)mtkView:(MTKView*)view drawableSizeWillChange:(CGSize)size {
    // Handle resize if needed
}

- (void)drawInMTKView:(MTKView*)view {
    if (!_grContext) {
        NSLog(@"drawInMTKView: No GrContext");
        return;
    }
    
    id<CAMetalDrawable> drawable = view.currentDrawable;
    if (!drawable) {
        NSLog(@"drawInMTKView: No drawable");
        return;
    }
    
    // Create Skia surface from Metal drawable
    GrMtlTextureInfo textureInfo;
    textureInfo.fTexture.retain((__bridge void*)drawable.texture);
    
    GrBackendRenderTarget backendRT = GrBackendRenderTargets::MakeMtl(
        view.drawableSize.width,
        view.drawableSize.height,
        textureInfo
    );
    
    sk_sp<SkSurface> surface = SkSurfaces::WrapBackendRenderTarget(
        _grContext.get(),
        backendRT,
        kTopLeft_GrSurfaceOrigin,
        kBGRA_8888_SkColorType,
        nullptr,
        nullptr
    );
    
    if (!surface) {
        NSLog(@"drawInMTKView: Failed to create surface (size: %.0fx%.0f)", 
              view.drawableSize.width, view.drawableSize.height);
        return;
    }
    
    SkCanvas* canvas = surface->getCanvas();
    
    canvas->clear(SK_ColorWHITE);
    
    // Render text with cursor
    _textRenderer->render(canvas, _renderX, _renderY, _showCursor);
    
    // Submit to GPU
    _grContext->flush();
    _grContext->submit(GrSyncCpu::kNo);
    
    // Present
    id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];
    // Note: Synchronous wait blocks CPU. For production, use async completion handlers.
    [commandBuffer waitUntilCompleted];
}

@end
