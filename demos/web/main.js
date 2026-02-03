let Module = null;
let dpr = 1;
let currentMaxWidth = 700;

// Text editing state
const textOffset = 50;  // Must match render position
let cursorVisible = false;
let cursorBlinkInterval = null;
let isEditing = false;
let isDragging = false;
let lastClickTime = 0;
let clickCount = 0;

// Typing style state (null means inherit from left)
let typingStyle = null;

// Hidden text input element (created during init)
let textInput = null;

// Focus the hidden text input
function focusTextInput() {
    if (!textInput) return;
    try {
        textInput.value = '';
        textInput.focus({ preventScroll: true });
    } catch {
        textInput.value = '';
        textInput.focus();
    }
}

// Insert text with optional styling
function insertTextValue(value) {
    if (!value || !Module) return;
    if (typingStyle) {
        Module.insertStyledText(
            value,
            typingStyle.fontFamily,
            typingStyle.fontSize,
            typingStyle.color,
            typingStyle.fontWeight,
            typingStyle.italic,
            typingStyle.underline,
            typingStyle.letterSpacing || 0,
            typingStyle.wordSpacing || 0,
            typingStyle.backgroundColor || 0x00000000,
            !!typingStyle.hasBackground,
            typingStyle.shadowColor || 0xFF000000,
            typingStyle.shadowOffsetX || 0,
            typingStyle.shadowOffsetY || 0,
            typingStyle.shadowBlurSigma || 0,
            !!typingStyle.hasShadow
        );
    } else {
        Module.insertText(value);
    }
    resetCursorBlink();
    updateMetrics();
    updateToolbarState();
}

// Get current typing style (from UI or cursor position)
function getCurrentStyle() {
    if (typingStyle) return typingStyle;
    if (!Module) return getDefaultStyle();
    return Module.getStyleAtCursor() || getDefaultStyle();
}

function getDefaultStyle() {
    return {
        fontFamily: 'Roboto',
        fontSize: 24,
        color: 0xFF000000,
        fontWeight: 400,
        italic: false,
        underline: false,
        letterSpacing: 0,
        wordSpacing: 0,
        hasBackground: false,
        backgroundColor: 0x00000000,
        hasShadow: false,
        shadowColor: 0xFF000000,
        shadowOffsetX: 0,
        shadowOffsetY: 0,
        shadowBlurSigma: 0
    };
}

// Update toolbar UI from a style object
function updateToolbarFromStyle(style) {
    if (!style) return;
    
    document.getElementById('fontFamily').value = style.fontFamily;
    
    // Find closest font size option in the select dropdown
    const fontSizeSelect = document.getElementById('fontSize');
    const targetSize = Math.round(style.fontSize);
    const options = Array.from(fontSizeSelect.options).map(o => parseInt(o.value));
    const closestSize = options.reduce((prev, curr) => 
        Math.abs(curr - targetSize) < Math.abs(prev - targetSize) ? curr : prev
    );
    fontSizeSelect.value = closestSize;
    
    document.getElementById('textColor').value = argbToHex(style.color);
    document.getElementById('boldBtn').classList.toggle('active', style.fontWeight >= 700);
    document.getElementById('italicBtn').classList.toggle('active', style.italic);
    document.getElementById('underlineBtn').classList.toggle('active', style.underline);
    
    const letterSpacingInput = document.getElementById('letterSpacingInput');
    if (letterSpacingInput) letterSpacingInput.value = (style.letterSpacing ?? 0).toString();
    
    const wordSpacingInput = document.getElementById('wordSpacingInput');
    if (wordSpacingInput) wordSpacingInput.value = (style.wordSpacing ?? 0).toString();
    
    const highlightToggle = document.getElementById('highlightToggle');
    const highlightColor = document.getElementById('highlightColor');
    if (highlightToggle) highlightToggle.checked = !!style.hasBackground;
    if (highlightColor) {
        if (style.hasBackground) {
            highlightColor.value = argbToHex(style.backgroundColor ?? 0x00000000);
        }
        highlightColor.disabled = !style.hasBackground;
    }
    
    const shadowToggle = document.getElementById('shadowToggle');
    if (shadowToggle) shadowToggle.checked = !!style.hasShadow;
}

