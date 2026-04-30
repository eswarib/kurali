#!/bin/bash
# Build kurali (C++ backend) for Windows — run in Git Bash with Visual Studio 2022 (C++ workload) installed.
#
# Prerequisites: Git Bash, Visual Studio 2022, Node.js
# Optional env: VCPKG_ROOT, WHISPER_DIR (default: repo/vcpkg, repo/whisper.cpp)
#
# Usage: cd build && bash build-windows.sh
set -e

BUILD_SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$BUILD_SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT"

# Paths (override with env: VCPKG_ROOT, WHISPER_DIR)
ELECTRON_DIR="$REPO_ROOT/coral-electron"
BACKEND_DIR="$REPO_ROOT/coral"
WHISPER_DIR="${WHISPER_DIR:-$REPO_ROOT/whisper.cpp}"
VCPKG_ROOT="${VCPKG_ROOT:-$REPO_ROOT/vcpkg}"

echo "=== Kurali Windows Build ==="
echo "Repo root:  $REPO_ROOT"
echo "vcpkg:      $VCPKG_ROOT"
echo "whisper.cpp: $WHISPER_DIR"
echo ""

# ── Check prerequisites ───────────────────────────────────────────────────────
if ! command -v cmake &>/dev/null; then
    echo "Error: cmake not found. Install Visual Studio with C++ workload."
    exit 1
fi

if ! command -v node &>/dev/null; then
    echo "Error: node not found. Install Node.js."
    exit 1
fi

# ── Set up vcpkg ─────────────────────────────────────────────────────────────
if [ ! -f "$VCPKG_ROOT/vcpkg.exe" ]; then
    echo "Setting up vcpkg..."
    if [ ! -d "$VCPKG_ROOT" ]; then
        git clone https://github.com/microsoft/vcpkg "$VCPKG_ROOT"
    fi
    (cd "$VCPKG_ROOT" && ./bootstrap-vcpkg.bat)
fi

echo "Installing portaudio and libsndfile..."
"$VCPKG_ROOT/vcpkg.exe" install portaudio:x64-windows libsndfile:x64-windows

# ── Set up whisper.cpp ───────────────────────────────────────────────────────
if [ ! -d "$WHISPER_DIR" ]; then
    echo "Cloning whisper.cpp..."
    git clone --depth 1 https://github.com/ggerganov/whisper.cpp "$WHISPER_DIR"
fi

# ── Version info ─────────────────────────────────────────────────────────────
APP_VERSION=$(node -p "require('$ELECTRON_DIR/package.json').version")
BUILD_DATE=$(date +%Y-%m-%d)
GIT_COMMIT=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")

echo "Version: $APP_VERSION ($GIT_COMMIT, $BUILD_DATE)"

if [ -f "$BACKEND_DIR/src/version.h.in" ]; then
    sed -e "s/@APP_VERSION@/$APP_VERSION/" \
        -e "s/@BUILD_DATE@/$BUILD_DATE/" \
        -e "s/@GIT_COMMIT@/$GIT_COMMIT/" \
        "$BACKEND_DIR/src/version.h.in" > "$BACKEND_DIR/src/version.h"
fi

# ── Build whisper.cpp ────────────────────────────────────────────────────────
echo "Building whisper.cpp..."
cd "$WHISPER_DIR"
cmake -B build \
    -G "Visual Studio 17 2022" -A x64 \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DGGML_OPENMP=OFF
cmake --build build --config Release --target ggml whisper
cd "$REPO_ROOT"

# Detect whisper and ggml lib paths (whisper.cpp layout varies by version)
echo "Listing .lib files in whisper build:"
find "$WHISPER_DIR/build" -name "*.lib" 2>/dev/null || true
WHISPERLIB=$(find "$WHISPER_DIR/build" \( -name "whisper*.lib" -o -name "whispercpp.lib" \) 2>/dev/null | head -1)
GGMLLIBS=$(find "$WHISPER_DIR/build" -name "ggml*.lib" 2>/dev/null | tr '\n' ';' | sed 's/;$//')

