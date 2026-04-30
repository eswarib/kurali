#!/bin/bash
# Build a self-contained .deb with the same layout as the AppImage staging
#   (Electron + kurali backend + whisper/coral libs from collect-libs + config + bundled model).
# Installs under /opt/coral — no FUSE, no AppImage.
#
# Usage: ./build-deb.sh [amd64|arm64]
#   default: amd64
#   Output: $REPO_ROOT/Coral-<version>_<arch>.deb
#
# Environment:
#   KURALI_DEB_SKIP_APT=1   — skip apt install (e.g. CI already installed deps)
#   KURALI_DEB_FRESH_NPM=1 — rm coral-electron/node_modules before npm install (needed when switching arch locally)
#   Legacy: CORAL_DEB_SKIP_APT / CORAL_DEB_FRESH_NPM are still accepted.
set -eu

DEB_ARCH="${1:-amd64}"
case "$DEB_ARCH" in
    amd64|arm64) ;;
    *)
        echo "Usage: $0 [amd64|arm64]"
        exit 1
        ;;
esac

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$SCRIPT_DIR"

echo ">>> Building .deb for architecture: $DEB_ARCH"

MACHINE="$(uname -m)"
if [ "$DEB_ARCH" = "amd64" ] && [ "$MACHINE" != "x86_64" ]; then
    echo "Warning: packaging amd64 on $MACHINE — use an x86_64 host or fix cross-compile."
fi
if [ "$DEB_ARCH" = "arm64" ] && [ "$MACHINE" != "aarch64" ]; then
    echo "Warning: packaging arm64 on $MACHINE — use an aarch64 host (e.g. ubuntu-24.04-arm) for a correct binary."
fi

