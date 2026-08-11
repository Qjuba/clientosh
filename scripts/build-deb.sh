#!/usr/bin/env bash
# Build a .deb for clientosh on Debian/Ubuntu/WSL.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build-deb}"
JOBS="$(nproc 2>/dev/null || echo 4)"

echo "==> Configuring ($BUILD_DIR)"
cmake -S "$ROOT" -B "$BUILD_DIR" \
  -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr \
  ${CMAKE_PREFIX_PATH:+-DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH"}

echo "==> Building"
cmake --build "$BUILD_DIR" -j"$JOBS"

echo "==> Packaging .deb"
cmake --build "$BUILD_DIR" --target package

echo
echo "Done. Package(s):"
find "$BUILD_DIR" -maxdepth 1 -name '*.deb' -print
