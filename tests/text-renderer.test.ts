/**
 * TextRenderer unit tests.
 *
 * Tests the core text rendering functionality including:
 * - Text layout and metrics
 * - Cursor navigation
 * - Text editing
 * - Selection handling
 */

import { describe, it, expect, beforeEach } from 'vitest'
import { getModule, setTextSimple, DEFAULT_STYLE } from './setup'

type RichTextSpan = {
  text: string
  style?: Partial<typeof DEFAULT_STYLE>
}

function setRichTextSimple(spans: RichTextSpan[]): void {
  const m = getModule()
  m.beginRichText()
  spans.forEach(({ text, style }) => {
    const merged = { ...DEFAULT_STYLE, ...(style ?? {}) }
    m.addStyledSpanSimple(
      text,
      merged.fontFamily,
      merged.fontSize,
      merged.color,
      merged.fontWeight,
      merged.italic,
      merged.underline
    )
  })
  m.endRichText()
}

describe('TextRenderer', () => {
  beforeEach(() => {
    // Reset state before each test
    const m = getModule()
    m.beginRichText()
    m.endRichText() // Clear text
    m.createTextRenderer() // Fresh renderer
    m.setScale(1)
  })

  describe('Layout Metrics', () => {
    it('returns zero height for empty text', () => {
      const m = getModule()
      m.setMaxWidth(500)
      expect(m.getHeight()).toBe(0)
    })

    it('returns positive height for non-empty text', () => {
      const m = getModule()
      setTextSimple('Hello World')
      m.setMaxWidth(500)
      expect(m.getHeight()).toBeGreaterThan(0)
    })

    it('returns correct line count for single line', () => {
      const m = getModule()
      setTextSimple('Hello World')
      m.setMaxWidth(500)
      expect(m.getLineCount()).toBe(1)
    })

    it('wraps text when exceeding maxWidth', () => {
      const m = getModule()
      setTextSimple('This is a very long text that should definitely wrap to multiple lines')
      m.setMaxWidth(100)
      expect(m.getLineCount()).toBeGreaterThan(1)
    })

    it('returns layout width equal to maxWidth', () => {
      const m = getModule()
      setTextSimple('Hello World')
      m.setMaxWidth(250)
      expect(m.getWidth()).toBeCloseTo(250, 5)
    })

    it('returns intrinsic widths for non-empty text', () => {
      const m = getModule()
      setTextSimple('Hello World')
      m.setMaxWidth(500)
      const minWidth = m.getMinIntrinsicWidth()
      const maxWidth = m.getMaxIntrinsicWidth()
      expect(minWidth).toBeGreaterThan(0)
      expect(maxWidth).toBeGreaterThan(minWidth)
    })

    it('returns text length in UTF-16 code units', () => {
      const m = getModule()
      setTextSimple('Hello')
      expect(m.getTextLength()).toBe(5)
    })

    it('counts emoji as multiple code units', () => {
      const m = getModule()
      // 👨‍👩‍👧‍👦 is 11 UTF-16 code units (surrogate pairs + ZWJ)
      setTextSimple('A👨‍👩‍👧‍👦B')
      expect(m.getTextLength()).toBe(13)
    })
  })

  describe('Rich Text', () => {
    it('applies distinct styles per span', () => {
      const m = getModule()
      setRichTextSimple([
        { text: 'Hello ', style: { fontFamily: 'Roboto', fontSize: 20, color: 0xff000000 } },
        {
          text: 'World',
          style: {
            fontFamily: 'Playfair',
            fontSize: 30,
            color: 0xff112233,
            fontWeight: 700,
            italic: true,
            underline: true,
          },
        },
      ])

      expect(m.getText()).toBe('Hello World')
      m.setCursorPosition(1)
      let style = m.getStyleAtCursor()
      expect(style.fontFamily).toBe('Roboto')
      expect(style.fontSize).toBe(20)
      expect(style.color >>> 0).toBe(0xff000000)

      m.setCursorPosition(7) // Inside "World"
      style = m.getStyleAtCursor()
      expect(style.fontFamily).toBe('Playfair')
      expect(style.fontSize).toBe(30)
      expect(style.color >>> 0).toBe(0xff112233)
      expect(style.fontWeight).toBe(700)
      expect(style.italic).toBe(true)
      expect(style.underline).toBe(true)
    })

    it('insertText at span boundary inherits left style', () => {
      const m = getModule()
      setRichTextSimple([
        { text: 'Hello', style: { fontFamily: 'Roboto' } },
        { text: 'World', style: { fontFamily: 'Playfair' } },
      ])

      m.setCursorPosition(5)
      m.insertText(' ')
      expect(m.getText()).toBe('Hello World')
      expect(m.getCursorPosition()).toBe(6)

      const style = m.getStyleAtCursor()
      expect(style.fontFamily).toBe('Roboto')
    })
  })

  describe('Cursor Position', () => {
    it('starts at position 0', () => {
      const m = getModule()
      setTextSimple('Hello')
      expect(m.getCursorPosition()).toBe(0)
    })

    it('setCursorPosition moves cursor', () => {
      const m = getModule()
      setTextSimple('Hello')
      m.setCursorPosition(3)
      expect(m.getCursorPosition()).toBe(3)
    })

    it('clamps cursor to text length', () => {
      const m = getModule()
      setTextSimple('Hello')
      m.setCursorPosition(100)
      expect(m.getCursorPosition()).toBe(5)
    })

    it('clamps negative cursor to 0', () => {
      const m = getModule()
      setTextSimple('Hello')
      m.setCursorPosition(-5)
      expect(m.getCursorPosition()).toBe(0)
    })
  })

  describe('Cursor Navigation', () => {
    it('moveCursorRight advances by one', () => {
      const m = getModule()
      setTextSimple('Hello')
      m.setCursorPosition(0)
      m.moveCursorRight(false)
      expect(m.getCursorPosition()).toBe(1)
    })

    it('moveCursorLeft goes back by one', () => {
      const m = getModule()
      setTextSimple('Hello')
      m.setCursorPosition(3)
      m.moveCursorLeft(false)
      expect(m.getCursorPosition()).toBe(2)
    })

    it('moveCursorRight at end stays at end', () => {
      const m = getModule()
      setTextSimple('Hello')
      m.setCursorPosition(5)
      m.moveCursorRight(false)
      expect(m.getCursorPosition()).toBe(5)
    })

    it('moveCursorLeft at start stays at start', () => {
      const m = getModule()
      setTextSimple('Hello')
      m.setCursorPosition(0)
      m.moveCursorLeft(false)
      expect(m.getCursorPosition()).toBe(0)
    })

    it('moveCursorToDocumentStart goes to position 0', () => {
      const m = getModule()
      setTextSimple('Hello World')
      m.setCursorPosition(6)
      m.moveCursorToDocumentStart(false)
      expect(m.getCursorPosition()).toBe(0)
    })

    it('moveCursorToDocumentEnd goes to text length', () => {
      const m = getModule()
      setTextSimple('Hello World')
      m.setCursorPosition(0)
      m.moveCursorToDocumentEnd(false)
      expect(m.getCursorPosition()).toBe(11)
    })
  })

  describe('Text Editing', () => {
    it('insertText adds text at cursor', () => {
      const m = getModule()
      setTextSimple('Hello')
      m.setCursorPosition(5)
      m.insertText(' World')
      expect(m.getText()).toBe('Hello World')
    })

    it('insertText in middle inserts correctly', () => {
      const m = getModule()
      setTextSimple('Helo')
      m.setCursorPosition(2)
      m.insertText('l')
      expect(m.getText()).toBe('Hello')
    })

    it('deleteBackward removes character before cursor', () => {
      const m = getModule()
      setTextSimple('Hello')
      m.setCursorPosition(5)
      m.deleteBackward()
      expect(m.getText()).toBe('Hell')
    })

    it('deleteBackward at position 0 does nothing', () => {
      const m = getModule()
      setTextSimple('Hello')
      m.setCursorPosition(0)
      m.deleteBackward()
      expect(m.getText()).toBe('Hello')
    })

    it('deleteForward removes character after cursor', () => {
      const m = getModule()
      setTextSimple('Hello')
      m.setCursorPosition(0)
      m.deleteForward()
      expect(m.getText()).toBe('ello')
    })

    it('deleteForward at end does nothing', () => {
      const m = getModule()
      setTextSimple('Hello')
      m.setCursorPosition(5)
      m.deleteForward()
      expect(m.getText()).toBe('Hello')
    })
  })

  describe('Word Deletion', () => {
    it('deleteWordBackward removes previous word', () => {
      const m = getModule()
      setTextSimple('Hello World')
      m.setCursorPosition(11)
      m.deleteWordBackward()
      expect(m.getText()).toBe('Hello ')
      expect(m.getCursorPosition()).toBe(6)
    })

    it('deleteWordForward removes next word', () => {
      const m = getModule()
      setTextSimple('Hello World')
      m.setCursorPosition(6)
      m.deleteWordForward()
      expect(m.getText()).toBe('Hello ')
      expect(m.getCursorPosition()).toBe(6)
    })
  })

  describe('Selection', () => {
    it('hasSelection returns false initially', () => {
      const m = getModule()
      setTextSimple('Hello')
      expect(m.hasSelection()).toBe(false)
    })

    it('setSelection creates a selection', () => {
      const m = getModule()
      setTextSimple('Hello World')
      m.setSelection(0, 5)
      expect(m.hasSelection()).toBe(true)
    })

    it('getSelection returns correct range', () => {
      const m = getModule()
      setTextSimple('Hello World')
      m.setSelection(6, 11)
      const sel = m.getSelection()
      expect(sel).toEqual({ start: 6, end: 11 })
    })

    it('getSelection normalizes reversed ranges', () => {
      const m = getModule()
      setTextSimple('Hello World')
      m.setSelection(8, 2)
      const sel = m.getSelection()
      expect(sel).toEqual({ start: 2, end: 8 })
    })

    it('getSelectedText returns selected text', () => {
      const m = getModule()
      setTextSimple('Hello World')
      m.setSelection(0, 5)
      expect(m.getSelectedText()).toBe('Hello')
    })

    it('clearSelection removes selection', () => {
      const m = getModule()
      setTextSimple('Hello World')
      m.setSelection(0, 5)
      m.clearSelection()
      expect(m.hasSelection()).toBe(false)
      expect(m.getCursorPosition()).toBe(5)
    })

    it('selectAll selects entire text', () => {
      const m = getModule()
      setTextSimple('Hello World')
      m.selectAll()
      const sel = m.getSelection()
      expect(sel).toEqual({ start: 0, end: 11 })
    })

    it('insertText replaces selection', () => {
      const m = getModule()
      setTextSimple('Hello World')
      m.setSelection(6, 11)
      m.insertText('Universe')
      expect(m.getText()).toBe('Hello Universe')
    })

    it('deleteSelection removes selected text', () => {
      const m = getModule()
      setTextSimple('Hello World')
      m.setSelection(5, 11)
      m.deleteSelection()
      expect(m.getText()).toBe('Hello')
    })
  })

  describe('Text Styling', () => {
    it('getStyleAtCursor returns current style', () => {
      const m = getModule()
      const style = {
        ...DEFAULT_STYLE,
        fontFamily: 'Playfair',
        fontSize: 30,
        color: 0xff112233,
        fontWeight: 700,
        italic: true,
        underline: true,
      }
      setTextSimple('Hello', style)
      m.setCursorPosition(5)
      const current = m.getStyleAtCursor()
      expect(current.fontFamily).toBe('Playfair')
      expect(current.fontSize).toBe(30)
      expect(current.color >>> 0).toBe(0xff112233)
      expect(current.fontWeight).toBe(700)
      expect(current.italic).toBe(true)
      expect(current.underline).toBe(true)
    })

    it('applyStyleToSelectionSimple updates selection style', () => {
      const m = getModule()
      setTextSimple('Hello World')
      m.setSelection(6, 11)
      m.applyStyleToSelectionSimple('Playfair', 28, 0xff00aa00, 700, true, true)
      m.setCursorPosition(7)
      const current = m.getStyleAtCursor()
      expect(current.fontFamily).toBe('Playfair')
      expect(current.fontSize).toBe(28)
      expect(current.color >>> 0).toBe(0xff00aa00)
      expect(current.fontWeight).toBe(700)
      expect(current.italic).toBe(true)
      expect(current.underline).toBe(true)
    })

    it('insertStyledTextSimple inserts text with explicit style', () => {
      const m = getModule()
      setTextSimple('Hello')
      m.setCursorPosition(5)
      m.insertStyledTextSimple(' World', 'Playfair', 22, 0xff445566, 400, false, false)
      expect(m.getText()).toBe('Hello World')
      const current = m.getStyleAtCursor()
      expect(current.fontFamily).toBe('Playfair')
      expect(current.fontSize).toBe(22)
      expect(current.color >>> 0).toBe(0xff445566)
    })
  })

  describe('Cursor Navigation with Selection', () => {
    it('moveCursorRight with shift extends selection', () => {
      const m = getModule()
      setTextSimple('Hello')
      m.setCursorPosition(0)
      m.moveCursorRight(true) // extend selection
      m.moveCursorRight(true)
      m.moveCursorRight(true)
      expect(m.hasSelection()).toBe(true)
      expect(m.getSelectedText()).toBe('Hel')
    })

    it('moveCursorLeft collapses selection to start', () => {
      const m = getModule()
      setTextSimple('Hello')
      m.setSelection(1, 4)
      m.moveCursorLeft(false)
      expect(m.hasSelection()).toBe(false)
      expect(m.getCursorPosition()).toBe(1)
    })

    it('moveCursorRight collapses selection to end', () => {
      const m = getModule()
      setTextSimple('Hello')
      m.setSelection(1, 4)
      m.moveCursorRight(false)
      expect(m.hasSelection()).toBe(false)
      expect(m.getCursorPosition()).toBe(4)
    })
  })

  describe('Word Boundaries', () => {
    it('getWordBoundary returns word range', () => {
      const m = getModule()
      setTextSimple('Hello World')
      m.setMaxWidth(500)
      const boundary = m.getWordBoundary(2) // middle of "Hello"
      expect(boundary).not.toBeNull()
      expect(boundary?.start).toBe(0)
      expect(boundary?.end).toBe(5)
    })

    it('moveCursorToWordEnd moves to end of word', () => {
      const m = getModule()
      setTextSimple('Hello World')
      m.setMaxWidth(500)
      m.setCursorPosition(0)
      m.moveCursorToWordEnd(false)
      expect(m.getCursorPosition()).toBe(5)
    })

    it('moveCursorToWordStart moves to start of word', () => {
      const m = getModule()
      setTextSimple('Hello World')
      m.setMaxWidth(500)
      m.setCursorPosition(8) // middle of "World"
      m.moveCursorToWordStart(false)
      expect(m.getCursorPosition()).toBe(6)
    })
  })

  describe('Grapheme Cluster Handling', () => {
    it('emoji is treated as single unit for cursor movement', () => {
      const m = getModule()
      // Simple emoji (single code point)
      setTextSimple('A😀B')
      m.setCursorPosition(1) // After 'A'
      m.moveCursorRight(false)
      // Should skip entire emoji (2 UTF-16 code units for 😀)
      expect(m.getCursorPosition()).toBe(3)
    })

    it('moveCursorLeft skips entire emoji cluster', () => {
      const m = getModule()
      setTextSimple('A😀B')
      m.setCursorPosition(3) // After emoji
      m.moveCursorLeft(false)
      expect(m.getCursorPosition()).toBe(1)
    })

    it('deleteBackward removes entire emoji', () => {
      const m = getModule()
      setTextSimple('A😀B')
      m.setMaxWidth(500)
      // Move to after emoji (position 3)
      m.setCursorPosition(3)
      m.deleteBackward()
      expect(m.getText()).toBe('AB')
      expect(m.getCursorPosition()).toBe(1)
    })

    it('deleteForward removes entire emoji', () => {
      const m = getModule()
      setTextSimple('A😀B')
      m.setCursorPosition(1) // Before emoji
      m.deleteForward()
      expect(m.getText()).toBe('AB')
      expect(m.getCursorPosition()).toBe(1)
    })
  })

  describe('Multi-line Navigation', () => {
    it('moveCursorDown moves to next line', () => {
      const m = getModule()
      setTextSimple('Line 1\nLine 2')
      m.setMaxWidth(500)
      const lastLine = m.getLineBoundary(m.getTextLength() - 1)
      expect(lastLine).not.toBeNull()
      const lastLineStart = lastLine?.start ?? 0
      m.setCursorPosition(0)
      m.moveCursorDown(false)
      expect(m.getCursorPosition()).toBe(lastLineStart)
    })

    it('moveCursorUp moves to previous line', () => {
      const m = getModule()
      setTextSimple('Line 1\nLine 2')
      m.setMaxWidth(500)
      const lastLine = m.getLineBoundary(m.getTextLength() - 1)
      expect(lastLine).not.toBeNull()
      const lastLineStart = lastLine?.start ?? 0
      m.setCursorPosition(lastLineStart) // Start of "Line 2"
      m.moveCursorUp(false)
      expect(m.getCursorPosition()).toBe(0)
    })
  })

  describe('Line Boundaries', () => {
    it('getLineBoundary returns line range', () => {
      const m = getModule()
      setTextSimple('Line 1\nLine 2')
      m.setMaxWidth(500)
      const boundary = m.getLineBoundary(2) // In "Line 1"
      expect(boundary).not.toBeNull()
      expect(boundary?.start).toBe(0)
    })

    it('moveCursorToLineStart moves to start of line', () => {
      const m = getModule()
      setTextSimple('Hello World')
      m.setMaxWidth(500)
      m.setCursorPosition(6)
      m.moveCursorToLineStart(false)
      expect(m.getCursorPosition()).toBe(0)
    })

    it('moveCursorToLineEnd moves to end of line', () => {
      const m = getModule()
      setTextSimple('Hello World')
      m.setMaxWidth(500)
      m.setCursorPosition(0)
      m.moveCursorToLineEnd(false)
      expect(m.getCursorPosition()).toBe(11)
    })

    it('moveCursorToLineEnd stops before newline', () => {
      const m = getModule()
      setTextSimple('Line 1\nLine 2')
      m.setMaxWidth(500)
      m.setCursorPosition(0)
      m.moveCursorToLineEnd(false)
      expect(m.getCursorPosition()).toBe(6)
    })
  })
})
