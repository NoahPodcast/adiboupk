#!/usr/bin/env bash
# Install adiboupk from source on Linux/macOS.
# Usage: curl -sSL https://raw.githubusercontent.com/NoahPodcast/adiboupk/main/install.sh | bash
#    or: git clone ... && cd adiboupk && ./install.sh
set -euo pipefail

INSTALL_DIR="${INSTALL_DIR:-/usr/local/bin}"
BUILD_TYPE="${BUILD_TYPE:-Release}"

info()  { printf '\033[1;34m==> \033[0m%s\n' "$*"; }
warn()  { printf '\033[1;33m==> \033[0m%s\n' "$*"; }
error() { printf '\033[1;31m==> \033[0m%s\n' "$*" >&2; }
die()   { error "$*"; exit 1; }

# --- Detect OS ---
OS="$(uname -s)"
case "$OS" in
    Linux)  PLATFORM="linux" ;;
    Darwin) PLATFORM="macos" ;;
    *)      die "Unsupported OS: $OS" ;;
esac

# --- CPU count (portable) ---
ncpus() {
    if [ "$PLATFORM" = "macos" ]; then
        sysctl -n hw.ncpu 2>/dev/null || echo 2
    else
        nproc 2>/dev/null || grep -c ^processor /proc/cpuinfo 2>/dev/null || echo 2
    fi
}

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

# --- Check for required tools ---
has_cmake=false
has_cxx=false
has_make=false

command -v cmake &>/dev/null && has_cmake=true
# macOS uses clang++, Linux typically g++
command -v c++ &>/dev/null || command -v g++ &>/dev/null || command -v clang++ &>/dev/null && has_cxx=true
command -v make &>/dev/null || command -v ninja &>/dev/null && has_make=true

# --- Install missing dependencies ---
if [ "$has_cmake" = false ] || [ "$has_cxx" = false ] || [ "$has_make" = false ]; then
    if [ "$PLATFORM" = "macos" ]; then
        # On macOS, Xcode CLT provides clang++ and make
        if [ "$has_cxx" = false ] || [ "$has_make" = false ]; then
            if ! xcode-select -p &>/dev/null; then
                info "Installing Xcode Command Line Tools..."
                xcode-select --install 2>/dev/null || true
                echo "    Please complete the Xcode CLT installation popup, then re-run this script."
                exit 1
            fi
        fi
        if [ "$has_cmake" = false ]; then
            if command -v brew &>/dev/null; then
                info "Installing cmake (brew)..."
                HOMEBREW_NO_AUTO_UPDATE=1 brew install cmake 2>/dev/null
            else
                die "cmake not found. Install it with: brew install cmake"
            fi
        fi
    else
        # Linux
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
        elif command -v apk &>/dev/null; then
            info "Installing build tools (apk)..."
            sudo apk add cmake g++ make python3 >/dev/null
        else
            die "Could not detect package manager. Install cmake, g++, and make manually."
        fi
    fi
fi

# --- Detect C++ compiler ---
CXX=""
if command -v c++ &>/dev/null; then
    CXX="c++"
elif command -v clang++ &>/dev/null; then
    CXX="clang++"
elif command -v g++ &>/dev/null; then
    CXX="g++"
fi

if [ -z "$CXX" ]; then
    die "No C++ compiler found. Install g++ or clang++."
fi

# --- Build ---
info "Building adiboupk..."
cd "$REPO_DIR"

CMAKE_ARGS=(-B build -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DBUILD_TESTS=OFF)

# On macOS, don't pass static linking flags (not supported by Apple clang)
if [ "$PLATFORM" = "macos" ]; then
    CMAKE_ARGS+=(-DCMAKE_CXX_COMPILER="$CXX")
fi

BUILD_LOG="$(mktemp)"

if ! cmake "${CMAKE_ARGS[@]}" >"$BUILD_LOG" 2>&1; then
    error "CMake configure failed:"
    cat "$BUILD_LOG"
    rm -f "$BUILD_LOG"
    exit 1
fi

if ! cmake --build build --config "$BUILD_TYPE" -j "$(ncpus)" >>"$BUILD_LOG" 2>&1; then
    error "Build failed:"
    tail -30 "$BUILD_LOG"
    rm -f "$BUILD_LOG"
    exit 1
fi

rm -f "$BUILD_LOG"

# Verify the binary was produced
if [ ! -f "build/adiboupk" ]; then
    die "Build succeeded but binary not found at build/adiboupk"
fi

# --- Install ---
info "Installing to $INSTALL_DIR..."
if [ -w "$INSTALL_DIR" ] 2>/dev/null || [ -w "$(dirname "$INSTALL_DIR")" ] 2>/dev/null; then
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
