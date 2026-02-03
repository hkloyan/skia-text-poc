import UIKit
import MetalKit

class SkiaMetalView: MTKView {
    private var renderer: SkiaRenderer!
    
    // Cursor blink state
    private var cursorVisible = false
    private var cursorBlinkTimer: Timer?
    private var isEditing = false
    
    // Touch tracking for multi-tap detection
    private var lastTapTime: Date?
    private var tapCount = 0
    
    // Text render offset (must match what's used in render calls)
    var textOffsetX: Float = 50.0
    var textOffsetY: Float = 50.0
    
    // Typing style (nil means inherit from cursor position)
    var typingStyle: [String: Any]?
    
    // Callback when cursor position or selection changes (for toolbar sync)
    var onCursorChanged: (() -> Void)?
    
    override init(frame frameRect: CGRect, device: MTLDevice?) {
        super.init(frame: frameRect, device: device ?? MTLCreateSystemDefaultDevice())
        setup()
    }
    
    required init(coder: NSCoder) {
        super.init(coder: coder)
        self.device = MTLCreateSystemDefaultDevice()
        setup()
    }
    
    private func setup() {
        self.colorPixelFormat = .bgra8Unorm
        self.framebufferOnly = false
        self.preferredFramesPerSecond = 60
        self.isMultipleTouchEnabled = false
        self.isUserInteractionEnabled = true
        
        guard let device = self.device else {
            fatalError("Metal device not available")
        }
        
        renderer = SkiaRenderer(metalDevice: device)
        renderer.loadFonts()
        
        // Set scale factor for high-DPI rendering
        // This applies a canvas transform so all coordinates work in logical pixels
        renderer.setScale(Float(contentScaleFactor))
        
        self.delegate = renderer
    }
    
    // MARK: - First Responder (for keyboard)
    
    override var canBecomeFirstResponder: Bool { true }
    
    override func becomeFirstResponder() -> Bool {
        let result = super.becomeFirstResponder()
        if result {
            startEditing()
        }
        return result
    }
    
    override func resignFirstResponder() -> Bool {
        let result = super.resignFirstResponder()
        if result {
            stopEditing()
        }
        return result
    }
    
    private func startEditing() {
        isEditing = true
        cursorVisible = true
        renderer.setShowCursor(true)
        setNeedsDisplay()
        startCursorBlink()
    }
    
    private func stopEditing() {
        isEditing = false
        stopCursorBlink()
        cursorVisible = false
        renderer.setShowCursor(false)
        setNeedsDisplay()
    }
    
    // MARK: - Cursor Blink
    
    private func startCursorBlink() {
        stopCursorBlink()
        cursorBlinkTimer = Timer.scheduledTimer(withTimeInterval: 0.5, repeats: true) { [weak self] _ in
            guard let self = self, self.isEditing else { return }
            self.cursorVisible.toggle()
            self.renderer.setShowCursor(self.cursorVisible)
            self.setNeedsDisplay()
        }
    }
    
    private func stopCursorBlink() {
        cursorBlinkTimer?.invalidate()
        cursorBlinkTimer = nil
    }
    
    private func resetCursorBlink() {
        // Show cursor and restart blink timer (called after user interaction)
        cursorVisible = true
        renderer.setShowCursor(true)
        setNeedsDisplay()
        startCursorBlink()
    }
    
    // MARK: - Touch Handling
    
    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard let touch = touches.first else { return }
        
        // Become first responder to get keyboard
        if !isFirstResponder {
            _ = becomeFirstResponder()
        }
        
        // Clear typing style when tapping (will inherit from new position)
        typingStyle = nil
        
        // Detect tap count for double/triple tap
        let now = Date()
        if let lastTap = lastTapTime, now.timeIntervalSince(lastTap) < 0.3 {
            tapCount += 1
        } else {
            tapCount = 1
        }
        lastTapTime = now
        
        let location = touch.location(in: self)
        
        // Convert to text-local coordinates (already in logical points from UIKit)
        let localX = Float(location.x) - textOffsetX
        let localY = Float(location.y) - textOffsetY
        
