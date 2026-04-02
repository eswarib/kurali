#!/bin/bash
set -eu

# ── Install build dependencies ──────────────────────────────────────────────
install_deps() {
    echo "Checking build dependencies..."

    # Build-time packages required to compile and link the backend
    local DEPS=(
        build-essential      # g++, make, etc.
        cmake                # needed to build whisper.cpp
        pkg-config           # used by Makefile to find libs
        nodejs               # needed for version extraction & Electron
        npm                  # Electron frontend build
        wget                 # model downloads
        libx11-dev           # X11 core
        libxtst-dev          # X11 XTest (key simulation)
        libxinerama-dev      # X11 multi-monitor support
        libxkbcommon-dev     # keyboard keymap handling
        portaudio19-dev      # audio recording
        libsndfile1-dev      # audio file I/O
        libportal-dev        # Wayland portal support
        libglib2.0-dev       # GLib/GIO/GObject (used by libportal)
        libfuse2             # needed to run AppImage tools
    )

    # Optional but useful
    local OPTIONAL_DEPS=(
        libatspi2.0-dev      # AT-SPI accessibility (optional)
        libxdo-dev           # xdotool library (optional, fallback)
    )

    local missing=()
    for pkg in "${DEPS[@]}"; do
        if ! dpkg -s "$pkg" &>/dev/null; then
            missing+=("$pkg")
        fi
    done

    local opt_missing=()
    for pkg in "${OPTIONAL_DEPS[@]}"; do
        if ! dpkg -s "$pkg" &>/dev/null; then
            opt_missing+=("$pkg")
        fi
    done

    if [ ${#missing[@]} -gt 0 ]; then
        echo "Installing missing required packages: ${missing[*]}"
        sudo apt-get update
        sudo apt-get install -y "${missing[@]}"
    else
        echo "All required packages are installed."
    fi

    if [ ${#opt_missing[@]} -gt 0 ]; then
        echo "Installing optional packages (if available): ${opt_missing[*]}"
        sudo apt-get install -y "${opt_missing[@]}" 2>/dev/null || true
    fi

    # appimagetool — download if not on PATH
    if ! command -v appimagetool &>/dev/null; then
        echo "appimagetool not found, downloading..."
        wget -q -O /tmp/appimagetool "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage"
        chmod +x /tmp/appimagetool
        sudo mv /tmp/appimagetool /usr/local/bin/appimagetool
        echo "appimagetool installed to /usr/local/bin/"
    fi

    echo "All dependencies satisfied."
}

install_deps

# ── Paths ────────────────────────────────────────────────────────────────────
ELECTRON_DIR="../coral-electron"
BACKEND_DIR="../coral"
APPDIR="../coral.appdir"
SCRIPTS_DIR="../coral/scripts"
WHISPER_DIR="../whisper.cpp"

# Check if required directories exist
if [ ! -d "$ELECTRON_DIR" ]; then
    echo "Error: Electron directory not found: $ELECTRON_DIR"
    exit 1
fi

if [ ! -d "$BACKEND_DIR" ]; then
    echo "Error: Backend directory not found: $BACKEND_DIR"
    exit 1
fi

if [ ! -d "$WHISPER_DIR" ]; then
    echo "Error: whisper.cpp directory not found: $WHISPER_DIR"
    echo "Clone it with: git clone https://github.com/ggerganov/whisper.cpp.git $WHISPER_DIR"
    exit 1
fi

# Pull version info from package.json
APP_VERSION=$(node -p "require('$ELECTRON_DIR/package.json').version")
BUILD_DATE=$(date +%Y-%m-%d)
GIT_COMMIT=$(git rev-parse --short HEAD)

echo "Embedding version $APP_VERSION ($GIT_COMMIT, $BUILD_DATE) into backend"

# Check if version.h.in exists
if [ ! -f "$BACKEND_DIR/src/version.h.in" ]; then
    echo "Warning: version.h.in not found, skipping version embedding"
else
    # Generate version.h from template
    sed -e "s/@APP_VERSION@/$APP_VERSION/" \
        -e "s/@BUILD_DATE@/$BUILD_DATE/" \
        -e "s/@GIT_COMMIT@/$GIT_COMMIT/" \
        "$BACKEND_DIR/src/version.h.in" > "$BACKEND_DIR/src/version.h"
fi

# ── Detect platform ──────────────────────────────────────────────────────────
IS_WINDOWS=false
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*|Windows_NT) IS_WINDOWS=true ;;
esac

# ── Build whisper.cpp ────────────────────────────────────────────────────────
echo "Building whisper.cpp..."
pushd "$WHISPER_DIR"
if [ "$IS_WINDOWS" = true ]; then
    # Windows: static libs, dynamic runtime, OpenMP enabled
    cmake -B build \
        -DBUILD_SHARED_LIBS=OFF \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_FLAGS="/openmp" \
        -DCMAKE_CXX_FLAGS="/openmp"
    cmake --build build --config Release
else
    # Linux: shared libs
    cmake -B build -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release -j$(nproc)
fi
popd

# Copy whisper libraries into coral/lib
mkdir -p "$BACKEND_DIR/lib"
if [ "$IS_WINDOWS" = true ]; then
    cp "$WHISPER_DIR"/build/src/Release/whisper.lib          "$BACKEND_DIR/lib/" 2>/dev/null || \
    cp "$WHISPER_DIR"/build/src/whisper.lib                  "$BACKEND_DIR/lib/" 2>/dev/null || true
    cp "$WHISPER_DIR"/build/ggml/src/Release/ggml*.lib       "$BACKEND_DIR/lib/" 2>/dev/null || \
    cp "$WHISPER_DIR"/build/ggml/src/ggml*.lib               "$BACKEND_DIR/lib/" 2>/dev/null || true
    echo "whisper.cpp build complete — static libraries copied to $BACKEND_DIR/lib/"
else
    cp "$WHISPER_DIR"/build/src/libwhisper.so*       "$BACKEND_DIR/lib/"
    cp "$WHISPER_DIR"/build/ggml/src/libggml*.so*    "$BACKEND_DIR/lib/"
    echo "whisper.cpp build complete — shared libraries copied to $BACKEND_DIR/lib/"
fi

# ── Build coral backend ──────────────────────────────────────────────────────
if [ "$IS_WINDOWS" = true ]; then
    # Windows: use CMake
    pushd "$BACKEND_DIR"
    cmake -B build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded" \
        -DAPP_VERSION="$APP_VERSION" \
        -DBUILD_DATE="$BUILD_DATE" \
        -DGIT_COMMIT="$GIT_COMMIT"
    cmake --build build --config Release
    popd
else
    # Linux: use Makefile
    pushd "$BACKEND_DIR"
    make clean
    make all
    popd
fi

echo "Backend build complete."

# Optional: Build Electron frontend
echo "Building Electron frontend..."
pushd "$ELECTRON_DIR"
npm install
# npm run build   # Uncomment if you have a build script

# electron is a dependency so prune keeps node_modules/electron for AppRun.
npm prune --production

popd
echo "Frontend build complete."

echo "Copying Electron app into AppDir..."
rm -rf "$APPDIR"
mkdir -p "$APPDIR"

# Copy Electron app files to root of AppDir (this is what the AppRun script expects)
cp -r "$ELECTRON_DIR/main.js" "$APPDIR/"
cp -r "$ELECTRON_DIR/renderer" "$APPDIR/"
cp -r "$ELECTRON_DIR/node_modules" "$APPDIR/"
cp -r "$ELECTRON_DIR/package.json" "$APPDIR/"


echo "Copying backend binary into AppDir..."
# Copy backend binary into AppDir
mkdir -p "$APPDIR/usr/bin"
cp -r "$BACKEND_DIR/bin/coral" "$APPDIR/usr/bin/"
strip "$APPDIR/usr/bin/coral"

echo "Copied backend binary into AppDir..."

#copy libraries needed for whisper
mkdir -p "$APPDIR/usr/lib"
cp -r "$BACKEND_DIR/lib/"* "$APPDIR/usr/lib/"
bash $BACKEND_DIR/scripts/collect-libs.sh $BACKEND_DIR/bin/coral $APPDIR/usr/lib

#copy config
mkdir -p "$APPDIR/usr/share/coral/conf"
cp "$BACKEND_DIR/conf/config-linux.json" "$APPDIR/usr/share/coral/conf/config.json"

MODEL_DIR="$APPDIR/usr/share/coral/models"
mkdir -p "$MODEL_DIR"

# Copy small model from local models folder, or download from Hugging Face if not present
MODELS_SRC="../models"
SMALL_MODEL="ggml-small.en.bin"
SMALL_MODEL_URL="https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.en.bin"
if [ -f "$MODELS_SRC/$SMALL_MODEL" ]; then
    cp "$MODELS_SRC/$SMALL_MODEL" "$MODEL_DIR/"
    echo "Copied $SMALL_MODEL from $MODELS_SRC"
elif [ ! -f "$MODEL_DIR/$SMALL_MODEL" ]; then
    echo "Downloading $SMALL_MODEL from Hugging Face..."
    wget -O "$MODEL_DIR/$SMALL_MODEL" "$SMALL_MODEL_URL"
    echo "Downloaded $SMALL_MODEL"
else
    echo "$SMALL_MODEL already in AppDir, skipping."
fi


# Copy icons, desktop file, and AppRun
cp "$SCRIPTS_DIR/coral.desktop" "$APPDIR/"
mkdir -p "$APPDIR/node_modules/electron/dist/resources"
cp "../logo/coral.png" "$APPDIR/node_modules/electron/dist/resources/coral.png"
cp "../logo/coral.png" "$APPDIR/usr/share/coral/coral.png"
cp "../logo/coral.png" "$APPDIR/coral.png"
# Create directory for desktop icon and copy icon
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps/"
cp "../logo/coral.png" "$APPDIR/usr/share/coral/coral.png"

cp -r "$ELECTRON_DIR/icons" "$APPDIR/"
mkdir -p "$APPDIR/node_modules/electron/dist/resources"
cp -r "$ELECTRON_DIR/icons" "$APPDIR/node_modules/electron/dist/resources/"
cp -r "$ELECTRON_DIR/icons" "$APPDIR/usr/share/coral/"
cp -r "$ELECTRON_DIR/icons" "$APPDIR/usr/share/coral/"

cp "$SCRIPTS_DIR/check-and-install-fuse.sh" "$APPDIR/"
cp "$ELECTRON_DIR/package.json" "$APPDIR/"
cp "$SCRIPTS_DIR/AppRun" "$APPDIR/"
chmod +x "$APPDIR/AppRun"

#if already present removes it
rm -f CoralApp-x86_64.AppImage

echo "Building AppImage..."
appimagetool "$APPDIR" ../CoralApp-${APP_VERSION}-x86_64.AppImage

# Remove the AppDir after building the AppImage
rm -rf "$APPDIR"

echo "AppImage built successfully!"
