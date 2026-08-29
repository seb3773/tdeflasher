#!/bin/bash
# create_deb.sh - Automates the packaging of TDE-Flasher into a Debian .deb file

set -e

# --- Configuration ---
APP_NAME="tdeflasher"
VERSION="1.0.0"
ARCH="amd64"
MAINTAINER="seb3773"
DESCRIPTION="Lightweight OS image flasher natively designed for Trinity Desktop (TDE) & Linux"

STATIC_TQT3=0
PKG_SUFFIX=""
CMAKE_OPTS="-DCMAKE_BUILD_TYPE=Release"

# Common base runtime dependencies for I/O and crypto
CORE_DEPENDS="libarchive13, libcurl4 | libcurl3-gnutls | libcurl4-gnutls-dev | libcurl3-nss, libgcrypt20"

# Parse arguments
while [[ "$#" -gt 0 ]]; do
    case $1 in
        -static) STATIC_TQT3=1 ;;
        *) echo "Unknown parameter passed: $1"; exit 1 ;;
    esac
    shift
done

if [ "$STATIC_TQT3" -eq 1 ]; then
    echo "[*] Static TQt3 mode enabled."
    PKG_SUFFIX="_static"
    PKG_NAME_SUFFIX="-static"
    CMAKE_OPTS="$CMAKE_OPTS -DUSE_STATIC_TQT3=ON"
    # For static build, include base X11/image format runtime libraries
    DEPENDS="$CORE_DEPENDS, libx11-6, libxext6, libxrender1, libxft2, libfontconfig1, libfreetype6, libpng16-16, libjpeg62-turbo | libjpeg62, libmng1 | libmng2, liblcms2-2"
    CONFLICTS="tdeflasher"
else
    echo "[*] Dynamic TQt3 mode enabled."
    PKG_SUFFIX=""
    PKG_NAME_SUFFIX=""
    CMAKE_OPTS="$CMAKE_OPTS -DUSE_STATIC_TQT3=OFF"
    # For dynamic build, use the Trinity TQt3 GUI library
    DEPENDS="libtqt3-mt-trinity (>= 4:14.0.0) | libtqt3-mt, $CORE_DEPENDS"
    CONFLICTS="tdeflasher-static"
fi
# ---------------------

BUILD_DIR="build"
PKG_DIR="${APP_NAME}_${VERSION}${PKG_SUFFIX}_${ARCH}"
DEB_NAME="${PKG_DIR}.deb"

echo "[*] Packaging TDE-Flasher version ${VERSION} for ${ARCH} (Suffix: ${PKG_SUFFIX:-None})..."

# 1. Clean and build the project
echo "[*] Cleaning old build..."
rm -rf "$BUILD_DIR"
mkdir "$BUILD_DIR"
cd "$BUILD_DIR"

echo "[*] Running CMake..."
cmake $CMAKE_OPTS ..

echo "[*] Compiling..."
make -j$(nproc)

cd ..

# 2. Prepare the Debian package structure
echo "[*] Creating Debian package structure..."
rm -rf "$PKG_DIR"
mkdir -p "$PKG_DIR/usr/bin"
mkdir -p "$PKG_DIR/usr/share/applications"
mkdir -p "$PKG_DIR/usr/share/icons/hicolor/48x48/apps"
mkdir -p "$PKG_DIR/DEBIAN"

# 3. Copy files to the structure
echo "[*] Copying binary and assets..."
if [ ! -f "build/gui/tde-flasher" ]; then
    echo "[!] Error: Compiled binary 'build/gui/tde-flasher' not found!"
    exit 1
fi
cp build/gui/tde-flasher "$PKG_DIR/usr/bin/$APP_NAME"
chmod 755 "$PKG_DIR/usr/bin/$APP_NAME"

# Strip binary
strip --strip-all "$PKG_DIR/usr/bin/$APP_NAME" || true

# Create desktop entry
cat << 'EOF' > "$PKG_DIR/usr/share/applications/tdeflasher.desktop"
[Desktop Entry]
Name=TDE-Flasher
Comment=Flash OS images to USB drives
Exec=tdeflasher
Icon=tdeflasher
Terminal=false
Type=Application
Categories=System;Utility;
Keywords=usb;flash;image;iso;img;
EOF
chmod 644 "$PKG_DIR/usr/share/applications/tdeflasher.desktop"

# Copy the logo for the desktop icon
cp konquiflasher.png "$PKG_DIR/usr/share/icons/hicolor/48x48/apps/tdeflasher.png"
chmod 644 "$PKG_DIR/usr/share/icons/hicolor/48x48/apps/tdeflasher.png"

# 4. Generate the Debian control file
INSTALLED_SIZE_KB="$(du -sk "$PKG_DIR/usr" | awk '{print $1}')"
echo "[*] Generating DEBIAN/control file..."
cat << EOF > "$PKG_DIR/DEBIAN/control"
Package: ${APP_NAME}${PKG_NAME_SUFFIX}
Version: $VERSION
Section: utils
Priority: optional
Architecture: $ARCH
Installed-Size: $INSTALLED_SIZE_KB
Depends: $DEPENDS
Conflicts: $CONFLICTS
Maintainer: $MAINTAINER
Description: $DESCRIPTION
 TDE-Flasher perfectly replicates the foolproof Etcher workflow in a native, 
 compiled TQt3 application that uses minimal memory and CPU cycles while 
 running natively alongside the rest of your TDE applications. Features
 URL streaming verification, block cloning, and safety checks.
EOF
chmod 644 "$PKG_DIR/DEBIAN/control"

# Post-install & Pre-remove scripts for icon cache
cat > "$PKG_DIR/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
	gtk-update-icon-cache -f -t /usr/share/icons/hicolor >/dev/null 2>&1 || true
fi
if command -v update-desktop-database >/dev/null 2>&1; then
	update-desktop-database -q /usr/share/applications >/dev/null 2>&1 || true
fi
exit 0
EOF
chmod 0755 "$PKG_DIR/DEBIAN/postinst"

cat > "$PKG_DIR/DEBIAN/prerm" <<'EOF'
#!/bin/sh
set -e
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
	gtk-update-icon-cache -f -t /usr/share/icons/hicolor >/dev/null 2>&1 || true
fi
if command -v update-desktop-database >/dev/null 2>&1; then
	update-desktop-database -q /usr/share/applications >/dev/null 2>&1 || true
fi
exit 0
EOF
chmod 0755 "$PKG_DIR/DEBIAN/prerm"

# 5. Build the .deb file
echo "[*] Building the .deb package..."
dpkg-deb --build "$PKG_DIR"

# 6. Cleanup the packaging directory
echo "[*] Cleaning up package directory..."
rm -rf "$PKG_DIR"

echo "[+] Done! Package generated: $DEB_NAME"
