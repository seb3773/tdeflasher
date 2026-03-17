#!/bin/bash
# create_deb.sh - Automates the packaging of TDE-Flasher into a Debian .deb file

set -e

# --- Configuration ---
APP_NAME="tdeflasher"
VERSION="1.0.0"
ARCH="amd64"
MAINTAINER="Your Name <your.email@example.com>"
DESCRIPTION="A lightweight, blazing-fast OS image flasher natively designed for TDE/TQt3."
# ---------------------

BUILD_DIR="build"
PKG_DIR="${APP_NAME}_${VERSION}_${ARCH}"
DEB_NAME="${PKG_DIR}.deb"

echo "[*] Packaging TDE-Flasher version ${VERSION} for ${ARCH}..."

# 1. Clean and build the project
echo "[*] Cleaning old build..."
rm -rf "$BUILD_DIR"
mkdir "$BUILD_DIR"
cd "$BUILD_DIR"

echo "[*] Running CMake..."
cmake -DCMAKE_BUILD_TYPE=Release ..

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

# Create a temporary desktop entry if it doesn't exist
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
echo "[*] Generating DEBIAN/control file..."
cat << EOF > "$PKG_DIR/DEBIAN/control"
Package: $APP_NAME
Version: $VERSION
Section: utils
Priority: optional
Architecture: $ARCH
Depends: libc6, libgcc-s1, libstdc++6, libarchive13, libcurl4 | libcurl4-nss | libcurl4-gnutls | libcurl3-nss | libcurl3-gnutls, libgcrypt20, tqt3 | libtqt4
Maintainer: $MAINTAINER
Description: $DESCRIPTION
 TDE-Flasher perfectly replicates the foolproof Etcher workflow in a native, 
 compiled TQt3 application that uses minimal memory and CPU cycles while 
 running natively alongside the rest of your TDE applications. Features
 URL streaming verification, block cloning, and safety checks.
EOF
chmod 644 "$PKG_DIR/DEBIAN/control"

# 5. Build the .deb file
echo "[*] Building the .deb package..."
dpkg-deb --build "$PKG_DIR"

# 6. Cleanup the packaging directory
echo "[*] Cleaning up package directory..."
rm -rf "$PKG_DIR"

echo "[+] Done! Package generated: $DEB_NAME"
