#!/usr/bin/env bash
# Wrapper that replaces "python" calls with adiboupk-routed calls.
# Usage: adiboupk-python.sh script.py [args...]
#
# Install by symlinking or aliasing:
#   alias python='./wrappers/adiboupk-python.sh'

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ADIBOUPK="${SCRIPT_DIR}/../build/adiboupk"

if [ ! -x "$ADIBOUPK" ]; then
    # Try system PATH
    ADIBOUPK="$(command -v adiboupk 2>/dev/null || true)"
fi

if [ -z "$ADIBOUPK" ] || [ ! -x "$ADIBOUPK" ]; then
    echo "adiboupk: binary not found. Build with 'cmake --build build' or install to PATH." >&2
    exit 1
fi

exec "$ADIBOUPK" run "$@"
