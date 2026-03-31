#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BINARY="$SCRIPT_DIR/dist/bin/nvcs-server"

if [[ ! -f "$BINARY" ]]; then
    echo "Server binary not found. Run ./build.sh first."
    exit 1
fi

PORT="${PORT:-7878}"
REPO_DIR="${REPO_DIR:-$SCRIPT_DIR/nvcs-repos}"
mkdir -p "$REPO_DIR"

exec "$BINARY" \
    --host 0.0.0.0 \
    --port "$PORT" \
    --repo-dir "$REPO_DIR"
