#!/usr/bin/env python3
"""Bump clientosh version across all project files.

Usage:
    python version.py 1.0.5
    python version.py v1.0.5
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
VERSION_RE = re.compile(r"^\d+\.\d+\.\d+$")


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def write_text(path: Path, text: str) -> None:
    path.write_text(text, encoding="utf-8", newline="\n")


def current_version() -> str | None:
    cmake = ROOT / "CMakeLists.txt"
    if not cmake.exists():
        return None
    match = re.search(r"project\(clientosh VERSION (\d+\.\d+\.\d+) LANGUAGES CXX\)", read_text(cmake))
    return match.group(1) if match else None


def bump_cmake(text: str, version: str) -> tuple[str, int]:
    pattern = re.compile(r"project\(clientosh VERSION \d+\.\d+\.\d+ LANGUAGES CXX\)")
    new_text, count = pattern.subn(f"project(clientosh VERSION {version} LANGUAGES CXX)", text, count=1)
    return new_text, count


def bump_pkgbuild(text: str, version: str) -> tuple[str, int]:
    pattern = re.compile(r"^pkgver=\d+\.\d+\.\d+$", re.MULTILINE)
    return pattern.subn(f"pkgver={version}", text, count=1)


def parse_version_arg(raw: str) -> str:
    version = raw.strip().removeprefix("v")
    if not VERSION_RE.fullmatch(version):
        raise ValueError(f"invalid version {raw!r} (expected major.minor.patch, e.g. 1.0.5)")
    return version


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print("Usage: python version.py <major.minor.patch>", file=sys.stderr)
        print("Example: python version.py 1.0.5", file=sys.stderr)
        return 1

    try:
        version = parse_version_arg(argv[1])
    except ValueError as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1

    before = current_version()
    if before == version:
        print(f"Version already set to {version}")
        return 0

    updates: list[tuple[Path, str, int]] = []

    cmake_path = ROOT / "CMakeLists.txt"
    cmake_text = read_text(cmake_path)
    cmake_text, count = bump_cmake(cmake_text, version)
    if count:
        write_text(cmake_path, cmake_text)
        updates.append((cmake_path, "project(VERSION ...)", count))

    pkgbuild_path = ROOT / "packaging" / "arch" / "PKGBUILD"
    if pkgbuild_path.exists():
        pkg_text = read_text(pkgbuild_path)
        pkg_text, count = bump_pkgbuild(pkg_text, version)
        if count:
            write_text(pkgbuild_path, pkg_text)
            updates.append((pkgbuild_path, "pkgver", count))

    if not updates:
        print("Error: no version fields were updated", file=sys.stderr)
        return 1

    label = f"{before} -> {version}" if before else version
    print(f"Version updated ({label}):")
    for path, field, count in updates:
        print(f"  {path.relative_to(ROOT)} ({field}, {count} change(s))")

    print()
    print("Regenerate build files after bumping:")
    print("  cmake -S . -B build")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
