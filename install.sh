#!/usr/bin/env bash
# Install adiboupk from source on Linux/macOS.
# Usage: curl -sSL https://raw.githubusercontent.com/NoahPodcast/adiboupk/main/install.sh | bash
#    or: git clone ... && cd adiboupk && ./install.sh
set -euo pipefail

INSTALL_DIR="${INSTALL_DIR:-/usr/local/bin}"
BUILD_TYPE="${BUILD_TYPE:-Release}"

info()  { echo -e "\033[1;34m==>\033[0m $*"; }
error() { echo -e "\033[1;31m==>\033[0m $*" >&2; }

# --- Detect if we're inside the repo or need to clone ---
if [ -f "CMakeLists.txt" ] && grep -q "project(adiboupk" CMakeLists.txt 2>/dev/null; then
    REPO_DIR="$(pwd)"
    CLONED=false
else
    REPO_DIR="$(mktemp -d)"
    CLONED=true
    info "Cloning adiboupk..."
    git clone --depth 1 https://github.com/NoahPodcast/adiboupk.git "$REPO_DIR"
fi

# --- Install build dependencies ---
install_deps() {
    if command -v apt-get &>/dev/null; then
        info "Installing build tools (apt)..."
        sudo apt-get update -qq
        sudo apt-get install -y -qq cmake g++ make python3 python3-venv >/dev/null
    elif command -v dnf &>/dev/null; then
        info "Installing build tools (dnf)..."
        sudo dnf install -y cmake gcc-c++ make python3 >/dev/null
    elif command -v yum &>/dev/null; then
        info "Installing build tools (yum)..."
        sudo yum install -y cmake gcc-c++ make python3 >/dev/null
    elif command -v pacman &>/dev/null; then
        info "Installing build tools (pacman)..."
        sudo pacman -S --noconfirm cmake gcc make python >/dev/null
    elif command -v brew &>/dev/null; then
        info "Installing build tools (brew)..."
        brew install cmake python3 >/dev/null
    else
        error "Could not detect package manager. Install cmake, g++, make, and python3 manually."
        exit 1
    fi
}

# Check if cmake and a C++ compiler are available
NEED_DEPS=false
command -v cmake &>/dev/null || NEED_DEPS=true
command -v g++ &>/dev/null || command -v c++ &>/dev/null || NEED_DEPS=true

if [ "$NEED_DEPS" = true ]; then
    install_deps
fi

# --- Build ---
info "Building adiboupk..."
cd "$REPO_DIR"
cmake -B build -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DBUILD_TESTS=OFF >/dev/null 2>&1
cmake --build build --config "$BUILD_TYPE" -j "$(nproc 2>/dev/null || echo 2)" >/dev/null 2>&1

# --- Install ---
info "Installing to $INSTALL_DIR..."
if [ -w "$INSTALL_DIR" ] || [ -w "$(dirname "$INSTALL_DIR")" ]; then
    mkdir -p "$INSTALL_DIR"
    cp build/adiboupk "$INSTALL_DIR/adiboupk"
    chmod +x "$INSTALL_DIR/adiboupk"
else
    sudo mkdir -p "$INSTALL_DIR"
    sudo cp build/adiboupk "$INSTALL_DIR/adiboupk"
    sudo chmod +x "$INSTALL_DIR/adiboupk"
fi

# --- Cleanup ---
if [ "$CLONED" = true ]; then
    rm -rf "$REPO_DIR"
fi

info "Done! adiboupk installed to $INSTALL_DIR/adiboupk"
echo ""
echo "  Usage:"
echo "    cd your-project/"
echo "    adiboupk setup"
echo "    adiboupk run ./ModuleA/script.py"
echo ""

# Verify
if command -v adiboupk &>/dev/null; then
    adiboupk version
else
    echo "Note: $INSTALL_DIR may not be in your PATH. Add it with:"
    echo "  export PATH=\"$INSTALL_DIR:\$PATH\""
fi
