#!/usr/bin/env bash
# Build an .rpm for clientosh on Fedora/RHEL/openSUSE.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build-rpm}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"
BUILD_CHANNEL="${CLIENTOSH_BUILD_CHANNEL:-dev}"
BUILD_NUMBER="${CLIENTOSH_BUILD_NUMBER:-}"

case "$BUILD_CHANNEL" in
  dev|stable)
    ;;
  beta)
    if [[ ! "$BUILD_NUMBER" =~ ^[1-9][0-9]*$ ]]; then
      echo "Error: beta builds require a positive numeric CLIENTOSH_BUILD_NUMBER." >&2
      echo "Example: CLIENTOSH_BUILD_CHANNEL=beta CLIENTOSH_BUILD_NUMBER=1 $0" >&2
      exit 1
    fi
    ;;
  *)
    echo "Error: CLIENTOSH_BUILD_CHANNEL must be dev, beta, or stable." >&2
    exit 1
    ;;
esac

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
  "-DCLIENTOSH_BUILD_CHANNEL=$BUILD_CHANNEL"
  "-DCLIENTOSH_BUILD_NUMBER=$BUILD_NUMBER"
)

if [[ -n "${CMAKE_PREFIX_PATH:-}" ]]; then
  cmake_args+=("-DCMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH")
fi

echo "==> Configuring ($BUILD_DIR, channel=$BUILD_CHANNEL${BUILD_NUMBER:+, build=$BUILD_NUMBER})"
cmake "${cmake_args[@]}"

echo "==> Building"
cmake --build "$BUILD_DIR" -j"$JOBS"

echo "==> Packaging .rpm"
cpack --config "$BUILD_DIR/CPackConfig.cmake" -G RPM -B "$BUILD_DIR"

echo "==> Embedded version: $(tr -d '\r\n' < "$BUILD_DIR/clientosh-version.txt")"

echo
echo "Done. Package(s):"
find "$BUILD_DIR" -maxdepth 1 -name '*.rpm' -print