        if tapCount == 2 {
            // Double tap - select word
            renderer.setWordSelectionAtCoordinate(localX, y: localY)
        } else if tapCount == 3 {
            // Triple tap - select line
            renderer.setLineSelectionAtCoordinate(localX, y: localY)
            tapCount = 0  // Reset after triple
        } else {
            // Single tap - begin selection (might become drag)
            renderer.beginSelection(atCoordinate: localX, y: localY)
        }
        
        resetCursorBlink()
        onCursorChanged?()
    }
    
    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard let touch = touches.first, tapCount == 1 else { return }
        
        let location = touch.location(in: self)
        
        // Convert to text-local coordinates (already in logical points from UIKit)
        let localX = Float(location.x) - textOffsetX
        let localY = Float(location.y) - textOffsetY
        
        renderer.extendSelection(toCoordinate: localX, y: localY)
        setNeedsDisplay()
        onCursorChanged?()
    }
    
    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
        // Touch handling complete
    }
    
    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
        // Touch handling cancelled
    }
    
    func setText(_ text: String, fontFamily: String, fontSize: Float, color: UInt32,
                 fontWeight: Int32 = 400, italic: Bool = false, underline: Bool = false,
                 letterSpacing: Float = 0, wordSpacing: Float = 0,
                 backgroundColor: UInt32 = 0x00000000, hasBackground: Bool = false,
                 shadowColor: UInt32 = 0xFF000000, shadowOffsetX: Float = 0, shadowOffsetY: Float = 0,
                 shadowBlurSigma: Float = 0, hasShadow: Bool = false) {
        renderer.setText(text, fontFamily: fontFamily, fontSize: fontSize, color: color,
                         fontWeight: fontWeight, italic: italic, underline: underline,
                         letterSpacing: letterSpacing, wordSpacing: wordSpacing,
                         backgroundColor: backgroundColor, hasBackground: hasBackground,
                         shadowColor: shadowColor, shadowOffsetX: shadowOffsetX, shadowOffsetY: shadowOffsetY,
                         shadowBlurSigma: shadowBlurSigma, hasShadow: hasShadow)
        setNeedsDisplay()
    }
    
    func beginRichText() {
        renderer.beginRichText()
    }
    
    func addStyledSpan(_ text: String, fontFamily: String, fontSize: Float, color: UInt32,
                       fontWeight: Int32 = 400, italic: Bool = false, underline: Bool = false,
                       letterSpacing: Float = 0, wordSpacing: Float = 0,
                       backgroundColor: UInt32 = 0x00000000, hasBackground: Bool = false,
                       shadowColor: UInt32 = 0xFF000000, shadowOffsetX: Float = 0, shadowOffsetY: Float = 0,
                       shadowBlurSigma: Float = 0, hasShadow: Bool = false) {
        renderer.addStyledSpan(text, fontFamily: fontFamily, fontSize: fontSize, color: color,
                               fontWeight: fontWeight, italic: italic, underline: underline,
                               letterSpacing: letterSpacing, wordSpacing: wordSpacing,
                               backgroundColor: backgroundColor, hasBackground: hasBackground,
                               shadowColor: shadowColor, shadowOffsetX: shadowOffsetX, shadowOffsetY: shadowOffsetY,
                               shadowBlurSigma: shadowBlurSigma, hasShadow: hasShadow)
    }
    
    func endRichText() {
        renderer.endRichText()
        setNeedsDisplay()
    }
    
    func setMaxWidth(_ maxWidth: Float) {
        renderer.setMaxWidth(maxWidth)
        setNeedsDisplay()
    }

    func setTextAlignment(_ alignment: SkiaTextAlignment) {
        renderer.setTextAlignment(alignment)
        setNeedsDisplay()
    }

    func setMaxLines(_ maxLines: Int) {
        renderer.setMaxLines(Int32(maxLines))
        setNeedsDisplay()
    }

    func setEllipsis(_ ellipsis: String) {
        renderer.setEllipsis(ellipsis)
        setNeedsDisplay()
    }

    func setLineHeight(_ height: Float) {
        renderer.setLineHeight(height)
        setNeedsDisplay()
    }

    func setStrutStyle(fontFamily: String, fontSize: Float, height: Float,
                       leading: Float, forceHeight: Bool, heightOverride: Bool, halfLeading: Bool) {
        renderer.setStrutStyleWithFontFamily(fontFamily, fontSize: fontSize, height: height,
                                             leading: leading, forceHeight: forceHeight,
                                             heightOverride: heightOverride, halfLeading: halfLeading)
        setNeedsDisplay()
    }

    func clearStrutStyle() {
        renderer.clearStrutStyle()
        setNeedsDisplay()
    }
    
    func setRenderPosition(x: Float, y: Float) {
        renderer.setRenderPosition(x, y: y)
    }
    
    func setShowCursor(_ show: Bool) {
        renderer.setShowCursor(show)
        setNeedsDisplay()
    }
    
    // MARK: - Layout metrics
    
    func getHeight() -> Float {
        return renderer.getHeight()
    }
    
    func getWidth() -> Float {
        return renderer.getWidth()
    }
    
    func getLineCount() -> Int {
        return Int(renderer.getLineCount())
    }
    
    func getMaxIntrinsicWidth() -> Float {
        return renderer.getMaxIntrinsicWidth()
    }
    
    func getMinIntrinsicWidth() -> Float {
        return renderer.getMinIntrinsicWidth()
    }
    
    func getTextLength() -> Int {
        return Int(renderer.getTextLength())
    }
    
    // MARK: - Text Content
    
    func getText() -> String {
        return renderer.getText()
    }
    
    func getSelectedText() -> String {
        return renderer.getSelectedText()
    }
    
    // MARK: - Text Editing
    
    func deleteForward() {
        renderer.deleteForward()
        setNeedsDisplay()
    }
    
    func deleteWordBackward() {
        renderer.deleteWordBackward()
        setNeedsDisplay()
    }
    
    func deleteWordForward() {
        renderer.deleteWordForward()
        setNeedsDisplay()
    }
    
    func deleteSelection() {
        renderer.deleteSelection()
        setNeedsDisplay()
    }
    
    func insertStyledText(_ text: String, fontFamily: String, fontSize: Float, color: UInt32,
                          fontWeight: Int32 = 400, italic: Bool = false, underline: Bool = false,
                          letterSpacing: Float = 0, wordSpacing: Float = 0,
                          backgroundColor: UInt32 = 0x00000000, hasBackground: Bool = false,
                          shadowColor: UInt32 = 0xFF000000, shadowOffsetX: Float = 0, shadowOffsetY: Float = 0,
                          shadowBlurSigma: Float = 0, hasShadow: Bool = false) {
        renderer.insertStyledText(text, fontFamily: fontFamily, fontSize: fontSize, color: color,
                                  fontWeight: fontWeight, italic: italic, underline: underline,
                                  letterSpacing: letterSpacing, wordSpacing: wordSpacing,
                                  backgroundColor: backgroundColor, hasBackground: hasBackground,
                                  shadowColor: shadowColor, shadowOffsetX: shadowOffsetX, shadowOffsetY: shadowOffsetY,
                                  shadowBlurSigma: shadowBlurSigma, hasShadow: hasShadow)
        setNeedsDisplay()
    }
    
    // MARK: - Text Styling
    
    func applyStyleToSelection(fontFamily: String, fontSize: Float, color: UInt32,
                               fontWeight: Int32 = 400, italic: Bool = false, underline: Bool = false,
                               letterSpacing: Float = 0, wordSpacing: Float = 0,
                               backgroundColor: UInt32 = 0x00000000, hasBackground: Bool = false,
                               shadowColor: UInt32 = 0xFF000000, shadowOffsetX: Float = 0, shadowOffsetY: Float = 0,
                               shadowBlurSigma: Float = 0, hasShadow: Bool = false) {
        renderer.applyStyleToSelection(withFontFamily: fontFamily, fontSize: fontSize, color: color,
                                       fontWeight: fontWeight, italic: italic, underline: underline,
                                       letterSpacing: letterSpacing, wordSpacing: wordSpacing,
                                       backgroundColor: backgroundColor, hasBackground: hasBackground,
                                       shadowColor: shadowColor, shadowOffsetX: shadowOffsetX, shadowOffsetY: shadowOffsetY,
                                       shadowBlurSigma: shadowBlurSigma, hasShadow: hasShadow)
        setNeedsDisplay()
    }
    
    func getStyleAtCursor() -> [String: Any] {
        return renderer.getStyleAtCursor() as? [String: Any] ?? [:]
    }
    
    // MARK: - Cursor Navigation
    
    func moveCursorLeft(extendSelection: Bool = false) {
        renderer.moveCursorLeft(extendSelection)
        setNeedsDisplay()
    }
    
    func moveCursorRight(extendSelection: Bool = false) {
        renderer.moveCursorRight(extendSelection)
        setNeedsDisplay()
    }
    
    func moveCursorUp(extendSelection: Bool = false) {
        renderer.moveCursorUp(extendSelection)
        setNeedsDisplay()
    }
    
    func moveCursorDown(extendSelection: Bool = false) {
        renderer.moveCursorDown(extendSelection)
        setNeedsDisplay()
    }
    
    func moveCursorToWordStart(extendSelection: Bool = false) {
        renderer.moveCursor(toWordStart: extendSelection)
        setNeedsDisplay()
    }
    
    func moveCursorToWordEnd(extendSelection: Bool = false) {
        renderer.moveCursor(toWordEnd: extendSelection)
        setNeedsDisplay()
    }
    
    func moveCursorToLineStart(extendSelection: Bool = false) {
        renderer.moveCursor(toLineStart: extendSelection)
        setNeedsDisplay()
    }
    
    func moveCursorToLineEnd(extendSelection: Bool = false) {
        renderer.moveCursor(toLineEnd: extendSelection)
        setNeedsDisplay()
    }
    
    func moveCursorToDocumentStart(extendSelection: Bool = false) {
        renderer.moveCursor(toDocumentStart: extendSelection)
        setNeedsDisplay()
    }
    
    func moveCursorToDocumentEnd(extendSelection: Bool = false) {
        renderer.moveCursor(toDocumentEnd: extendSelection)
        setNeedsDisplay()
    }
    
    func selectAll() {
        renderer.selectAll()
        setNeedsDisplay()
    }
    
    // MARK: - Cursor
    
    func setCursorPosition(_ position: Int) {
        renderer.setCursorPosition(Int32(position))
        setNeedsDisplay()
    }
    
    func getCursorPosition() -> Int {
        return Int(renderer.getCursorPosition())
    }
    
    func setCursorPositionAtCoordinate(x: Float, y: Float) {
        renderer.setCursorPositionAtCoordinate(x, y: y)
        setNeedsDisplay()
    }
    
    // MARK: - Selection
    
    func setSelection(start: Int, end: Int) {
        renderer.setSelectionStart(Int32(start), end: Int32(end))
        setNeedsDisplay()
    }
    
    func clearSelection() {
        renderer.clearSelection()
        setNeedsDisplay()
    }
    
    func hasSelection() -> Bool {
        return renderer.hasSelection()
    }
    
    func getSelection() -> (start: Int, end: Int) {
        return (Int(renderer.getSelectionStart()), Int(renderer.getSelectionEnd()))
    }
    
    func setWordSelectionAtCoordinate(x: Float, y: Float) {
        renderer.setWordSelectionAtCoordinate(x, y: y)
        setNeedsDisplay()
    }
    
    func setLineSelectionAtCoordinate(x: Float, y: Float) {
        renderer.setLineSelectionAtCoordinate(x, y: y)
        setNeedsDisplay()
    }
    
    func beginSelectionAtCoordinate(x: Float, y: Float) {
        renderer.beginSelection(atCoordinate: x, y: y)
        setNeedsDisplay()
    }
    
    func extendSelectionToCoordinate(x: Float, y: Float) {
        renderer.extendSelection(toCoordinate: x, y: y)
        setNeedsDisplay()
    }
    
    // MARK: - Colors
    
    func setCursorColor(_ color: UInt32) {
        renderer.setCursorColor(color)
    }
    
    func getCursorColor() -> UInt32 {
        return renderer.getCursorColor()
    }
    
    func setSelectionColor(_ color: UInt32) {
        renderer.setSelectionColor(color)
    }
    
    func getSelectionColor() -> UInt32 {
        return renderer.getSelectionColor()
    }
    
    // MARK: - Query
    
    func getGlyphPositionAtCoordinate(x: Float, y: Float) -> Int {
        return Int(renderer.getGlyphPosition(atCoordinate: x, y: y))
    }
    
    func snapshot() -> UIImage? {
        // Force multiple draws to ensure the drawable we read has current content
        // (Metal uses triple buffering, so we cycle through all buffers)
        // This is just for PoC, as we're drawing directly onto the drawable
        draw()
        draw()
        draw()
        
        // Get the current drawable texture
        guard let drawable = currentDrawable else {
            return nil
        }
        let texture = drawable.texture
        
        let width = Int(drawableSize.width)
        let height = Int(drawableSize.height)
        let bytesPerPixel = 4
        let bytesPerRow = width * bytesPerPixel
        let dataSize = bytesPerRow * height
        
        var data = [UInt8](repeating: 0, count: dataSize)
        
        let region = MTLRegion(origin: MTLOrigin(x: 0, y: 0, z: 0),
                               size: MTLSize(width: width, height: height, depth: 1))
        
        texture.getBytes(&data, bytesPerRow: bytesPerRow, from: region, mipmapLevel: 0)
        
        // Convert BGRA to RGBA
        for i in stride(from: 0, to: dataSize, by: bytesPerPixel) {
            let b = data[i]
            let r = data[i + 2]
            data[i] = r
            data[i + 2] = b
        }
        
        guard let provider = CGDataProvider(data: Data(data) as CFData),
              let cgImage = CGImage(
                width: width,
                height: height,
                bitsPerComponent: 8,
                bitsPerPixel: 32,
                bytesPerRow: bytesPerRow,
                space: CGColorSpaceCreateDeviceRGB(),
                bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.premultipliedLast.rawValue),
                provider: provider,
                decode: nil,
                shouldInterpolate: false,
                intent: .defaultIntent
              ) else {
            return nil
        }
        
        // Use scale 1.0 to save at full resolution (not divided by contentScaleFactor)
        return UIImage(cgImage: cgImage, scale: 1.0, orientation: .up)
    }
}

