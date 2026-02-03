#import <Foundation/Foundation.h>
#import <MetalKit/MetalKit.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, SkiaTextAlignment) {
    SkiaTextAlignmentLeft = 0,
    SkiaTextAlignmentRight = 1,
    SkiaTextAlignmentCenter = 2,
    SkiaTextAlignmentJustify = 3,
    SkiaTextAlignmentStart = 4,
    SkiaTextAlignmentEnd = 5
};

@interface SkiaRenderer : NSObject <MTKViewDelegate>

- (instancetype)initWithMetalDevice:(id<MTLDevice>)device;
- (void)loadFonts;

// Simple text API (basic style only)
- (void)setText:(NSString*)text
     fontFamily:(NSString*)fontFamily
       fontSize:(float)fontSize
          color:(uint32_t)color;
- (void)setText:(NSString*)text
     fontFamily:(NSString*)fontFamily
       fontSize:(float)fontSize
          color:(uint32_t)color
     fontWeight:(int)fontWeight
         italic:(BOOL)italic
      underline:(BOOL)underline;

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
       hasShadow:(BOOL)hasShadow;

// Rich text API
- (void)beginRichText;
- (void)addStyledSpan:(NSString*)text
           fontFamily:(NSString*)fontFamily
             fontSize:(float)fontSize
                color:(uint32_t)color
           fontWeight:(int)fontWeight
               italic:(BOOL)italic
            underline:(BOOL)underline;
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
          hasShadow:(BOOL)hasShadow;
- (void)endRichText;
- (void)setMaxWidth:(float)maxWidth;
- (void)layoutIfNeeded;

// Render position and cursor visibility
- (void)setRenderPosition:(float)x y:(float)y;
- (void)setShowCursor:(BOOL)show;

// Scale factor for high-DPI rendering (set to contentScaleFactor)
- (void)setScale:(float)scale;

// Layout metrics
- (float)getHeight;
- (float)getWidth;
- (int)getLineCount;
- (float)getMaxIntrinsicWidth;
- (float)getMinIntrinsicWidth;
- (int)getTextLength;

// Text content
- (NSString*)getText;
- (NSString*)getSelectedText;

// Text editing
- (void)insertText:(NSString*)text;
- (void)insertStyledText:(NSString*)text
              fontFamily:(NSString*)fontFamily
                fontSize:(float)fontSize
                   color:(uint32_t)color
              fontWeight:(int)fontWeight
                  italic:(BOOL)italic
               underline:(BOOL)underline;
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
             hasShadow:(BOOL)hasShadow;
- (void)deleteBackward;
- (void)deleteForward;
- (void)deleteWordBackward;
- (void)deleteWordForward;
- (void)deleteSelection;

// Text styling
- (void)applyStyleToSelectionWithFontFamily:(NSString*)fontFamily
                                   fontSize:(float)fontSize
                                      color:(uint32_t)color
                                 fontWeight:(int)fontWeight
                                     italic:(BOOL)italic
                                  underline:(BOOL)underline;
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
                                hasShadow:(BOOL)hasShadow;
- (NSDictionary*)getStyleAtCursor;

// Paragraph-level style
- (void)setTextAlignment:(SkiaTextAlignment)alignment;
- (void)setMaxLines:(int)maxLines;
- (void)setEllipsis:(NSString*)ellipsis;
- (void)setLineHeight:(float)height;
- (void)setStrutStyleWithFontFamily:(NSString*)fontFamily
                           fontSize:(float)fontSize
                             height:(float)height
                            leading:(float)leading
                        forceHeight:(BOOL)forceHeight
                     heightOverride:(BOOL)heightOverride
                       halfLeading:(BOOL)halfLeading;
- (void)clearStrutStyle;

// Cursor navigation
- (void)moveCursorLeft:(BOOL)extendSelection;
- (void)moveCursorRight:(BOOL)extendSelection;
- (void)moveCursorUp:(BOOL)extendSelection;
- (void)moveCursorDown:(BOOL)extendSelection;
- (void)moveCursorToWordStart:(BOOL)extendSelection;
- (void)moveCursorToWordEnd:(BOOL)extendSelection;
- (void)moveCursorToLineStart:(BOOL)extendSelection;
- (void)moveCursorToLineEnd:(BOOL)extendSelection;
- (void)moveCursorToDocumentStart:(BOOL)extendSelection;
- (void)moveCursorToDocumentEnd:(BOOL)extendSelection;
- (void)selectAll;

// Cursor
- (void)setCursorPosition:(int)position;
- (int)getCursorPosition;
- (void)setCursorPositionAtCoordinate:(float)x y:(float)y;

// Selection
- (void)setSelectionStart:(int)start end:(int)end;
- (void)clearSelection;
- (BOOL)hasSelection;
- (int)getSelectionStart;
- (int)getSelectionEnd;
- (void)setWordSelectionAtCoordinate:(float)x y:(float)y;
- (void)setLineSelectionAtCoordinate:(float)x y:(float)y;
- (void)beginSelectionAtCoordinate:(float)x y:(float)y;
- (void)extendSelectionToCoordinate:(float)x y:(float)y;

// Colors
- (void)setCursorColor:(uint32_t)color;
- (uint32_t)getCursorColor;
- (void)setSelectionColor:(uint32_t)color;
- (uint32_t)getSelectionColor;

// Query methods
- (int)getGlyphPositionAtCoordinate:(float)x y:(float)y;

@end

NS_ASSUME_NONNULL_END
