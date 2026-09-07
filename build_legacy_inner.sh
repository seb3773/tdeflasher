#!/usr/bin/env bash
set -euo pipefail

SRC_ROOT="/src"
BUILD_DIR="$SRC_ROOT/build-legacy"
APPDIR="$BUILD_DIR/AppDir"
OUT_FILE="$SRC_ROOT/tdeflasher-legacy-x86_64.AppImage"

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "info: configuring and compiling in Ubuntu 20.04 environment..."
cmake -DCMAKE_BUILD_TYPE=Release -DUSE_STATIC_TQT3=OFF "$SRC_ROOT"
make -j"$(nproc)"

BIN_PATH="$BUILD_DIR/gui/tde-flasher"
if [ ! -x "$BIN_PATH" ]; then
    echo "error: binary $BIN_PATH not found" >&2
    exit 1
fi

echo "info: preparing AppDir..."
rm -rf "$APPDIR"
mkdir -p \
    "$APPDIR/usr/bin" \
    "$APPDIR/usr/lib" \
    "$APPDIR/usr/share/applications" \
    "$APPDIR/usr/share/icons/hicolor/48x48/apps"

cp -a "$BIN_PATH" "$APPDIR/usr/bin/tdeflasher"
strip --strip-all "$APPDIR/usr/bin/tdeflasher" 2>/dev/null || true

echo "info: copying targeted legacy library dependencies..."
libraries=(
    libtqt-mt.so.3
    libtdecore.so.14
    libtdeui.so.14
    libDCOP.so.14
    libtdefx.so.14
    libtqt.so.4
    libart_lgpl_2.so.2
    libarchive.so.13
    libgcrypt.so.20
    libgpg-error.so.0
    libidn.so.11
    libaudio.so.2
    libjpeg.so.8
    libnettle.so.7
)

search_dirs=(/opt/trinity/lib /usr/lib/x86_64-linux-gnu /lib/x86_64-linux-gnu /usr/lib)

for lib in "${libraries[@]}"; do
    libpath=""
    for d in "${search_dirs[@]}"; do
        if [ -f "$d/$lib" ]; then
            libpath="$d/$lib"
            break
        fi
    done
    if [ -n "$libpath" ] && [ -f "$libpath" ]; then
        echo "  -> bundling: $lib ($libpath)"
        cp -L "$libpath" "$APPDIR/usr/lib/$lib"
        chmod u+w "$APPDIR/usr/lib/$lib"
        strip --strip-unneeded "$APPDIR/usr/lib/$lib" 2>/dev/null || true
    else
        echo "  warning: library $lib not found"
    fi
done

# Compatibility symlink for libidn.so.12
if [ -f "$APPDIR/usr/lib/libidn.so.11" ]; then
    ln -sf libidn.so.11 "$APPDIR/usr/lib/libidn.so.12"
fi

# Icons
cp -a "$SRC_ROOT/konquiflasher.png" "$APPDIR/tdeflasher.png"
cp -a "$SRC_ROOT/konquiflasher.png" "$APPDIR/usr/share/icons/hicolor/48x48/apps/tdeflasher.png"

# Desktop entry
cat > "$APPDIR/tdeflasher.desktop" <<EOF
[Desktop Entry]
Version=1.0
Name=TDE-Flasher
Comment=Flash OS images to USB drives
Exec=tdeflasher
Icon=tdeflasher
Terminal=false
Type=Application
Categories=System;Utility;
Keywords=usb;flash;image;iso;img;
EOF
chmod 0644 "$APPDIR/tdeflasher.desktop"
cp -a "$APPDIR/tdeflasher.desktop" "$APPDIR/usr/share/applications/tdeflasher.desktop"

# AppRun script
cat > "$APPDIR/AppRun" <<'EOF'
#!/bin/sh
SELF=$(readlink -f "$0")
HERE=${SELF%/*}
export LD_LIBRARY_PATH="$HERE/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$HERE/usr/bin/tdeflasher" "$@"
EOF
chmod 0755 "$APPDIR/AppRun"

# Generate AppImage
echo "info: generating legacy AppImage with appimagetool..."
export ARCH=x86_64
rm -f "$OUT_FILE"
appimagetool --appimage-extract-and-run "$APPDIR" "$OUT_FILE"
chmod +x "$OUT_FILE"

if [ -n "${USER_UID:-}" ] && [ -n "${USER_GID:-}" ]; then
    chown "${USER_UID}:${USER_GID}" "$OUT_FILE"
fi

echo "info: cleaning up build-legacy directory..."
rm -rf "$BUILD_DIR"

