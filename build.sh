#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

echo "==> Configuring NVCS build..."
cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$SCRIPT_DIR/dist" \
    "$@"

echo "==> Building..."
cmake --build "$BUILD_DIR" --parallel "$(nproc)"

echo "==> Installing to dist/..."
cmake --install "$BUILD_DIR"

echo ""
echo "Build complete!"
echo "  CLI:    $SCRIPT_DIR/dist/bin/nvcs"
echo "  Server: $SCRIPT_DIR/dist/bin/nvcs-server"
echo ""
echo "Quick start:"
echo "  mkdir my-project && cd my-project"
echo "  $SCRIPT_DIR/dist/bin/nvcs init"
echo "  echo 'hello' > README.md"
echo "  $SCRIPT_DIR/dist/bin/nvcs add README.md"
echo "  $SCRIPT_DIR/dist/bin/nvcs commit -m 'initial commit'"
echo ""
echo "Start server:"
echo "  $SCRIPT_DIR/dist/bin/nvcs-server --port 7878 --repo-dir ./repos"
