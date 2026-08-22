#!/usr/bin/env bash
# Build an .rpm for clientosh on Fedora/RHEL/openSUSE.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build-rpm}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"

for tool in cmake cpack rpmbuild; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "Error: required command '$tool' was not found in PATH." >&2
    if [[ "$tool" == "rpmbuild" ]]; then
      echo "Install the RPM build tools (for example: sudo dnf install rpm-build)." >&2
    fi
    exit 1
  fi
done

cmake_args=(
  -S "$ROOT"
  -B "$BUILD_DIR"
  -G "Unix Makefiles"
  -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_INSTALL_PREFIX=/usr
)

if [[ -n "${CMAKE_PREFIX_PATH:-}" ]]; then
  cmake_args+=("-DCMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH")
fi

echo "==> Configuring ($BUILD_DIR)"
cmake "${cmake_args[@]}"

echo "==> Building"
cmake --build "$BUILD_DIR" -j"$JOBS"

echo "==> Packaging .rpm"
cpack --config "$BUILD_DIR/CPackConfig.cmake" -G RPM -B "$BUILD_DIR"

echo
echo "Done. Package(s):"
find "$BUILD_DIR" -maxdepth 1 -name '*.rpm' -print
