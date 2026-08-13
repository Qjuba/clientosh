# clientosh

A raw, dark-gray SSH client with a split-pane terminal, built in C++17 with **Qt 6** and **libssh**. Ships native support for Windows, Linux, and macOS.

![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)
![C++17](https://img.shields.io/badge/C%2B%2B-17-brightgreen)
![Qt 6](https://img.shields.io/badge/Qt-6-green)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey)
[![CI](https://github.com/hdmain/clientosh/actions/workflows/ci.yml/badge.svg)](https://github.com/hdmain/clientosh/actions/workflows/ci.yml)
[![Release](https://github.com/hdmain/clientosh/actions/workflows/release.yml/badge.svg)](https://github.com/hdmain/clientosh/actions/workflows/release.yml)

---

## Overview

clientosh is designed for people who manage a lot of remote hosts and want a fast, no-nonsense desktop client. Terminal and SFTP panes live in the **same OS window**; you can open multiple sessions and **split them into tiled panes** without juggling separate windows.

While the UI is intentionally minimal and opinionated, the underlying SSH/SFTP stack is real and non-blocking: connect, authenticate, and browse happen on worker threads so the interface never freezes.

---

## Features

- **Split-pane terminal workspace** — drag a session tab onto another tab to preview its viewport, then drop on a Left / Right / Top / Bottom zone to split. Terminal and SFTP panes share one window.
- **Session management** — save named profiles (host, port, user, add optional password / SSH key), switch between SSH and SFTP-only modes, and jump back in from the dashboard.
- **Keyring** — store SSH private keys (and passwords/passphrases) in the OS keyring (Windows Credential Manager / Keychain / Secret Service, with an encrypted file fallback), then pick a pre-saved key from a dropdown when creating or configuring a host.
- **Authentication** — password or private-key (with optional passphrase). Saved credentials stay local and are protected by an encrypted vault backed by the OS keyring.
- **Live server stats** — per-session CPU / RAM / disk readouts pulled from a dedicated SSH channel.
- **Bundled SFTP file manager** — browse, upload, download, and manage remote files for the active session.
- **Scrollback & keyword highlighting** — full terminal emulation with a scrollable history buffer.
- **Detachable & attachable terminals** — pull a terminal out into its own view, then dock it back.
- **Dark monospace theme** — flat, high-contrast raw look with subtle animated motion (`src/ui/Motion`).
- **Cross-platform** — Windows (MinGW), Linux (deb packaging via CPack), and macOS.

---

## Screenshots

> Placeholder — add your own images to `/docs/screenshots/` and reference them here:

| Dashboard | Split workspace | SFTP browser |
|---|---|---|
| ![Dashboard](docs/screenshots/dashboard.png) | ![Workspace](docs/screenshots/workspace.png) | ![SFTP](docs/screenshots/sftp.png) |

---

## Quick start

### Requirements

- CMake 3.21+
- A C++17 compiler (GCC, Clang, MinGW, or MSVC)
- Qt 6 (Modules: Widgets, Network, Svg)
- libssh (2.x)

### Install dependencies

<details>
<summary><b>Windows — MSYS2 (MinGW / UCRT64)</b></summary>

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake \
  mingw-w64-x86_64-qt6-base mingw-w64-x86_64-qt6-svg \
  mingw-w64-x86_64-libssh
```

Or build against an existing Qt install (see *Windows build* below).
</details>

<details>
<summary><b>Debian / Ubuntu / WSL</b></summary>

```bash
sudo apt install build-essential cmake pkg-config \
  qt6-base-dev libqt6svg6-dev libssh-dev
```
</details>

<details>
<summary><b>Fedora</b></summary>

```bash
sudo dnf install cmake gcc-c++ qt6-qtbase-devel qt6-qtsvg-devel libssh-devel
```
</details>

<details>
<summary><b>macOS — Homebrew</b></summary>

```bash
brew install cmake qt libssh pkg-config
```
</details>

---

## Building

> **Important:** use a **separate build directory per platform**. Reusing a Windows `build/` from WSL (or the reverse) fails because CMake caches generators and paths differ.

### Linux / WSL / macOS

```bash
cmake -S . -B build-linux -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu || echo 4)"
./build-linux/clientosh
```

If CMake cannot find Qt, point it at the Qt6 include tree:

```bash
cmake -S . -B build-linux -DCMAKE_PREFIX_PATH=/usr            # Debian/Ubuntu (usually /usr)
# or, for a manual Qt build:
cmake -S . -B build-linux -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/gcc_64
```

### Windows — Qt + MinGW (pinned Qt 6.9.2)

The recommended way is the CMake preset, which pins the build to **exactly Qt 6.9.2**
plus its matching **MinGW 13.1.0** toolchain and libssh/OpenSSL from MSYS2 — the same
set the release CI and the locally tested kit use. This keeps the rendered app and
its behavior identical to the released build (the rolling MSYS2 Qt renders and behaves
differently, which is why the app can look off / misbehave when built against it).

```powershell
cmake --preset windows-qt692-mingw   # Qt 6.9.2 + MinGW 13.1, outputs to build-win/
cmake --build  --preset windows-qt692-mingw
.\build-win\clientosh.exe

# Optional: repackage the installer after a rebuild
& "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" build-win\installer\clientosh_installer.iss
```

The same configuration spelled out manually:

```powershell
cmake -S . -B build-win -G "MinGW Makefiles" `
  -DCMAKE_MAKE_PROGRAM="C:/Qt/Tools/mingw1310_64/bin/mingw32-make.exe" `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.9.2/mingw_64" `
  -DCMAKE_C_COMPILER="C:/Qt/Tools/mingw1310_64/bin/gcc.exe" `
  -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw1310_64/bin/g++.exe" `
  -DLIBSSH_ROOT="C:/msys64/mingw64"

cmake --build build-win
.\build-win\clientosh.exe
```

> **Note** — build from a shell where the Qt toolchain `bin` is on `PATH`
> (`C:/Qt/Tools/mingw1310_64/bin`), otherwise CMake's compiler test can fail on the
> plain PowerShell prompt. The preset above injects it automatically.

`LIBSSH_ROOT` is optional when pkg-config can find libssh. When set, CMake also copies the required runtime DLLs (libssh, OpenSSL, MinGW runtime) next to the executable. The build additionally runs `windeployqt --release` as a post-build step to bundle the full Qt runtime (platforms, styles, imageformats, tls, …) so the exe is self-contained.

---

## Installation packages

### Linux — .deb

```bash
chmod +x scripts/build-deb.sh
./scripts/build-deb.sh
sudo apt install ./build-deb/clientosh_*.deb
```

The package is produced with CPack and installs the desktop entry, app icon, and metainfo under `/usr`.

### Windows — Inno Setup / NSIS installer

CMake generates the installer scripts at configure time from `.in` templates
(`packaging/clientosh_installer.iss.in`, `packaging/clientosh_installer.nsi.in`)
into the build directory. They read the version and absolute build paths from
CMake, so **the version stays a single source of truth** in `project(...)`.

Inside the build directory, compile with [Inno Setup](https://jrsoftware.org/isinfo.php)
or NSIS (`makensis`):

```bash
# after cmake -S . -B build-win, the generated scripts live here:
#   build-win/installer/clientosh_installer.iss
#   build-win/installer/clientosh_installer.nsi
makensis build-win/installer/clientosh_installer.nsi
```

The result is `clientosh-<version>-setup.exe` in the repo root.

---

## Usage

1. **Add a session** from the dashboard — give it a name, host, port, and user.
2. **Connect** with a password or a private key; choose **SSH** (interactive terminal) or **SFTP only**.
3. **Split panes** — drag a tab onto another tab, then drop onto a Left / Right / Top / Bottom zone.
4. **SFTP** — click the folder icon in the top bar to open the file manager for the active session (browse / upload / download).
5. **Detach / attach** — pull a terminal into its own viewport, then dock it back into the workspace.

> **Security note:** host key checking is currently disabled to keep the local client raw and friction-free. Prefer using clientosh on trusted networks until key verification is implemented.

---

## Architecture

```
src/
├── main.cpp                 # application entry point
├── core/                    # backend / network layer
│   ├── SshSession.*         # managed SSH interactive session
│   ├── SessionManager.*     # lifecycle & ownership of sessions
│   ├── SessionProfile.h     # profile model + QSettings persistence
│   ├── SftpClient.*         # SFTP file operations (worker thread)
│   ├── ServerStatsClient.*  # live CPU/RAM/disk polling (worker thread)
│   ├── FontManager.*        # cross-platform monospace font resolution
│   └── AppSettings.h        # application-level settings
├── ui/                      # motion / animation helpers
│   └── Motion.*             # eased glows, hovers, and transitions
├── TerminalWidget.*         # terminal emulator
├── SessionWorkspace.*       # split-pane workspace & docking
├── PaneFrame.*              # a single terminal/SFTP pane container
├── SessionChip.*            # session tab ("chip") with drag & drop
├── DropOverlay.*            # split-preview overlay on hover
├── DashboardPage.*          # session list / launcher
├── TopNavBar.*              # top navigation & actions
├── SftpWindow.*             # SFTP browser UI
└── MainWindow.*             # top-level window tying it all together
```

**Threading model:** all network work (auth, shell I/O, SFTP, stats polling) runs on worker threads; the GUI thread never blocks on connect or auth. Signals queue updates back to the UI thread.

---

## Configuration

Session profiles are persisted with Qt's `QSettings` (registry on Windows, INI/plist elsewhere). Only profiles explicitly marked *save password* store the credential; rejected by default.

---

## Contributing

Contributions are welcome! Please:

1. Fork the repository.
2. Create a feature branch (`git checkout -b feat/my-change`).
3. Keep builds clean on Linux, macOS, and Windows.
4. Open a pull request against `main`.

The CI workflow builds and smoke-tests the project on Ubuntu, macOS, and Windows (MSYS2) for every push and PR, and the `Release` workflow produces `.deb` and Windows binaries on version tags.

---

## License

Released under the [MIT License](LICENSE).
