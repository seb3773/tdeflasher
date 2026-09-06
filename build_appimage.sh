#!/usr/bin/env bash
set -euo pipefail

SRC_ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SRC_ROOT/build"
APPDIR="$BUILD_DIR/AppDir"

need_cmd() {
	command -v "$1" >/dev/null 2>&1 || {
		echo "error: missing required command: $1" >&2
		exit 1
	}
}

need_cmd cmake
need_cmd make
need_cmd pkg-config
need_cmd strip
need_cmd sed
need_cmd awk
need_cmd wget
need_cmd cp
need_cmd chmod
need_cmd mkdir

# Make sure build dir exists and clean previous builds
echo "info: cleaning and compiling tde-flasher..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

cd "$BUILD_DIR"
cmake -DCMAKE_BUILD_TYPE=Release -DUSE_STATIC_TQT3=OFF ..
make -j$(nproc)
cd "$SRC_ROOT"

BIN_PATH="$BUILD_DIR/gui/tde-flasher"
if test ! -x "$BIN_PATH"; then
	echo "error: missing built binary: $BIN_PATH" >&2
	exit 1
fi

# Clean and create AppDir structure
echo "info: preparing AppDir..."
rm -rf -- "$APPDIR"
mkdir -p -- \
	"$APPDIR/usr/bin" \
	"$APPDIR/usr/lib" \
	"$APPDIR/usr/share/applications" \
	"$APPDIR/usr/share/icons/hicolor/48x48/apps"

# Copy binary
cp -a "$BIN_PATH" "$APPDIR/usr/bin/tdeflasher"

# Strip staged binary
if command -v sstrip >/dev/null 2>&1; then
	echo "info: stripping staged binary with sstrip"
	sstrip "$APPDIR/usr/bin/tdeflasher" >/dev/null 2>&1 || true
else
	echo "info: using strip --strip-all"
	strip --strip-all "$APPDIR/usr/bin/tdeflasher" >/dev/null 2>&1 || true
fi

# Resolve and copy library dependencies recursively
echo "info: resolving and copying library dependencies recursively..."
declare -A visited_libs

# Standard libraries that must be provided by the host system:
# glibc core, basic X11 protocol, hardware OpenGL drivers, and libicu/libxml2 (system provides libxml2 with its own ICU data)
EXCLUDE_REGEX="^(ld-linux.*|libc\..*|libm\..*|libpthread\..*|libdl\..*|librt\..*|libresolv\..*|libutil\..*|libanl\..*|libnss_.*|libGL\..*|libGLX\..*|libEGL\..*|libdrm\..*|libglapi\..*|libgbm\..*|libX11\..*|libX11-xcb\..*|libxcb\..*|libXau\..*|libXdmcp\..*|libstdc\+\+\..*|libgcc_s\..*|libgtk3-nocsd\..*|libxml2\..*|libicu.*)$"

queue=("$APPDIR/usr/bin/tdeflasher")

while [ ${#queue[@]} -gt 0 ]; do
	current="${queue[0]}"
	queue=("${queue[@]:1}")
	
	while read -r libname libpath; do
		[[ -z "$libname" || -z "$libpath" || ! -f "$libpath" ]] && continue
		[[ "$libname" =~ $EXCLUDE_REGEX ]] && continue
		
		if [ -z "${visited_libs[$libname]:-}" ]; then
			visited_libs["$libname"]=1
			echo "  -> bundling: $libname ($libpath)"
			cp -L "$libpath" "$APPDIR/usr/lib/$libname"
			chmod u+w "$APPDIR/usr/lib/$libname"
			strip --strip-unneeded "$APPDIR/usr/lib/$libname" 2>/dev/null || true
			queue+=("$APPDIR/usr/lib/$libname")
		fi
	done < <(LD_LIBRARY_PATH="$APPDIR/usr/lib" ldd "$current" 2>/dev/null | awk '/=>/ {print $1, $3}')
done

# Copy icon
ICON_SRC="$SRC_ROOT/konquiflasher.png"
if test -f "$ICON_SRC"; then
	cp -a "$ICON_SRC" "$APPDIR/tdeflasher.png"
	cp -a "$ICON_SRC" "$APPDIR/usr/share/icons/hicolor/48x48/apps/tdeflasher.png"
else
	echo "error: missing $ICON_SRC" >&2
	exit 1
fi

# Create Desktop entry at root of AppDir and usr/share/applications
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

# Create AppRun entry script
cat > "$APPDIR/AppRun" <<'EOF'
#!/bin/sh
SELF=$(readlink -f "$0")
HERE=${SELF%/*}
export LD_LIBRARY_PATH="$HERE/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$HERE/usr/bin/tdeflasher" "$@"
EOF
chmod 0755 "$APPDIR/AppRun"

# Download appimagetool if not present
APPIMAGETOOL="$BUILD_DIR/appimagetool"
if [ ! -s "$APPIMAGETOOL" ]; then
	echo "info: downloading appimagetool..."
	wget -q --show-progress -O "$APPIMAGETOOL" "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage"
	chmod +x "$APPIMAGETOOL"
fi

# Build AppImage using --appimage-extract-and-run to bypass FUSE requirements
OUT_APPIMAGE="$SRC_ROOT/tdeflasher-x86_64.AppImage"
rm -f -- "$OUT_APPIMAGE"

echo "info: generating AppImage..."
# Set ARCH environment variable so appimagetool knows what architecture we are packaging
export ARCH=x86_64
"$APPIMAGETOOL" --appimage-extract-and-run "$APPDIR" "$OUT_APPIMAGE"

echo "AppImage successfully built: $OUT_APPIMAGE"
exit 0
