#!/usr/bin/env bash
# Build scene-wall.plugin for macOS against the installed OBS.app.
# Usage: ./build-macos.sh [--install]
set -euo pipefail
cd "$(dirname "$0")"

OBS_APP="${OBS_APP:-/Applications/OBS.app}"
OBS_VERSION="${OBS_VERSION:-$(defaults read "$OBS_APP/Contents/Info" CFBundleShortVersionString)}"
DEPS=.deps
ARCH="${ARCH:-$(uname -m)}"

command -v cmake >/dev/null || { echo "Run: brew install cmake ninja simde"; exit 1; }

# 1. OBS headers matching the installed OBS version
if [ ! -d "$DEPS/obs-studio" ]; then
    git clone -q --depth 1 --branch "$OBS_VERSION" https://github.com/obsproject/obs-studio.git "$DEPS/obs-studio"
fi

# 2. OBS's own pre-built Qt6 (version + hash taken from obs-studio's CMakePresets.json)
if [ ! -d "$DEPS/qt6" ]; then
    read -r QT_DATE QT_HASH < <(python3 -I -c '
import json,sys
p=json.load(open(sys.argv[1]))
d=next(c for c in p["configurePresets"] if c["name"]=="dependencies")["vendor"]["obsproject.com/obs-studio"]["dependencies"]["qt6"]
print(d["version"], d["hashes"]["macos-universal"])' "$DEPS/obs-studio/CMakePresets.json")
    f="macos-deps-qt6-$QT_DATE-universal.tar.xz"
    curl -fL -o "$DEPS/$f" "https://github.com/obsproject/obs-deps/releases/download/$QT_DATE/$f"
    echo "$QT_HASH  $DEPS/$f" | shasum -a 256 -c -
    mkdir -p "$DEPS/qt6" && tar -xf "$DEPS/$f" -C "$DEPS/qt6" && rm "$DEPS/$f"
fi

# 3. Configure + build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="$ARCH" -DBUILD_TESTING=OFF \
    -DOBS_APP_DIR="$OBS_APP" -DOBS_SDK_DIR="$PWD/$DEPS/obs-studio" -DOBS_QT_CMAKE_DIR="$PWD/$DEPS/qt6"
cmake --build build

echo "Built: build/scene-wall.plugin"

# 4. Optional install into the user plugin folder
if [ "${1:-}" = "--install" ]; then
    dest="$HOME/Library/Application Support/obs-studio/plugins"
    mkdir -p "$dest"
    rm -rf "$dest/scene-wall.plugin"
    cp -R build/scene-wall.plugin "$dest/"
    echo "Installed to $dest/scene-wall.plugin — restart OBS."
fi
