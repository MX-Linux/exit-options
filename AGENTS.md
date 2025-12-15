# Repository Guidelines

## Project Overview
`exit-options` is a small Qt6 Widgets app that shows a popup dialog with session/power actions (logout, reboot, shutdown, suspend, lock) targeted at MX Fluxbox and other lightweight desktops.

## Project Structure
- `main.cpp`, `mainwindow.{h,cpp}`, `common.h`: application code (currently kept at repo root).
- `icons/`: bundled icon themes used by the UI.
- `translations/`: Qt Linguist sources (`*.ts`).
- `exit-options.qrc`: Qt resource collection for assets.
- `debian/`: Debian packaging (also the source of the app version via `debian/changelog`).
- `build.sh`: convenience build script; `build/` is a local build output directory (do not commit generated artifacts).

## Build, Run, and Packaging
Prereqs: Qt6 (Core/Gui/Widgets/LinguistTools), CMake ≥ 3.16, Ninja; on Debian-based systems install `dpkg-dev` (CMake reads version via `dpkg-parsechangelog`).

- `./build.sh`: configure + build with CMake/Ninja (Release by default).
- `./build.sh --clean`: remove `build/` and common packaging artifacts.
- `./build.sh --debug`: Debug build.
- `./build.sh --clang`: build with clang.
- `./build.sh --debian`: build a Debian package via `debuild` and place results in `debs/`.
- Run locally: `./build/exit-options --help` (also supports `-h/--horizontal`, `-v/--vertical`, `-t/--timeout`).

## Coding Style & Naming
- C++20, Qt idioms (prefer `QStringLiteral`, `QSettings`, `QStandardPaths`).
- Indentation: 4 spaces; keep formatting consistent with nearby code (braces on next line for functions/classes).
- Keep builds warning-clean: the project enables strict warnings and treats many as errors.

## Testing Guidelines
No dedicated automated test suite in this repo. For changes, do a quick manual smoke test:
- Launch with `--help`, then run once with `--horizontal` and `--vertical`.
- Verify config precedence works with `~/.config/MX-Linux/exit-options.conf` and `/etc/exit-options.conf`.

## Commits & Pull Requests
- Commit subjects in this repo are short, imperative, and descriptive (e.g., “Update translations”, “Fix fallback”); avoid trailing periods.
- PRs should include: what/why, relevant issue link, and a screenshot/GIF for UI or icon changes. Note the distro/Qt version used to test.