// Update toolbar UI to reflect style at cursor position
function updateToolbarState() {
    if (!Module) return;
    updateToolbarFromStyle(Module.getStyleAtCursor());
}

// Convert ARGB to hex color
function argbToHex(argb) {
    const r = (argb >> 16) & 0xFF;
    const g = (argb >> 8) & 0xFF;
    const b = argb & 0xFF;
    return '#' + [r, g, b].map(x => x.toString(16).padStart(2, '0')).join('');
}

// Convert hex color to ARGB
function hexToArgb(hex) {
    const r = parseInt(hex.slice(1, 3), 16);
    const g = parseInt(hex.slice(3, 5), 16);
    const b = parseInt(hex.slice(5, 7), 16);
    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

// Get style from toolbar inputs
function getToolbarStyle() {
    const fontSize = parseFloat(document.getElementById('fontSize').value) || 24;  // Default to 24 if NaN
    const letterSpacing = parseFloat(document.getElementById('letterSpacingInput')?.value ?? '0');
    const wordSpacing = parseFloat(document.getElementById('wordSpacingInput')?.value ?? '0');
    const hasBackground = !!document.getElementById('highlightToggle')?.checked;
    const highlightColor = document.getElementById('highlightColor')?.value || '#ffff00';
    const hasShadow = !!document.getElementById('shadowToggle')?.checked;
        const shadowDefaults = {
            color: 0x80000000,
        offsetX: 1,
        offsetY: 1,
        blurSigma: 2
    };
    return {
        fontFamily: document.getElementById('fontFamily').value || 'Roboto',
        fontSize: fontSize,
        color: hexToArgb(document.getElementById('textColor').value),
        fontWeight: document.getElementById('boldBtn').classList.contains('active') ? 700 : 400,
        italic: document.getElementById('italicBtn').classList.contains('active'),
        underline: document.getElementById('underlineBtn').classList.contains('active'),
        letterSpacing: Number.isFinite(letterSpacing) ? letterSpacing : 0,
        wordSpacing: Number.isFinite(wordSpacing) ? wordSpacing : 0,
        hasBackground,
        backgroundColor: hexToArgb(highlightColor),
        hasShadow,
        shadowColor: shadowDefaults.color,
        shadowOffsetX: shadowDefaults.offsetX,
        shadowOffsetY: shadowDefaults.offsetY,
        shadowBlurSigma: shadowDefaults.blurSigma
    };
}

// Apply style - either to selection or set as typing style
function applyStyle(styleProp, value) {
    if (!Module) return;
    
    // Build new style from TOOLBAR state (which reflects user's cumulative choices)
    // This ensures multiple style changes combine correctly
    const newStyle = getToolbarStyle();
    newStyle[styleProp] = value;  // Override with the just-changed property
    
    if (Module.hasSelection()) {
        // Apply to selection
        Module.applyStyleToSelection(
            newStyle.fontFamily,
            newStyle.fontSize,
            newStyle.color,
            newStyle.fontWeight,
            newStyle.italic,
            newStyle.underline,
            newStyle.letterSpacing,
            newStyle.wordSpacing,
            newStyle.backgroundColor,
            newStyle.hasBackground,
            newStyle.shadowColor,
            newStyle.shadowOffsetX,
            newStyle.shadowOffsetY,
            newStyle.shadowBlurSigma,
            newStyle.hasShadow
        );
        render();
        // Don't update toolbar state after selection apply - keep user's choices
    } else {
        // Set as typing style for next input
        typingStyle = newStyle;
        // Update toolbar to show the typing style
        updateToolbarFromStyle(newStyle);
    }
}

// === Cursor Blink ===

function startCursorBlink() {
    stopCursorBlink();
    cursorVisible = true;
    render();
    cursorBlinkInterval = setInterval(() => {
        cursorVisible = !cursorVisible;
        render();
    }, 500);
}

function stopCursorBlink() {
    if (cursorBlinkInterval) {
        clearInterval(cursorBlinkInterval);
        cursorBlinkInterval = null;
    }
}

function resetCursorBlink() {
    startCursorBlink();
}

function render() {
    if (!Module) return;
    // All coordinates are in logical pixels - scale transform is applied in renderer
    Module.render(textOffset, textOffset, cursorVisible && isEditing);
}

// === Mouse/Touch Coordinate Conversion ===

function getLocalCoordinates(e, canvas) {
    const rect = canvas.getBoundingClientRect();
    // Coordinates are in logical pixels (CSS pixels match our coordinate system)
    const x = e.clientX - rect.left - textOffset;
    const y = e.clientY - rect.top - textOffset;
    return { x, y };
}


function renderText(maxWidth) {
    if (!Module) return;
    
    // All values are in logical pixels - scale transform is applied in renderer
    Module.setMaxWidth(maxWidth);
    Module.beginRichText();
    Module.addStyledSpanSimple("Hello, ", "Playfair", 32.0, 0xFF2196F3, 400, false, false);
    Module.addStyledSpanSimple("World! ", "Roboto", 28.0, 0xFFF44336, 700, false, false);
    Module.addStyledSpanSimple("This is ", "Roboto", 24.0, 0xFF000000, 400, false, false);
    Module.addStyledSpanSimple("rich text ", "Playfair", 26.0, 0xFF4CAF50, 400, false, false);
    Module.addStyledSpanSimple("with ", "Roboto", 24.0, 0xFF000000, 400, false, false);
    Module.addStyledSpanSimple("multiple fonts ", "Playfair-Italic", 26.0, 0xFF9C27B0, 400, false, false);
    Module.addStyledSpanSimple("and ", "Roboto", 24.0, 0xFF000000, 400, false, false);
    Module.addStyledSpanSimple("styles! ", "Roboto", 24.0, 0xFFFF9800, 400, false, true);
    Module.addStyledSpanSimple("The quick brown fox jumps over the lazy dog. We also have emojis: 😄😅🫠", "Playfair", 22.0, 0xFF666666, 400, false, false);
    Module.addStyledSpanSimple("\nClick to edit. Try typing!", "Playfair", 22.0, 0xFF666666, 400, false, false);
    Module.endRichText();
    
    // Move cursor to end initially
    Module.moveCursorToDocumentEnd(false);
    
    render();
    updateMetrics();
}

function updateMetrics() {
    if (!Module) return;
    
    // Metrics are in logical pixels
    document.getElementById('height').textContent = Module.getHeight().toFixed(2) + ' px';
    document.getElementById('width').textContent = Module.getWidth().toFixed(2) + ' px';
    document.getElementById('lineCount').textContent = Module.getLineCount();
    document.getElementById('maxIntrinsicWidth').textContent = Module.getMaxIntrinsicWidth().toFixed(2) + ' px';
    document.getElementById('minIntrinsicWidth').textContent = Module.getMinIntrinsicWidth().toFixed(2) + ' px';
}

function saveCanvas() {
    if (!Module) return;
    
    // Save current state
    const hadSelection = Module.hasSelection();
    const selection = Module.getSelection();
    const cursorPosition = Module.getCursorPosition();
    
    // Temporarily clear selection and render without cursor
    Module.clearSelection();
    Module.render(textOffset, textOffset, false);
    
    // Take snapshot
    const canvas = document.getElementById('canvas');
    const link = document.createElement('a');
    link.download = 'skia-text-render.png';
    link.href = canvas.toDataURL('image/png');
    link.click();
    
    // Restore state
    if (hadSelection) {
        Module.setSelection(selection.start, selection.end);
    } else {
        Module.setCursorPosition(cursorPosition);
    }
    render();
}

// === Setup Functions ===

function setupControlHandlers() {
    const slider = document.getElementById('maxWidthSlider');
    const maxWidthValue = document.getElementById('maxWidthValue');
    const textAlign = document.getElementById('textAlign');
    const maxLinesInput = document.getElementById('maxLinesInput');
    const ellipsisInput = document.getElementById('ellipsisInput');
    const lineHeightInput = document.getElementById('lineHeightInput');

    slider.addEventListener('input', (e) => {
        currentMaxWidth = parseInt(e.target.value, 10);
        maxWidthValue.textContent = currentMaxWidth;
        Module.setMaxWidth(currentMaxWidth);
        render();
        updateMetrics();
    });

    const applyMaxLines = () => {
        const maxLines = parseInt(maxLinesInput.value, 10);
        const clamped = Number.isFinite(maxLines) ? maxLines : 0;
        Module.setMaxLines(clamped);
        Module.setEllipsis(clamped > 0 ? (ellipsisInput.value || '...') : '');
        render();
        updateMetrics();
    };

    if (textAlign && maxLinesInput && ellipsisInput && lineHeightInput) {
        textAlign.addEventListener('change', (e) => {
            Module.setTextAlignment(parseInt(e.target.value, 10));
            render();
            updateMetrics();
        });
        maxLinesInput.addEventListener('input', applyMaxLines);
        ellipsisInput.addEventListener('input', applyMaxLines);
        lineHeightInput.addEventListener('input', (e) => {
            const val = parseFloat(e.target.value);
            Module.setLineHeight(Number.isFinite(val) ? val : 0);
            render();
            updateMetrics();
        });
    }
}

function setupInlineStyleHandlers() {
    const letterSpacingInput = document.getElementById('letterSpacingInput');
    const wordSpacingInput = document.getElementById('wordSpacingInput');
    const highlightToggle = document.getElementById('highlightToggle');
    const highlightColor = document.getElementById('highlightColor');
    const shadowToggle = document.getElementById('shadowToggle');

    if (highlightColor && highlightToggle) {
        highlightColor.disabled = !highlightToggle.checked;
    }

    if (letterSpacingInput) {
        letterSpacingInput.addEventListener('input', (e) => {
            const val = parseFloat(e.target.value);
            applyStyle('letterSpacing', Number.isFinite(val) ? val : 0);
            focusTextInput();
        });
    }

    if (wordSpacingInput) {
        wordSpacingInput.addEventListener('input', (e) => {
            const val = parseFloat(e.target.value);
            applyStyle('wordSpacing', Number.isFinite(val) ? val : 0);
            focusTextInput();
        });
    }

    if (highlightToggle && highlightColor) {
        highlightToggle.addEventListener('change', (e) => {
            highlightColor.disabled = !e.target.checked;
            applyStyle('hasBackground', e.target.checked);
            focusTextInput();
        });
        highlightColor.addEventListener('input', (e) => {
            applyStyle('backgroundColor', hexToArgb(e.target.value));
            focusTextInput();
        });
    }

    if (shadowToggle) {
        shadowToggle.addEventListener('change', (e) => {
            applyStyle('hasShadow', e.target.checked);
            focusTextInput();
        });
    }
}

function setupToolbarHandlers() {
    // Prevent toolbar buttons from stealing focus
    document.getElementById('formatToolbar').addEventListener('mousedown', (e) => {
        const tag = e.target.tagName.toLowerCase();
        if (tag !== 'select' && tag !== 'input' && tag !== 'option') {
            e.preventDefault();
        }
    });

    document.getElementById('fontFamily').addEventListener('change', (e) => {
        applyStyle('fontFamily', e.target.value);
        focusTextInput();
    });

    document.getElementById('fontSize').addEventListener('change', (e) => {
        applyStyle('fontSize', parseFloat(e.target.value));
        focusTextInput();
    });

    document.getElementById('textColor').addEventListener('input', (e) => {
        applyStyle('color', hexToArgb(e.target.value));
        focusTextInput();
    });

    document.getElementById('boldBtn').addEventListener('click', () => {
        const isActive = document.getElementById('boldBtn').classList.toggle('active');
        applyStyle('fontWeight', isActive ? 700 : 400);
        focusTextInput();
    });

    document.getElementById('italicBtn').addEventListener('click', () => {
        const isActive = document.getElementById('italicBtn').classList.toggle('active');
        applyStyle('italic', isActive);
        focusTextInput();
    });

    document.getElementById('underlineBtn').addEventListener('click', () => {
        const isActive = document.getElementById('underlineBtn').classList.toggle('active');
        applyStyle('underline', isActive);
        focusTextInput();
    });
}

function setupCanvasHandlers(canvas) {
    canvas.addEventListener('mousedown', (e) => {
        e.preventDefault();
        const { x, y } = getLocalCoordinates(e, canvas);
        
        isEditing = true;
        isDragging = true;
        typingStyle = null;
        
        const now = Date.now();
        if (now - lastClickTime < 300) {
            clickCount++;
        } else {
            clickCount = 1;
        }
        lastClickTime = now;
        
        if (clickCount === 2) {
            Module.setWordSelectionAtCoordinate(x, y);
        } else if (clickCount >= 3) {
            Module.setLineSelectionAtCoordinate(x, y);
            clickCount = 0;
        } else {
            Module.beginSelectionAtCoordinate(x, y);
        }
        
        resetCursorBlink();
        focusTextInput();
        updateToolbarState();
    });

    canvas.addEventListener('mousemove', (e) => {
        if (!isDragging || clickCount !== 1) return;
        const { x, y } = getLocalCoordinates(e, canvas);
        Module.extendSelectionToCoordinate(x, y);
        render();
    });

    canvas.addEventListener('mouseup', () => {
        isDragging = false;
        if (isEditing) focusTextInput();
    });

    canvas.addEventListener('mouseleave', () => {
        isDragging = false;
    });
}

function setupTextInput() {
    textInput = document.createElement('textarea');
    textInput.setAttribute('aria-hidden', 'true');
    textInput.autocapitalize = 'off';
    textInput.autocomplete = 'off';
    textInput.spellcheck = false;
    Object.assign(textInput.style, {
        position: 'fixed',
        opacity: '0',
        left: '-9999px',
        top: '0',
        width: '1px',
        height: '1px',
        pointerEvents: 'auto'
    });
    document.body.appendChild(textInput);
}

function setupKeyboardHandlers() {
    textInput.addEventListener('keydown', (e) => {
        if (!isEditing) return;
        
        const shift = e.shiftKey;
        const cmd = e.metaKey || e.ctrlKey;
        const alt = e.altKey;
        let handled = true;
        
        switch (e.key) {
            case 'ArrowLeft':
                if (cmd && shift) Module.moveCursorToLineStart(true);
                else if (cmd) Module.moveCursorToLineStart(false);
                else if (alt && shift) Module.moveCursorToWordStart(true);
                else if (alt) Module.moveCursorToWordStart(false);
                else Module.moveCursorLeft(shift);
                break;
            case 'ArrowRight':
                if (cmd && shift) Module.moveCursorToLineEnd(true);
                else if (cmd) Module.moveCursorToLineEnd(false);
                else if (alt && shift) Module.moveCursorToWordEnd(true);
                else if (alt) Module.moveCursorToWordEnd(false);
                else Module.moveCursorRight(shift);
                break;
            case 'ArrowUp':
                if (cmd && shift) Module.moveCursorToDocumentStart(true);
                else if (cmd) Module.moveCursorToDocumentStart(false);
                else Module.moveCursorUp(shift);
                break;
            case 'ArrowDown':
                if (cmd && shift) Module.moveCursorToDocumentEnd(true);
                else if (cmd) Module.moveCursorToDocumentEnd(false);
                else Module.moveCursorDown(shift);
                break;
            case 'Backspace':
                if (alt) Module.deleteWordBackward();
                else Module.deleteBackward();
                break;
            case 'Delete':
                if (alt) Module.deleteWordForward();
                else Module.deleteForward();
                break;
            case 'Home':
                if (cmd) Module.moveCursorToDocumentStart(shift);
                else Module.moveCursorToLineStart(shift);
                break;
            case 'End':
                if (cmd) Module.moveCursorToDocumentEnd(shift);
                else Module.moveCursorToLineEnd(shift);
                break;
            case 'a':
                if (cmd) Module.selectAll();
                else handled = false;
                break;
            case 'Enter':
                handled = false;
                break;
            default:
                handled = false;
        }
        
        if (handled) {
            e.preventDefault();
            resetCursorBlink();
            updateMetrics();
            updateToolbarState();
            typingStyle = null;
        }
    });

    textInput.addEventListener('compositionend', () => {
        const value = textInput.value;
        if (value) {
            insertTextValue(value);
            textInput.value = '';
        }
    });

    textInput.addEventListener('input', (e) => {
        if (!isEditing) return;
        if (e.isComposing || e.inputType === 'insertCompositionText') return;
        
        if (e.inputType === 'insertLineBreak' || e.inputType === 'insertParagraph') {
            insertTextValue('\n');
            textInput.value = '';
            return;
        }
        
        const value = textInput.value;
        if (value) {
            insertTextValue(value);
            textInput.value = '';
        }
    });

    textInput.addEventListener('paste', (e) => {
        if (!isEditing) return;
        const text = e.clipboardData?.getData('text');
        if (text) {
            e.preventDefault();
            insertTextValue(text);
            textInput.value = '';
        }
    });

    textInput.addEventListener('blur', () => {
        cursorVisible = false;
        render();
        stopCursorBlink();
    });

    textInput.addEventListener('focus', () => {
        if (isEditing) resetCursorBlink();
    });
}

// Try to load emoji font from system fonts
// Returns true if successful, false otherwise
async function tryLoadEmojiFont() {
    if (!('queryLocalFonts' in window)) {
        return false;
    }
    
    try {
        const fonts = await window.queryLocalFonts();
        const emojiFont = fonts.find(f => 
            f.family === 'Apple Color Emoji' || 
            f.family === 'Segoe UI Emoji' ||
            f.family === 'Noto Color Emoji'
        );
        
        if (emojiFont) {
            const blob = await emojiFont.blob();
            const buffer = await blob.arrayBuffer();
            const data = new Uint8Array(buffer);
            
            const ptr = Module._malloc(data.length);
            Module.HEAPU8.set(data, ptr);
            Module.registerFont('System Emoji', ptr, data.length);
            Module._free(ptr);
            
            console.log('Emoji font loaded:', emojiFont.family);
            return emojiFont.family;
        }
    } catch (e) {
        // Permission not granted or other error - this is expected on first visit
        console.log('Emoji font auto-load skipped:', e.message);
    }
    return false;
}

function setupEmojiFontButton(alreadyLoaded) {
    const emojiFontBtn = document.getElementById('loadEmojiFontBtn');
    
    // If already loaded on init, update button state
    if (alreadyLoaded) {
        emojiFontBtn.disabled = true;
        emojiFontBtn.textContent = alreadyLoaded + ' loaded';
        emojiFontBtn.style.background = 'linear-gradient(135deg, #00ff88, #00d9ff)';
        return;
    }
    
    emojiFontBtn.addEventListener('click', async () => {
        if (!('queryLocalFonts' in window)) {
            alert('Local Font Access API not supported in this browser. Try Chrome.');
            return;
        }
        
        try {
            emojiFontBtn.disabled = true;
            emojiFontBtn.textContent = 'Loading...';
            
            const loadedFont = await tryLoadEmojiFont();
            
            if (loadedFont) {
                emojiFontBtn.textContent = loadedFont + ' loaded';
                emojiFontBtn.style.background = 'linear-gradient(135deg, #00ff88, #00d9ff)';
                renderText(currentMaxWidth);
            } else {
                emojiFontBtn.disabled = false;
                emojiFontBtn.textContent = '❌ No emoji font found';
            }
        } catch (e) {
            console.error('Failed to load emoji font:', e);
            emojiFontBtn.disabled = false;
            emojiFontBtn.textContent = '❌ ' + e.message;
        }
    });
}

async function init() {
    const statusEl = document.getElementById('status');
    const saveBtn = document.getElementById('saveBtn');
    const canvas = document.getElementById('canvas');
    
    try {
        statusEl.textContent = 'Loading WASM module...';
        statusEl.className = 'status loading';
        
        // Use fixed 3x scale to match iOS retina resolution
        // TODO: Use devicePixelRatio instead of hardcoded 3x scale
        dpr = 3;
        const displayWidth = 800;
        const displayHeight = 400;
        
        canvas.style.width = displayWidth + 'px';
        canvas.style.height = displayHeight + 'px';
        canvas.width = displayWidth * dpr;
        canvas.height = displayHeight * dpr;
        canvas.tabIndex = -1;
        canvas.style.outline = 'none';
        
        // Load WASM module
        Module = await SkiaTextModule({
            locateFile: (path) => `dist/${path}`,
            preserveDrawingBuffer: true
        });
        
        statusEl.textContent = 'Initializing Skia...';
        if (!Module.initSkia(canvas.width, canvas.height)) {
            throw new Error('Failed to initialize Skia');
        }
        
        statusEl.textContent = 'Loading fonts...';
        
        async function loadFont(name, path) {
            const response = await fetch(path);
            if (!response.ok) throw new Error(`Failed to load font ${name}: ${response.status}`);
            const buffer = await response.arrayBuffer();
            const data = new Uint8Array(buffer);
            const ptr = Module._malloc(data.length);
            Module.HEAPU8.set(data, ptr);
            Module.registerFont(name, ptr, data.length);
            Module._free(ptr);
        }
        
        await loadFont('Roboto', '../../assets/fonts/Roboto-Regular.ttf');
        await loadFont('Playfair', '../../assets/fonts/PlayfairDisplay-Regular.ttf');
        await loadFont('Playfair-Italic', '../../assets/fonts/PlayfairDisplay-Italic.ttf');
        
        // Try to auto-load emoji font if permission was previously granted
        const emojiFontLoaded = await tryLoadEmojiFont();
        
        statusEl.textContent = 'Rendering text...';
        
        Module.createTextRenderer();
        Module.setScale(dpr);
        renderText(currentMaxWidth);
        
        // Setup all event handlers
        saveBtn.disabled = false;
        saveBtn.addEventListener('click', saveCanvas);
        
        setupTextInput();
        setupControlHandlers();
        setupInlineStyleHandlers();
        setupToolbarHandlers();
        setupCanvasHandlers(canvas);
        setupKeyboardHandlers();
        setupEmojiFontButton(emojiFontLoaded);
        
        statusEl.textContent = 'Ready';
        statusEl.className = 'status ready';
        console.log('Skia Text PoC initialized successfully');
        
    } catch (error) {
        console.error('Initialization failed:', error);
        statusEl.textContent = `Error: ${error.message}`;
        statusEl.className = 'status error';
    }
}

// Start initialization when DOM is ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
} else {
    init();
}

// Cleanup on page unload
window.addEventListener('beforeunload', () => {
    if (Module && Module.destroySkia) {
        Module.destroySkia();
    }
});
