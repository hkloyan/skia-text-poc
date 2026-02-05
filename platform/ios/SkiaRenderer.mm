#import "SkiaRenderer.hh"
#include "core/text_editor.hpp"
#include "core/font_manager.hpp"

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

@implementation SkiaRenderer {
    id<MTLDevice> _device;
    id<MTLCommandQueue> _commandQueue;
    sk_sp<GrDirectContext> _grContext;
    std::unique_ptr<TextEditor> _textEditor;
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
        
        _textEditor = std::make_unique<TextEditor>();
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
    _textEditor->setText([text UTF8String], 
                         TextEditor::makeStyle([fontFamily UTF8String], fontSize, color, fontWeight, italic, underline,
                                               letterSpacing, wordSpacing, backgroundColor, hasBackground,
                                               shadowColor, shadowOffsetX, shadowOffsetY, shadowBlurSigma, hasShadow));
}

- (void)beginRichText {
    _textEditor->beginRichText();
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
    _textEditor->addStyledSpan(TextEditor::makeSpan([text UTF8String], [fontFamily UTF8String], fontSize, color, 
                                                     fontWeight, italic, underline, letterSpacing, wordSpacing, 
                                                     backgroundColor, hasBackground, shadowColor, shadowOffsetX, 
                                                     shadowOffsetY, shadowBlurSigma, hasShadow));
}

- (void)endRichText {
    _textEditor->endRichText();
}

- (void)setMaxWidth:(float)maxWidth {
    _textEditor->setMaxWidth(maxWidth);
}

- (void)layoutIfNeeded {
    _textEditor->layoutIfNeeded();
}

- (float)getHeight {
    return _textEditor->getHeight();
}

- (float)getWidth {
    return _textEditor->getWidth();
}

- (int)getLineCount {
    return _textEditor->getLineCount();
}

- (float)getMaxIntrinsicWidth {
    return _textEditor->getMaxIntrinsicWidth();
}

- (float)getMinIntrinsicWidth {
    return _textEditor->getMinIntrinsicWidth();
}

- (int)getTextLength {
    return _textEditor->getTextLength();
}

- (void)setShowCursor:(BOOL)show {
    _showCursor = show;
}

- (void)setScale:(float)scale {
    _textEditor->setScale(scale);
}

#pragma mark - Text Content

- (NSString*)getText {
    return [NSString stringWithUTF8String:_textEditor->getText().c_str()];
}

- (NSString*)getSelectedText {
    return [NSString stringWithUTF8String:_textEditor->getSelectedText().c_str()];
}

#pragma mark - Text Editing

- (void)insertText:(NSString*)text {
    _textEditor->insertText([text UTF8String]);
}

- (void)deleteBackward {
    _textEditor->deleteBackward();
}

- (void)deleteForward {
    _textEditor->deleteForward();
}

- (void)deleteWordBackward {
    _textEditor->deleteWordBackward();
}

- (void)deleteWordForward {
    _textEditor->deleteWordForward();
}

- (void)deleteSelection {
    _textEditor->deleteSelection();
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
    _textEditor->insertStyledText(TextEditor::makeSpan([text UTF8String], [fontFamily UTF8String], fontSize, color,
                                                        fontWeight, italic, underline, letterSpacing, wordSpacing,
                                                        backgroundColor, hasBackground, shadowColor, shadowOffsetX,
                                                        shadowOffsetY, shadowBlurSigma, hasShadow));
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
    _textEditor->applyStyleToSelection(TextEditor::makeStyle([fontFamily UTF8String], fontSize, color, fontWeight,
                                                              italic, underline, letterSpacing, wordSpacing,
                                                              backgroundColor, hasBackground, shadowColor,
                                                              shadowOffsetX, shadowOffsetY, shadowBlurSigma, hasShadow));
}

- (NSDictionary*)getStyleAtCursor {
    TextStyle style = _textEditor->getStyleAtCursor();
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
    _textEditor->setTextAlignment(static_cast<TextAlignment>(alignment));
}

- (void)setMaxLines:(int)maxLines {
    _textEditor->setMaxLines(maxLines);
}

