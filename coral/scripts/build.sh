#!/bin/bash
set -eu

# Paths
ELECTRON_DIR="../coral-electron"
BACKEND_DIR="../coral"
APPDIR="../coral.appdir:"

# Pull version info from package.json
APP_VERSION=$(node -p "require('$ELECTRON_DIR/package.json').version")
BUILD_DATE=$(date +%Y-%m-%d)
GIT_COMMIT=$(git rev-parse --short HEAD)

echo "Embedding version $APP_VERSION ($GIT_COMMIT, $BUILD_DATE) into backend"

# Generate version.h from template
sed -e "s/@APP_VERSION@/$APP_VERSION/" \
    -e "s/@BUILD_DATE@/$BUILD_DATE/" \
    -e "s/@GIT_COMMIT@/$GIT_COMMIT/" \
    "$BACKEND_DIR/src/version.h.in" > "$BACKEND_DIR/src/version.h"

# Build backend
pushd "$BACKEND_DIR"
make clean
make
popd

echo "Backend build complete."

# Optional: Build Electron frontend
echo "Building Electron frontend..."
pushd "$ELECTRON_DIR"
npm install
# npm run build   # Uncomment if you have a build script

npm prune --production

popd
echo "Frontend build complete."

echo "Copying Electron app into AppDir..."
rm -rf "$APPDIR"
mkdir -p "$APPDIR"
cp -r "$ELECTRON_DIR/main.js" "$APPDIR/"
cp -r "$ELECTRON_DIR/renderer" "$APPDIR/"
cp -r "$ELECTRON_DIR/node_modules" "$APPDIR/"

# Copy backend binary into AppDir
mkdir -p "$APPDIR/usr/bin"
cp "$BACKEND_DIR/bin/kurali" "$APPDIR/usr/bin/"
strip "$APPDIR/usr/bin/kurali"

#copy libraries needed for whisper
mkdir -p "$APPDIR/usr/lib"
sh ../coral/scripts/collect-libs.sh

#copy config
mkdir -p "$APPDIR/usr/share/coral/conf"
cp ../coral/conf/config-linux.json "$APPDIR/usr/share/coral/conf/config.json"

#copy models
mkdir -p "$APPDIR/usr/share/coral/models"
cp ../coral/models/ggml-base.en.bin "$APPDIR/usr/share/coral/models/ggml-base.en.bin"

# Copy icons, desktop file, and AppRun
cp "./coral.desktop" "$APPDIR/"
cp "../../logo/coral.png" "$APPDIR/"
cp ./check-and-install-dependencies.sh "$APPDIR/"
cp ./run-appimage.sh "$APPDIR/"
cp ./package.json "$APPDIR/"
cp "./AppRun" "$APPDIR/"
chmod +x "$APPDIR/AppRun"

#if already present removes it
rm -f CoralApp-x86_64.AppImage

echo "Building AppImage..."
appimagetool "$APPDIR" CoralApp-x86_64.AppImage

echo "AppImage built successfully!"
