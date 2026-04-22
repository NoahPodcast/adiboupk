#!/usr/bin/env bash
# Setup and serve adiboupk MkDocs documentation on Ubuntu Server.
# Usage: sudo ./setup.sh [--port PORT] [--host HOST]
#
# Options:
#   --port PORT   Port to serve on (default: 8000)
#   --host HOST   Host to bind to (default: 0.0.0.0)
#   --build-only  Build static site in site/ without starting a server
#   --systemd     Install as a systemd service (auto-start on boot)

set -euo pipefail

PORT=8000
HOST="0.0.0.0"
BUILD_ONLY=false
INSTALL_SERVICE=false
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

info()  { printf '\033[1;34m==>\033[0m %s\n' "$*"; }
error() { printf '\033[1;31m==>\033[0m %s\n' "$*" >&2; }

while [[ $# -gt 0 ]]; do
    case "$1" in
        --port)       PORT="$2"; shift 2 ;;
        --host)       HOST="$2"; shift 2 ;;
        --build-only) BUILD_ONLY=true; shift ;;
        --systemd)    INSTALL_SERVICE=true; shift ;;
        *)            error "Unknown option: $1"; exit 1 ;;
    esac
done

# --- Install dependencies ---
info "Installing system packages..."
apt-get update -qq
apt-get install -y -qq python3 python3-pip python3-venv > /dev/null

# --- Create venv ---
VENV_DIR="$SCRIPT_DIR/.venv"
if [ ! -d "$VENV_DIR" ]; then
    info "Creating Python virtual environment..."
    python3 -m venv "$VENV_DIR"
fi
source "$VENV_DIR/bin/activate"

# --- Install MkDocs + Material theme ---
info "Installing MkDocs and dependencies..."
pip install --quiet --upgrade pip
pip install --quiet mkdocs-material

# --- Build only ---
if [ "$BUILD_ONLY" = true ]; then
    info "Building static site..."
    cd "$SCRIPT_DIR"
    mkdocs build
    info "Static site generated in $SCRIPT_DIR/site/"
    exit 0
fi

# --- Systemd service ---
if [ "$INSTALL_SERVICE" = true ]; then
    info "Installing systemd service (port $PORT, host $HOST)..."

    cat > /etc/systemd/system/mkdocs-adiboupk.service <<EOF
[Unit]
Description=adiboupk MkDocs documentation server
After=network.target

[Service]
Type=simple
WorkingDirectory=$SCRIPT_DIR
ExecStart=$VENV_DIR/bin/mkdocs serve --dev-addr $HOST:$PORT
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

    systemctl daemon-reload
    systemctl enable mkdocs-adiboupk
    systemctl restart mkdocs-adiboupk

    info "Service installed and started."
    info "Documentation available at http://$HOST:$PORT"
    info "Manage with: systemctl {start|stop|restart|status} mkdocs-adiboupk"
    exit 0
fi

# --- Serve directly ---
info "Serving documentation on http://$HOST:$PORT"
cd "$SCRIPT_DIR"
mkdocs serve --dev-addr "$HOST:$PORT"
