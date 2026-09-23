#!/usr/bin/env bash
# Build the release AppImage inside an Ubuntu 22.04 container so it runs on
# every distro with glibc >= 2.35 (Ubuntu 22.04/24.04, Debian 12, Fedora 36+,
# Arch/CachyOS, …). Issue #15: an AppImage built on a rolling distro needed
# GLIBC_2.43 and refused to start on Ubuntu 24.04 LTS.
#
# Usage (from anywhere; needs docker and the SDK under sdk/ or SDK_ROOT=…):
#   qt/packaging/build-appimage-docker.sh          → dist/OBSBOT4Linux-x86_64.AppImage
#
# The container image (Qt 6.9.3 via aqtinstall + build deps) is built once and
# cached by docker; the repo is bind-mounted, so the artifact and the
# intermediate dirs (qt/build-appimage, qt/AppDir, qt/packaging/tools, dist/)
# land in the checkout exactly as with build-appimage.sh, owned by you.
set -euo pipefail

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)   # qt/packaging
REPO=$(cd -- "$HERE/../.." && pwd)
IMAGE="${IMAGE:-obsbot4linux-appimage-builder:22.04}"

# Resolve the SDK dir (sdk/ is frequently a symlink on dev boxes — a bind
# mount of the repo would carry a dangling link into the container).
SDK_ROOT=$(readlink -f "${SDK_ROOT:-$REPO/sdk/libdev_v2.1.0_8}")
[ -f "$SDK_ROOT/linux/x86_64-release/libdev.so" ] || {
    echo "error: libdev.so not found under $SDK_ROOT — see docs/INSTALL.md §1 (or set SDK_ROOT=…)" >&2; exit 1; }

command -v docker >/dev/null || { echo "error: docker not found" >&2; exit 1; }

echo ">> builder image: $IMAGE"
docker build -q -t "$IMAGE" "$HERE/docker" >/dev/null

echo ">> building inside $IMAGE (repo: $REPO, SDK: $SDK_ROOT)"
docker run --rm \
    --user "$(id -u):$(id -g)" \
    -e HOME=/tmp \
    -e SDK_ROOT=/sdk \
    -v "$REPO:/work" \
    -v "$SDK_ROOT:/sdk:ro" \
    -w /work \
    "$IMAGE" \
    bash -euo pipefail -c 'qt/packaging/build-appimage.sh'

echo ">> host smoke test:"
"$REPO/dist/OBSBOT4Linux-x86_64.AppImage" --appimage-extract-and-run --help | head -1
