#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "Syncing Skia dependencies..."
cd "$PROJECT_ROOT/third_party/skia"

# Ensure depot_tools is in PATH
export PATH="$HOME/repos/tools/depot_tools:$PATH"

python3 tools/git-sync-deps

echo "Skia dependencies synced successfully!"