// MARK: - UIKeyInput (Keyboard Support)

extension SkiaMetalView: UIKeyInput {
    var hasText: Bool {
        return getTextLength() > 0
    }
    
    func insertText(_ text: String) {
        if let style = typingStyle {
            let fontFamily = style["fontFamily"] as? String ?? "Roboto"
            let fontSize = (style["fontSize"] as? NSNumber)?.floatValue ?? 24
            let color = (style["color"] as? NSNumber)?.uint32Value ?? 0xFF000000
            let fontWeight = (style["fontWeight"] as? NSNumber)?.int32Value ?? 400
            let italic = style["italic"] as? Bool ?? false
            let underline = style["underline"] as? Bool ?? false
            let letterSpacing = (style["letterSpacing"] as? NSNumber)?.floatValue ?? 0
            let wordSpacing = (style["wordSpacing"] as? NSNumber)?.floatValue ?? 0
            let hasBackground = style["hasBackground"] as? Bool ?? false
            let backgroundColor = (style["backgroundColor"] as? NSNumber)?.uint32Value ?? 0x00000000
            let hasShadow = style["hasShadow"] as? Bool ?? false
            let shadowColor = (style["shadowColor"] as? NSNumber)?.uint32Value ?? 0xFF000000
            let shadowOffsetX = (style["shadowOffsetX"] as? NSNumber)?.floatValue ?? 0
            let shadowOffsetY = (style["shadowOffsetY"] as? NSNumber)?.floatValue ?? 0
            let shadowBlurSigma = (style["shadowBlurSigma"] as? NSNumber)?.floatValue ?? 0
            renderer.insertStyledText(text, fontFamily: fontFamily, fontSize: fontSize, color: color,
                                      fontWeight: fontWeight, italic: italic, underline: underline,
                                      letterSpacing: letterSpacing, wordSpacing: wordSpacing,
                                      backgroundColor: backgroundColor, hasBackground: hasBackground,
                                      shadowColor: shadowColor, shadowOffsetX: shadowOffsetX, shadowOffsetY: shadowOffsetY,
                                      shadowBlurSigma: shadowBlurSigma, hasShadow: hasShadow)
        } else {
            renderer.insertText(text)
        }
        resetCursorBlink()
        onCursorChanged?()
    }
    
