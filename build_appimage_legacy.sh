#!/usr/bin/env bash
set -euo pipefail

SRC_ROOT="$(cd "$(dirname "$0")" && pwd)"
IMAGE_NAME="tdeflasher-legacy-builder:latest"
OUT_APPIMAGE="$SRC_ROOT/tdeflasher-legacy-x86_64.AppImage"

# Ensure docker is installed
if ! command -v docker >/dev/null 2>&1; then
    echo "error: docker is required to build the legacy AppImage" >&2
    exit 1
fi

# Build docker builder image if not present
if ! docker image inspect "$IMAGE_NAME" >/dev/null 2>&1; then
    echo "info: building docker image $IMAGE_NAME from Dockerfile.legacy..."
    docker build -t "$IMAGE_NAME" -f "$SRC_ROOT/Dockerfile.legacy" "$SRC_ROOT"
fi

chmod +x "$SRC_ROOT/build_legacy_inner.sh"

echo "info: compiling and packaging legacy AppImage (Ubuntu 20.04 / GLIBC 2.31 base)..."

docker run --rm \
    -v "$SRC_ROOT:/src" \
    -e USER_UID="$(id -u)" \
    -e USER_GID="$(id -g)" \
    "$IMAGE_NAME" /src/build_legacy_inner.sh

echo "Legacy AppImage successfully built: $OUT_APPIMAGE"
ls -lh "$OUT_APPIMAGE"
