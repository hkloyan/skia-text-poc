import { defineConfig } from 'vitest/config'
import { resolve } from 'path'

export default defineConfig({
  test: {
    browser: {
      enabled: true,
      provider: 'playwright',
      instances: [
        { browser: 'chromium' },
      ],
      headless: true,
    },
    // Increase timeout for WASM loading
    testTimeout: 30000,
    hookTimeout: 30000,
    // Setup file runs before each test file
    setupFiles: ['./setup.ts'],
    // Include pattern
    include: ['**/*.test.ts'],
  },
  // Serve static files for tests
  publicDir: resolve(__dirname, 'public'),
  server: {
    fs: {
      // Allow serving files from parent directories
      allow: ['..'],
    },
  },
})
