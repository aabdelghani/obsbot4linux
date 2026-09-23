# Packaging — AppImage

A self-contained **AppImage** so end users just download it, `chmod +x`, and run —
no Qt install, no build. Bundles Qt 6, the QML runtime, the platform plugins
(xcb + offscreen; wayland opt-in), and the OBSBOT SDK (`libdev.so`).

## Portability: build it in the container (issue #15)

An AppImage inherits the **glibc of the machine it is built on**, and glibc is
forward-compatible only. The v0.3.1 image was built on a rolling distro, needed
`GLIBC_2.43`, and refused to start on Ubuntu 24.04 (glibc 2.39). Releases are
therefore built inside an **Ubuntu 22.04** container (glibc 2.35) with Qt 6.9
from the Qt archive:

```sh
qt/packaging/build-appimage-docker.sh   # docker only; → dist/OBSBOT4Linux-x86_64.AppImage
```

`docker/Dockerfile` is the builder (Ubuntu 22.04 + build deps + Qt via
aqtinstall, pinned). The repo is bind-mounted, so outputs land in the checkout
as usual. The script ends with a **portability report** — the highest
`GLIBC_x.y` symbol version any bundled ELF needs (currently 2.34) — so a build
accidentally made on a newer host is caught before release, not by a user.
Hardware-validated: the container build runs on Ubuntu 24.04 (`--self-test`
finds the Tiny 3, full GUI + embedded preview work).

`SDK_ROOT=/abs/path` overrides the SDK location for both scripts (the docker
wrapper resolves a symlinked `sdk/` for you).

> **Prebuilt artifact:** `dist/OBSBOT4Linux-x86_64.AppImage` (validated:
> loads the full GUI headless with no errors; `--self-test` runs discovery +
> clean shutdown against the bundled SDK). Just `chmod +x` and run it.

## Build it (one command, on a real desktop)

```sh
qt/packaging/build-appimage.sh
```

Output: `OBSBOT4Linux-x86_64.AppImage` in the repo root. Then:

```sh
chmod +x OBSBOT4Linux-x86_64.AppImage
./OBSBOT4Linux-x86_64.AppImage            # runs on KDE and GNOME
./OBSBOT4Linux-x86_64.AppImage --self-test
```

The script: builds Release → installs into an `AppDir` → bundles `libdev.so` →
runs `linuxdeploy` + the Qt plugin (which auto-scans `qml/` for the QML modules to
include) → emits the AppImage. It downloads `linuxdeploy`, `linuxdeploy-plugin-qt`
and `appimagetool` into `qt/packaging/tools/` on first run.

### Requirements on the build machine
- `cmake`, a C++ compiler, and **Qt 6 dev** (`qmake` on PATH).
  - Arch/CachyOS: `sudo pacman -S --needed cmake qt6-base qt6-declarative`
- Standard desktop X11/xcb client libraries (present on any KDE/GNOME install) —
  the Qt `xcb` platform plugin links them. On a **headless** box they may be
  missing (e.g. `libxcb-cursor.so.0`) and the deploy step will say
  `Could not find dependency: …` — build on a normal desktop instead.
- Non-PATH Qt (e.g. an aqt install): pass `QMAKE=… CMAKE_PREFIX_PATH=…`.

### Platform: xcb by default (runs everywhere)
The AppImage ships the **xcb** platform plugin, which runs natively under X11 and
via XWayland under Wayland — so it works on KDE and GNOME out of the box. The
GPU/GL stack (`libGL`/`libEGL`/`libGLX`/`libgbm`/`libdrm`) is **deliberately not
bundled** so the host's GPU driver provides it (bundling it breaks GL context
creation → `QRhiGles2: Failed to create context` → SIGABRT).

Native Wayland is opt-in (`WITH_WAYLAND=1`), but note `linuxdeploy-plugin-qt` does
not bundle the wayland-egl graphics-integration plugin, so a native-Wayland build
can fail with "Failed to load client buffer integration wayland-egl". xcb via
XWayland is the reliable default.

If a specific GPU/driver still can't create a GL context, force software
rendering at runtime:
```sh
QT_QUICK_BACKEND=software ./OBSBOT4Linux-x86_64.AppImage
```

## Where settings live

The installed app stores settings per-user (XDG), not in the repo:
```
~/.config/obsbot4linux/obsbot4linux.json
```
Override with `OBSBOT4LINUX_CONFIG=/path`.

## Icon

`icons/obsbot4linux.svg` (source) + PNGs at 16–512 px, regenerable with
`python packaging/make_icon.py` (needs Pillow). The coral OBSBOT ring on obsidian.

## Sandbox build note

The prebuilt `dist/` AppImage was assembled in a headless CI VM. That box lacks a
few X11/xcb client libs the Qt `xcb` plugin bundles (`libxcb-cursor.so.0`,
`libxkbcommon-x11.so.0`, …) — on a **normal desktop these are already installed**,
so `build-appimage.sh` just works. To reproduce in a headless environment, stage
the missing libs onto `LD_LIBRARY_PATH` before running (they were pulled from the
distro's own runtime + the `libxcb-cursor0` package). On a real KDE/GNOME desktop
you don't need any of that.

A native `build-appimage.sh` run is built against the host's glibc; use the
docker wrapper above for anything you intend to publish.

A Flatpak manifest can be added later if Flathub distribution is wanted.
