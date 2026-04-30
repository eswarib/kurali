#!/bin/bash
set -e

MODEL_DIR="/usr/share/coral/models"
mkdir -p "$MODEL_DIR"

# Download models if not already present
download_model() {
    local url="$1"
    local dest="$2"
    if [ ! -f "$dest" ]; then
        echo "Downloading $dest..."
        wget -O "$dest" "$url"
    else
        echo "$dest already exists, skipping."
    fi
}

#download_model "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.en.bin" "$MODEL_DIR/ggml-tiny.en.bin"
download_model "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin" "$MODEL_DIR/ggml-base.en.bin"
#download_model "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.en.bin" "$MODEL_DIR/ggml-small.en.bin"
download_model "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-medium.en.bin" "$MODEL_DIR/ggml-medium.en.bin"
#download_model "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3.bin" "$MODEL_DIR/ggml-large-v3.bin"

# Copy config.json to user's home directory if not present
for userhome in /home/*; do
    if [ -d "$userhome" ]; then
        target="$userhome/.kurali/conf"
        mkdir -p "$target"
        if [ ! -f "$target/config.json" ]; then
            cp /usr/share/coral/conf/config.json "$target/config.json"
            chown $(basename "$userhome"):$(basename "$userhome") "$target/config.json"
            echo "Copied config.json to $target/config.json"
        fi
    fi
done

set -e

LIBDIR="/opt/coral/lib"

# Create symlinks
ln -sf libwhisper.so.1.7.5 "$LIBDIR/libwhisper.so.1"
ln -sf libwhisper.so.1.7.5 "$LIBDIR/libwhisper.so"

# Add to ld.so.conf if needed
echo "$LIBDIR" > /etc/ld.so.conf.d/coral.conf
ldconfig

# Also handle root user
#if [ -n "$SUDO_USER" ] && [ "$SUDO_USER" != "root" ]; then
#    userhome=$(eval echo "~$SUDO_USER")
#    target="$userhome/.kurali/conf"
#    mkdir -p "$target"
#    if [ ! -f "$target/config.json" ]; then
#        cp /usr/share/coral/conf/config.json "$target/config.json"
#        chown "$SUDO_USER":"$SUDO_USER" "$target/config.json"
#          echo "Copied config.json to $target/config.json"
#    fi
#fi