- (void)setEllipsis:(NSString*)ellipsis {
    _textEditor->setEllipsis([ellipsis UTF8String]);
}

- (void)setLineHeight:(float)height {
    _textEditor->setLineHeight(height);
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
    _textEditor->setStrutStyle(strut);
}

- (void)clearStrutStyle {
    _textEditor->clearStrutStyle();
}

#pragma mark - Cursor Navigation

- (void)moveCursorLeft:(BOOL)extendSelection {
    _textEditor->moveCursorLeft(extendSelection);
}

- (void)moveCursorRight:(BOOL)extendSelection {
    _textEditor->moveCursorRight(extendSelection);
}

- (void)moveCursorUp:(BOOL)extendSelection {
    _textEditor->moveCursorUp(extendSelection);
}

- (void)moveCursorDown:(BOOL)extendSelection {
    _textEditor->moveCursorDown(extendSelection);
}

- (void)moveCursorToWordStart:(BOOL)extendSelection {
    _textEditor->moveCursorToWordStart(extendSelection);
}

- (void)moveCursorToWordEnd:(BOOL)extendSelection {
    _textEditor->moveCursorToWordEnd(extendSelection);
}

- (void)moveCursorToLineStart:(BOOL)extendSelection {
    _textEditor->moveCursorToLineStart(extendSelection);
}

- (void)moveCursorToLineEnd:(BOOL)extendSelection {
    _textEditor->moveCursorToLineEnd(extendSelection);
}

- (void)moveCursorToDocumentStart:(BOOL)extendSelection {
    _textEditor->moveCursorToDocumentStart(extendSelection);
}

- (void)moveCursorToDocumentEnd:(BOOL)extendSelection {
    _textEditor->moveCursorToDocumentEnd(extendSelection);
}

- (void)selectAll {
    _textEditor->selectAll();
}

#pragma mark - Cursor

- (void)setCursorPosition:(int)position {
    _textEditor->setCursorPosition(position);
}

- (int)getCursorPosition {
    return _textEditor->getCursorPosition();
}

- (void)setCursorPositionAtCoordinate:(float)x y:(float)y {
    _textEditor->setCursorPositionAtCoordinate(x, y);
}

#pragma mark - Selection

- (void)setSelectionStart:(int)start end:(int)end {
    _textEditor->setSelection(start, end);
}

- (void)clearSelection {
    _textEditor->clearSelection();
}

- (BOOL)hasSelection {
    return _textEditor->hasSelection();
}

- (int)getSelectionStart {
    auto [start, end] = _textEditor->getSelection();
    return start;
}

- (int)getSelectionEnd {
    auto [start, end] = _textEditor->getSelection();
    return end;
}

- (void)setWordSelectionAtCoordinate:(float)x y:(float)y {
    _textEditor->setWordSelectionAtCoordinate(x, y);
}

- (void)setLineSelectionAtCoordinate:(float)x y:(float)y {
    _textEditor->setLineSelectionAtCoordinate(x, y);
}

- (void)beginSelectionAtCoordinate:(float)x y:(float)y {
    _textEditor->beginSelectionAtCoordinate(x, y);
}

- (void)extendSelectionToCoordinate:(float)x y:(float)y {
    _textEditor->extendSelectionToCoordinate(x, y);
}

#pragma mark - Colors

- (void)setCursorColor:(uint32_t)color {
    _textEditor->setCursorColor(Color::fromARGB(color));
}

- (uint32_t)getCursorColor {
    return _textEditor->getCursorColor().toARGB();
}

- (void)setSelectionColor:(uint32_t)color {
    _textEditor->setSelectionColor(Color::fromARGB(color));
}

- (uint32_t)getSelectionColor {
    return _textEditor->getSelectionColor().toARGB();
}

#pragma mark - Query

- (int)getGlyphPositionAtCoordinate:(float)x y:(float)y {
    auto pos = _textEditor->getGlyphPositionAtCoordinate(x, y);
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
    _textEditor->render(canvas, _renderX, _renderY, _showCursor);
    
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
