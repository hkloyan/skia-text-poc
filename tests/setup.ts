/**
 * Test setup - loads WASM module and fonts before tests run.
 */

import { beforeAll, afterAll } from 'vitest'

// eslint-disable-next-line @typescript-eslint/no-explicit-any
type SkiaTextModule = any

// Global module instance shared across tests
let module: SkiaTextModule | null = null
let canvas: HTMLCanvasElement | null = null

// Default text style for tests
export const DEFAULT_STYLE = {
  fontFamily: 'Roboto',
  fontSize: 24,
  color: 0xff000000,
  fontWeight: 400,
  italic: false,
  underline: false,
}

/**
 * Get the initialized module instance.
 * Call this in your tests after setup has run.
 */
export function getModule(): SkiaTextModule {
  if (!module) {
    throw new Error('Module not initialized. Make sure setup has completed.')
  }
  return module
}

/**
 * Helper to set simple text on the renderer.
 */
export function setTextSimple(text: string, style = DEFAULT_STYLE): void {
  const m = getModule()
  m.setTextSimple(
    text,
    style.fontFamily,
    style.fontSize,
    style.color,
    style.fontWeight,
    style.italic,
    style.underline
  )
}

/**
 * Helper to load a font file.
 */
async function loadFont(m: SkiaTextModule, name: string, url: string): Promise<void> {
  const response = await fetch(url)
  if (!response.ok) {
    throw new Error(`Failed to load font ${name}: ${response.status}`)
  }
  const buffer = await response.arrayBuffer()
  const data = new Uint8Array(buffer)
  const ptr = m._malloc(data.length)
  m.HEAPU8.set(data, ptr)
  m.registerFont(name, ptr, data.length)
  m._free(ptr)
}

/**
 * Dynamically load the WASM module script.
 */
// eslint-disable-next-line @typescript-eslint/no-explicit-any
async function loadWasmScript(): Promise<any> {
  return new Promise((resolve, reject) => {
    // Check if already loaded
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    const existing = (window as any).SkiaTextModule
    if (existing) {
      resolve(existing)
      return
    }

    const script = document.createElement('script')
    script.src = '/dist/skia_text.js'
    script.onload = () => {
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      const factory = (window as any).SkiaTextModule
      if (factory) {
        resolve(factory)
      } else {
        reject(new Error('SkiaTextModule not found after script load'))
      }
    }
    script.onerror = () => reject(new Error('Failed to load skia_text.js'))
    document.head.appendChild(script)
  })
}

beforeAll(async () => {
  // Create a canvas element for WebGL context
  canvas = document.createElement('canvas')
  canvas.id = 'canvas' // Must be 'canvas' to match the ID expected by initSkia
  canvas.width = 800
  canvas.height = 600
  canvas.style.position = 'absolute'
  canvas.style.left = '-9999px' // Hide off-screen
  document.body.appendChild(canvas)

  // Dynamically load and initialize the WASM module
  const factory = await loadWasmScript()

  module = await factory({
    locateFile: (path: string) => `/dist/${path}`,
  })

  // Initialize Skia with WebGL
  const success = module.initSkia(canvas.width, canvas.height)
  if (!success) {
    throw new Error('Failed to initialize Skia')
  }

  // Load test fonts
  await loadFont(module, 'Roboto', '/fonts/Roboto-Regular.ttf')
  await loadFont(module, 'Roboto-Bold', '/fonts/Roboto-Bold.ttf')
  await loadFont(module, 'Playfair', '/fonts/PlayfairDisplay-Regular.ttf')
  await loadFont(module, 'Playfair-Italic', '/fonts/PlayfairDisplay-Italic.ttf')

  // Create the text renderer
  module.createTextRenderer()
  module.setScale(1) // Use 1x scale for predictable metrics in tests
})

afterAll(() => {
  if (module) {
    module.destroySkia()
    module = null
  }
  if (canvas) {
    canvas.remove()
    canvas = null
  }
})