if [ -z "$WHISPERLIB" ]; then
    echo "Error: whisper.lib not found under $WHISPER_DIR/build"
    echo "Ensure whisper.cpp is at $WHISPER_DIR or set WHISPER_DIR before running."
    exit 1
fi
if [ -z "$GGMLLIBS" ]; then
    echo "Error: ggml*.lib not found"
    exit 1
fi

echo "Whisper lib: $WHISPERLIB"
echo "GGML libs:   $GGMLLIBS"

# ── Build kurali (CMake) ──────────────────────────────────────────────────────
echo "Building kurali..."
rm -rf "$REPO_ROOT/build-win"
cmake -S coral -B build-win \
    -G "Visual Studio 17 2022" -A x64 \
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
    -DVCPKG_TARGET_TRIPLET=x64-windows \
    -DCMAKE_BUILD_TYPE=Release \
    -DWHISPER_LIBRARY="$WHISPERLIB" \
    -DGGML_LIBRARIES="$GGMLLIBS" \
    -DAPP_VERSION="$APP_VERSION" \
    -DBUILD_DATE="$BUILD_DATE" \
    -DGIT_COMMIT="$GIT_COMMIT"

cmake --build build-win --config Release

# ── Bundle EXE and DLLs ─────────────────────────────────────────────────────
OUT_DIR="$REPO_ROOT/coral-windows-x64-v$APP_VERSION"
EXE_SRC=$(find "$REPO_ROOT/build-win" -name "kurali.exe" 2>/dev/null | head -1)

if [ -z "$EXE_SRC" ]; then
    echo "Error: kurali.exe not found"
    exit 1
fi

echo "Bundling to $OUT_DIR..."
rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"
cp "$EXE_SRC" "$OUT_DIR/kurali-$APP_VERSION.exe"

# VC++ runtime DLLs — from vswhere path (matches build toolset)
VSWHERE="/c/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe"
if [ ! -f "$VSWHERE" ]; then
    VSWHERE="C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe"
fi
if [ -f "$VSWHERE" ]; then
    INSTALL_DIR=$(powershell.exe -NoProfile -Command \
        "(& \"$VSWHERE\" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null).Trim()")
    INSTALL_DIR=$(echo "$INSTALL_DIR" | tr -d '\r' | sed 's/\\/\//g')
    if [ -n "$INSTALL_DIR" ] && [ -d "$INSTALL_DIR" ]; then
        VER_FILE="$INSTALL_DIR/VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt"
        if [ -f "$VER_FILE" ]; then
            VC_VER=$(cat "$VER_FILE" | tr -d '[:space:]')
            CRT_DIR="$INSTALL_DIR/VC/Tools/MSVC/$VC_VER/redist/x64"
            for crate in "$CRT_DIR"/Microsoft.VC*.CRT; do
                if [ -d "$crate" ]; then
                    cp "$crate/msvcp140.dll" "$crate/vcruntime140.dll" "$crate/vcruntime140_1.dll" "$OUT_DIR/" 2>/dev/null && echo "Bundled VC++ CRT from $crate"
                    break
                fi
            done
        fi
    fi
fi

if [ ! -f "$OUT_DIR/msvcp140.dll" ]; then
    echo "Warning: VC++ DLLs not found via vswhere. Copy msvcp140.dll, vcruntime140.dll, vcruntime140_1.dll manually, or install VC++ Redist."
fi

# vcpkg DLLs
VCPKG_BIN="$VCPKG_ROOT/installed/x64-windows/bin"
if [ -d "$VCPKG_BIN" ]; then
    cp "$VCPKG_BIN"/*.dll "$OUT_DIR/" 2>/dev/null && echo "Bundled vcpkg DLLs"
fi

echo ""
echo "=== Build complete ==="
echo "Output: $OUT_DIR"
ls -la "$OUT_DIR"
echo ""
echo "Run: $OUT_DIR/kurali-$APP_VERSION.exe"