    func deleteBackward() {
        renderer.deleteBackward()
        resetCursorBlink()
        onCursorChanged?()
    }
}

// MARK: - UITextInputTraits

extension SkiaMetalView: UITextInputTraits {
    var keyboardType: UIKeyboardType {
        get { .default }
        set { }
    }
    
    var returnKeyType: UIReturnKeyType {
        get { .default }
        set { }
    }
    
    var autocorrectionType: UITextAutocorrectionType {
        get { .no }
        set { }
    }
    
    var autocapitalizationType: UITextAutocapitalizationType {
        get { .none }
        set { }
    }
}

// MARK: - Key Commands (Hardware Keyboard)

extension SkiaMetalView {
    override var keyCommands: [UIKeyCommand]? {
        return [
            // Arrow keys
            UIKeyCommand(input: UIKeyCommand.inputLeftArrow, modifierFlags: [], action: #selector(handleLeftArrow)),
            UIKeyCommand(input: UIKeyCommand.inputRightArrow, modifierFlags: [], action: #selector(handleRightArrow)),
            UIKeyCommand(input: UIKeyCommand.inputUpArrow, modifierFlags: [], action: #selector(handleUpArrow)),
            UIKeyCommand(input: UIKeyCommand.inputDownArrow, modifierFlags: [], action: #selector(handleDownArrow)),
            
            // Arrow keys with Shift (selection)
            UIKeyCommand(input: UIKeyCommand.inputLeftArrow, modifierFlags: .shift, action: #selector(handleShiftLeftArrow)),
            UIKeyCommand(input: UIKeyCommand.inputRightArrow, modifierFlags: .shift, action: #selector(handleShiftRightArrow)),
            UIKeyCommand(input: UIKeyCommand.inputUpArrow, modifierFlags: .shift, action: #selector(handleShiftUpArrow)),
            UIKeyCommand(input: UIKeyCommand.inputDownArrow, modifierFlags: .shift, action: #selector(handleShiftDownArrow)),
            
            // Option + Arrow (word navigation)
            UIKeyCommand(input: UIKeyCommand.inputLeftArrow, modifierFlags: .alternate, action: #selector(handleOptionLeftArrow)),
            UIKeyCommand(input: UIKeyCommand.inputRightArrow, modifierFlags: .alternate, action: #selector(handleOptionRightArrow)),
            
            // Option + Shift + Arrow (word selection)
            UIKeyCommand(input: UIKeyCommand.inputLeftArrow, modifierFlags: [.alternate, .shift], action: #selector(handleOptionShiftLeftArrow)),
            UIKeyCommand(input: UIKeyCommand.inputRightArrow, modifierFlags: [.alternate, .shift], action: #selector(handleOptionShiftRightArrow)),
            
            // Command + Arrow (line/document navigation)
            UIKeyCommand(input: UIKeyCommand.inputLeftArrow, modifierFlags: .command, action: #selector(handleCommandLeftArrow)),
            UIKeyCommand(input: UIKeyCommand.inputRightArrow, modifierFlags: .command, action: #selector(handleCommandRightArrow)),
            UIKeyCommand(input: UIKeyCommand.inputUpArrow, modifierFlags: .command, action: #selector(handleCommandUpArrow)),
            UIKeyCommand(input: UIKeyCommand.inputDownArrow, modifierFlags: .command, action: #selector(handleCommandDownArrow)),
            
            // Command + Shift + Arrow (line/document selection)
            UIKeyCommand(input: UIKeyCommand.inputLeftArrow, modifierFlags: [.command, .shift], action: #selector(handleCommandShiftLeftArrow)),
            UIKeyCommand(input: UIKeyCommand.inputRightArrow, modifierFlags: [.command, .shift], action: #selector(handleCommandShiftRightArrow)),
            UIKeyCommand(input: UIKeyCommand.inputUpArrow, modifierFlags: [.command, .shift], action: #selector(handleCommandShiftUpArrow)),
            UIKeyCommand(input: UIKeyCommand.inputDownArrow, modifierFlags: [.command, .shift], action: #selector(handleCommandShiftDownArrow)),
            
            // Delete keys
            UIKeyCommand(input: "\u{7F}", modifierFlags: .alternate, action: #selector(handleOptionDelete)),  // Option+Backspace
            
            // Select all
            UIKeyCommand(input: "a", modifierFlags: .command, action: #selector(handleSelectAll)),
        ]
    }
    
    private func handleKeyCommand(_ action: () -> Void,
                                  resetBlink: Bool = true,
                                  clearTypingStyle: Bool = true,
                                  needsDisplay: Bool = false) {
        action()
        if resetBlink {
            resetCursorBlink()
        }
        if clearTypingStyle {
            typingStyle = nil
        }
        if needsDisplay {
            setNeedsDisplay()
        }
        onCursorChanged?()
    }
    
    // Arrow keys
    @objc private func handleLeftArrow() { handleKeyCommand({ moveCursorLeft() }) }
    @objc private func handleRightArrow() { handleKeyCommand({ moveCursorRight() }) }
    @objc private func handleUpArrow() { handleKeyCommand({ moveCursorUp() }) }
    @objc private func handleDownArrow() { handleKeyCommand({ moveCursorDown() }) }
    
    // Arrow keys with Shift
    @objc private func handleShiftLeftArrow() { handleKeyCommand({ moveCursorLeft(extendSelection: true) }) }
    @objc private func handleShiftRightArrow() { handleKeyCommand({ moveCursorRight(extendSelection: true) }) }
    @objc private func handleShiftUpArrow() { handleKeyCommand({ moveCursorUp(extendSelection: true) }) }
    @objc private func handleShiftDownArrow() { handleKeyCommand({ moveCursorDown(extendSelection: true) }) }
    
    // Option + Arrow (word)
    @objc private func handleOptionLeftArrow() { handleKeyCommand({ moveCursorToWordStart() }) }
    @objc private func handleOptionRightArrow() { handleKeyCommand({ moveCursorToWordEnd() }) }
    
    // Option + Shift + Arrow (word selection)
    @objc private func handleOptionShiftLeftArrow() { handleKeyCommand({ moveCursorToWordStart(extendSelection: true) }) }
    @objc private func handleOptionShiftRightArrow() { handleKeyCommand({ moveCursorToWordEnd(extendSelection: true) }) }
    
    // Command + Arrow (line/document)
    @objc private func handleCommandLeftArrow() { handleKeyCommand({ moveCursorToLineStart() }) }
    @objc private func handleCommandRightArrow() { handleKeyCommand({ moveCursorToLineEnd() }) }
    @objc private func handleCommandUpArrow() { handleKeyCommand({ moveCursorToDocumentStart() }) }
    @objc private func handleCommandDownArrow() { handleKeyCommand({ moveCursorToDocumentEnd() }) }
    
    // Command + Shift + Arrow (line/document selection)
    @objc private func handleCommandShiftLeftArrow() { handleKeyCommand({ moveCursorToLineStart(extendSelection: true) }) }
    @objc private func handleCommandShiftRightArrow() { handleKeyCommand({ moveCursorToLineEnd(extendSelection: true) }) }
    @objc private func handleCommandShiftUpArrow() { handleKeyCommand({ moveCursorToDocumentStart(extendSelection: true) }) }
    @objc private func handleCommandShiftDownArrow() { handleKeyCommand({ moveCursorToDocumentEnd(extendSelection: true) }) }
    
    // Delete
    @objc private func handleOptionDelete() { handleKeyCommand({ deleteWordBackward() }, clearTypingStyle: false) }
    
    // Select all
    @objc private func handleSelectAll() { handleKeyCommand({ selectAll() }, resetBlink: false, clearTypingStyle: false, needsDisplay: true) }
}