# ── Install build dependencies (no FUSE / no appimagetool) ──────────────────
install_build_deps() {
    echo "Checking build dependencies..."

    local DEPS=(
        build-essential
        cmake
        pkg-config
        nodejs
        npm
        wget
        libx11-dev
        libxtst-dev
        libxinerama-dev
        libxkbcommon-dev
        portaudio19-dev
        libsndfile1-dev
        libportal-dev
        libglib2.0-dev
    )

    local OPTIONAL_DEPS=(
        libatspi2.0-dev
        fakeroot
    )

    local missing=()
    for pkg in "${DEPS[@]}"; do
        if ! dpkg -s "$pkg" &>/dev/null; then
            missing+=("$pkg")
        fi
    done

    if [ ${#missing[@]} -gt 0 ]; then
        echo "Installing missing required packages: ${missing[*]}"
        sudo apt-get update
        sudo apt-get install -y "${missing[@]}"
    else
        echo "All required build packages are installed."
    fi

    echo "Installing optional packages (fakeroot recommended for .deb): ${OPTIONAL_DEPS[*]}"
    sudo apt-get install -y "${OPTIONAL_DEPS[@]}" 2>/dev/null || true

    echo "Build dependencies satisfied."
}

if [ "${KURALI_DEB_SKIP_APT:-${CORAL_DEB_SKIP_APT:-}}" != "1" ]; then
    install_build_deps
else
    echo "Skipping apt (KURALI_DEB_SKIP_APT=1 or CORAL_DEB_SKIP_APT=1)"
fi

# ── Paths (same as build/build.sh) ───────────────────────────────────────────
ELECTRON_DIR="$REPO_ROOT/coral-electron"
BACKEND_DIR="$REPO_ROOT/coral"
STAGE="$SCRIPT_DIR/coral.deb.stage.$DEB_ARCH"
SCRIPTS_DIR="$BACKEND_DIR/scripts"
WHISPER_DIR="$REPO_ROOT/whisper.cpp"
LOGO="$REPO_ROOT/logo/coral.png"

if [ ! -d "$ELECTRON_DIR" ] || [ ! -d "$BACKEND_DIR" ]; then
    echo "Error: expected $ELECTRON_DIR and $BACKEND_DIR"
    exit 1
fi

if [ ! -f "$WHISPER_DIR/CMakeLists.txt" ]; then
    echo "Error: whisper.cpp not found at $WHISPER_DIR (clone: git clone https://github.com/ggerganov/whisper.cpp.git whisper.cpp)"
    exit 1
fi

APP_VERSION="$(node -p "require('$ELECTRON_DIR/package.json').version")"
BUILD_DATE="$(date +%Y-%m-%d)"
GIT_COMMIT="$(cd "$REPO_ROOT" && git rev-parse --short HEAD 2>/dev/null || echo unknown)"

echo "Embedding version $APP_VERSION ($GIT_COMMIT, $BUILD_DATE) into backend"

if [ -f "$BACKEND_DIR/src/version.h.in" ]; then
    sed -e "s/@APP_VERSION@/$APP_VERSION/" \
        -e "s/@BUILD_DATE@/$BUILD_DATE/" \
        -e "s/@GIT_COMMIT@/$GIT_COMMIT/" \
        "$BACKEND_DIR/src/version.h.in" > "$BACKEND_DIR/src/version.h"
fi

# ── Build whisper.cpp (shared libs) ──────────────────────────────────────────
echo "Building whisper.cpp for $DEB_ARCH..."
rm -rf "$WHISPER_DIR/build"
pushd "$WHISPER_DIR"
cmake -B build -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j"$(nproc)"
popd

mkdir -p "$BACKEND_DIR/lib"
cp "$WHISPER_DIR"/build/src/libwhisper.so* "$BACKEND_DIR/lib/"
cp "$WHISPER_DIR"/build/ggml/src/libggml*.so* "$BACKEND_DIR/lib/"
echo "whisper libraries copied to $BACKEND_DIR/lib/"

# ── Build kurali backend ──────────────────────────────────────────────────────
pushd "$BACKEND_DIR"
make clean
make all
popd
echo "Backend build complete."

# ── Electron frontend (CI / KURALI_DEB_FRESH_NPM forces clean npm for correct Electron arch) ──
echo "Building Electron frontend..."
if [ -n "${GITHUB_ACTIONS:-}" ] || [ "${KURALI_DEB_FRESH_NPM:-${CORAL_DEB_FRESH_NPM:-}}" = "1" ]; then
    rm -rf "$ELECTRON_DIR/node_modules"
fi
pushd "$ELECTRON_DIR"
npm install
npm prune --production
popd

# ── Stage payload (identical layout to AppDir, under STAGE) ───────────────────
echo "Staging package tree..."
rm -rf "$STAGE"
mkdir -p "$STAGE"

cp -r "$ELECTRON_DIR/main.js" "$STAGE/"
cp -r "$ELECTRON_DIR/renderer" "$STAGE/"
cp -r "$ELECTRON_DIR/node_modules" "$STAGE/"
cp -r "$ELECTRON_DIR/package.json" "$STAGE/"

mkdir -p "$STAGE/usr/bin"
cp "$BACKEND_DIR/bin/kurali" "$STAGE/usr/bin/kurali"
strip "$STAGE/usr/bin/kurali"

mkdir -p "$STAGE/usr/lib"
cp -r "$BACKEND_DIR/lib/"* "$STAGE/usr/lib/"
bash "$BACKEND_DIR/scripts/collect-libs.sh" "$BACKEND_DIR/bin/kurali" "$STAGE/usr/lib"

mkdir -p "$STAGE/usr/share/coral/conf"
cp "$BACKEND_DIR/conf/config-linux.json" "$STAGE/usr/share/coral/conf/config.json"

MODEL_DIR="$STAGE/usr/share/coral/models"
mkdir -p "$MODEL_DIR"
MODELS_SRC="$REPO_ROOT/models"
SMALL_MODEL="ggml-small.en.bin"
SMALL_MODEL_URL="https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.en.bin"
if [ -f "$MODELS_SRC/$SMALL_MODEL" ]; then
    cp "$MODELS_SRC/$SMALL_MODEL" "$MODEL_DIR/"
    echo "Copied $SMALL_MODEL from $MODELS_SRC"
elif [ ! -f "$MODEL_DIR/$SMALL_MODEL" ]; then
    echo "Downloading $SMALL_MODEL from Hugging Face..."
    wget -O "$MODEL_DIR/$SMALL_MODEL" "$SMALL_MODEL_URL"
else
    echo "$SMALL_MODEL already staged."
fi

mkdir -p "$STAGE/usr/share/coral"
cp "$LOGO" "$STAGE/usr/share/coral/coral.png"
cp "$LOGO" "$STAGE/coral.png"
mkdir -p "$STAGE/node_modules/electron/dist/resources"
cp "$LOGO" "$STAGE/node_modules/electron/dist/resources/coral.png"
cp -r "$ELECTRON_DIR/icons" "$STAGE/"
cp -r "$ELECTRON_DIR/icons" "$STAGE/node_modules/electron/dist/resources/"
cp -r "$ELECTRON_DIR/icons" "$STAGE/usr/share/coral/"

# ── Assemble .deb filesystem ────────────────────────────────────────────────
PKG_NAME="coral_${APP_VERSION}_${DEB_ARCH}"
DEB_ROOT="$SCRIPT_DIR/$PKG_NAME"
rm -rf "$DEB_ROOT"
mkdir -p "$DEB_ROOT/opt/coral"
cp -a "$STAGE/." "$DEB_ROOT/opt/coral/"

mkdir -p "$DEB_ROOT/usr/bin"
cat > "$DEB_ROOT/usr/bin/kurali" << 'LAUNCHER'
#!/bin/bash
# Launcher for /opt/coral — same env as AppRun (bundled usr/lib + Electron)
HERE=/opt/coral
cd "$HERE" || exit 1
export LD_LIBRARY_PATH="$HERE/usr/lib:${LD_LIBRARY_PATH:-}"
export ELECTRON_DISABLE_SANDBOX=1
export ELECTRON_ENABLE_LOGGING=0
if [ -x "$HERE/node_modules/electron/dist/electron" ]; then
    exec "$HERE/node_modules/electron/dist/electron" . "$@"
elif [ -x "$HERE/node_modules/.bin/electron" ]; then
    exec "$HERE/node_modules/.bin/electron" . "$@"
fi
echo "Electron runtime not found under $HERE"
exit 1
LAUNCHER
chmod 755 "$DEB_ROOT/usr/bin/kurali"

mkdir -p "$DEB_ROOT/usr/share/applications"
cat > "$DEB_ROOT/usr/share/applications/kurali.desktop" << DESKTOP
[Desktop Entry]
Name=Kurali
Comment=Voice transcription
Exec=/usr/bin/kurali
Icon=kurali
Terminal=false
Type=Application
Categories=Utility;Audio;AudioVideo;
DESKTOP

mkdir -p "$DEB_ROOT/usr/share/icons/hicolor/256x256/apps"
cp "$LOGO" "$DEB_ROOT/usr/share/icons/hicolor/256x256/apps/kurali.png"
cp "$LOGO" "$DEB_ROOT/usr/share/icons/hicolor/256x256/apps/coral.png"

# Runtime deps for Electron GTK stack (not bundled by node_modules); adjust if needed.
# libasound2t64 is Ubuntu 24+; keep alternative for 22.04.
DEPS='libc6 (>= 2.31), libgtk-3-0 | libgtk-4-1, libnotify4, libnss3, libxss1, libxtst6, xdg-utils, libatspi2.0-0, libdrm2, libgbm1, libsecret-1-0, libasound2 | libasound2t64'

mkdir -p "$DEB_ROOT/DEBIAN"
cat > "$DEB_ROOT/DEBIAN/control" << CONTROL
Package: coral
Version: ${APP_VERSION}
Section: sound
Priority: optional
Architecture: ${DEB_ARCH}
Maintainer: Kurali <https://github.com/eswarib/kurali>
Depends: ${DEPS}
Description: Kurali — local speech-to-text (Whisper)
 Same payload as the AppImage: bundled backend, libs, and default model under /opt/coral.
CONTROL

cat > "$DEB_ROOT/DEBIAN/postinst" << 'POSTINST'
#!/bin/sh
set -e
if [ "$1" = "configure" ]; then
  gtk-update-icon-cache -f -t /usr/share/icons/hicolor 2>/dev/null || true
  update-desktop-database /usr/share/applications 2>/dev/null || true
fi
exit 0
POSTINST
chmod 755 "$DEB_ROOT/DEBIAN/postinst"

OUT_DEB="$REPO_ROOT/Coral-${APP_VERSION}_${DEB_ARCH}.deb"
rm -f "$OUT_DEB"

echo "Building .deb → $OUT_DEB"
if command -v fakeroot >/dev/null 2>&1; then
    fakeroot dpkg-deb --build --root-owner-group "$DEB_ROOT" "$OUT_DEB"
else
    echo "Warning: install 'fakeroot' for correct package ownership; building anyway."
    dpkg-deb --build --root-owner-group "$DEB_ROOT" "$OUT_DEB"
fi

rm -rf "$DEB_ROOT"
echo "Done ($DEB_ARCH). Install with: sudo apt install $OUT_DEB"
echo "(Or: sudo dpkg -i $OUT_DEB && sudo apt -f install)"
