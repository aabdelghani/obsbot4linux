# Installation & Build Guide

OBSBOT4Linux — native Linux (Qt 6 / QML). This covers the SDK
requirement, the prebuilt AppImage, building from source, and troubleshooting.

---

## 1. The OBSBOT SDK (required, not included)

This app links OBSBOT's proprietary **`libdev`** SDK. It is **not** distributed
with this repository — you must obtain it from OBSBOT and place it under `sdk/`:

```
sdk/
└── libdev_v2.1.0_8/
    ├── include/            # dev/dev.hpp, dev/devs.hpp, util/comm.hpp
    └── linux/
        └── x86_64-release/
            ├── libdev.so
            ├── libdev.so.1
            └── libdev.so.1.0.3
```

The build expects `sdk/libdev_v2.1.0_8/linux/x86_64-release/libdev.so`. If your
SDK version directory is named differently, either rename it to
`libdev_v2.1.0_8` or point the build at it:

```sh
cmake -S qt -B qt/build -DSDK_ROOT=/absolute/path/to/your/libdev_dir
```

The `sdk/` folder is git-ignored, so it never gets committed or published.

---

## 2. Prebuilt AppImage (easiest)

For 64-bit Linux with **glibc 2.35 or newer** — Ubuntu 22.04 / 24.04, Debian 12,
Fedora 36+, Arch / CachyOS and anything more recent. (Release builds are made
inside an Ubuntu 22.04 container precisely so LTS systems can run them; an
AppImage built on a rolling distro would demand that distro's glibc, which is
what the `GLIBC_2.43' not found` failure on Ubuntu 24.04 was. Check yours with
`ldd --version`.)

```sh
chmod +x OBSBOT4Linux-x86_64.AppImage
./OBSBOT4Linux-x86_64.AppImage

# confirm it sees your camera (headless, no window):
./OBSBOT4Linux-x86_64.AppImage --self-test
```

The AppImage bundles Qt, the QML runtime and the OBSBOT SDK; it uses your
system's GPU/OpenGL and X11/xcb libraries (so it runs on KDE and GNOME via
XWayland). Nothing is installed system-wide.

**FUSE note.** The AppImage uses the static AppImage runtime, so it does **not**
need the old `libfuse2` package. At run time it needs the kernel's `/dev/fuse`
and a `fusermount` helper on `$PATH`. The `fuse3` package every current distro
ships provides `fusermount3` plus (normally) a `fusermount` compatibility link.
If you see *"No suitable fusermount binary found on the $PATH"* or any other
mount error, either of these works:

```sh
# tell the runtime the helper's name
FUSERMOUNT_PROG=fusermount3 ./OBSBOT4Linux-x86_64.AppImage

# or skip FUSE entirely (extracts to a temp dir and runs from there)
./OBSBOT4Linux-x86_64.AppImage --appimage-extract-and-run
```

Inside containers or on systems without `/dev/fuse`, `--appimage-extract-and-run`
is the way to go.

**GL fallback:** if a specific GPU/driver can't create an OpenGL context, force
software rendering:

```sh
QT_QUICK_BACKEND=software ./OBSBOT4Linux-x86_64.AppImage
```

---

## 3. Build from source

### Dependencies

**Arch / CachyOS**
```sh
sudo pacman -S --needed cmake qt6-base qt6-declarative qt6-multimedia qt6-wayland
# optional: ttf-jetbrains-mono (chrome font), ffmpeg (external preview)
```

**Ubuntu / Debian**
```sh
sudo apt install cmake g++ \
  qt6-base-dev qt6-declarative-dev \
  qml6-module-qtquick qml6-module-qtquick-controls \
  qml6-module-qtquick-layouts qml6-module-qtquick-window \
  qt6-multimedia-dev qml6-module-qtmultimedia \
  qt6-wayland
```

**Fedora**
```sh
sudo dnf install cmake gcc-c++ qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtwayland
```

Requires **Qt 6.4 or newer** and **CMake 3.21+** — the Qt packages of Ubuntu
24.04 / Debian 12 (6.4.2) are enough; Qt 6.9 is what the release AppImage uses.

### Build & run

The repo-root launcher builds on first run and launches with the right library
paths and platform:

```sh
./obsbot4linux
```

or manually:

```sh
cmake -S qt -B qt/build -DCMAKE_BUILD_TYPE=Release
cmake --build qt/build -j
./qt/build/obsbot4linux
```

The binary is linked with an rpath to `sdk/.../libdev.so`, so there is no global
SDK install and no `sudo`.

### Hardware-free tests

```sh
ctest --test-dir qt/build --output-on-failure         # settings/preset persistence
./qt/build/obsbot4linux --self-test         # discovery + clean shutdown
QT_QPA_PLATFORM=offscreen ./qt/build/obsbot4linux   # load the full UI headless
```

### Build your own AppImage

Portable build (the way releases are made) — needs only docker:

```sh
qt/packaging/build-appimage-docker.sh   # → dist/OBSBOT4Linux-x86_64.AppImage, glibc ≥ 2.34
```

It builds an Ubuntu 22.04 image with Qt 6.9 once (cached), compiles inside it
and prints the minimum glibc the bundle requires. A native build on your own
machine also works but inherits *your* glibc, so it only runs on distros at
least as new as yours:

```sh
qt/packaging/build-appimage.sh          # native; bundles Qt + the SDK, prunes host GL/X11 libs
```

See `qt/packaging/README.md` for details and options (e.g. `WITH_WAYLAND=1`,
`SDK_ROOT=…`).

### Arch Linux / CachyOS package

`packaging/aur/PKGBUILD` builds a native package from a release tag with the
distro's Qt. The SDK is never downloaded — you supply yours and the package is
built (and stays) on your machine:

```sh
OBSBOT_SDK_ROOT=/path/to/libdev_v2.1.0_8 makepkg -si
```

---

## 4. Running & permissions

- **No sudo needed.** SDK USB discovery works unprivileged; you just need to be in
  the `video` group (default on most distros): `groups | grep video`.
- **`ffplay` preview** is optional — install `ffmpeg` to enable the preview button.
  It opens the camera in a separate window and will conflict with a browser / OBS /
  Meet using the camera at the same time.
- **Config** is stored per-user at
  `~/.config/obsbot4linux/obsbot4linux.json`
  (override with `OBSBOT4LINUX_CONFIG=/path/to/file`).
- **Several OBSBOT cameras:** the app controls one at a time. A *Camera* picker
  appears in the top bar when more than one is attached; the choice is remembered.
- **Debug log on the terminal:** `OBSBOT4LINUX_LOG_STDERR=1 ./obsbot4linux`
  mirrors the in-app activity log to stderr.

---

## 5. Troubleshooting

| Symptom | Fix |
|---|---|
| `libdev.so not found` at configure | Place the SDK under `sdk/` (section 1) or pass `-DSDK_ROOT=…`. |
| `` version `GLIBC_2.xx' not found `` when starting the AppImage | Your glibc is older than the one the AppImage was built against. Use a release ≥ 0.4.0 (built for glibc 2.35+), or build it yourself with `qt/packaging/build-appimage-docker.sh`. |
| `No suitable fusermount binary found` | `FUSERMOUNT_PROG=fusermount3 ./…AppImage`, or `--appimage-extract-and-run` (section 2). |
| CMake wants Qt 6.5 but the distro has 6.4 | Fixed in 0.4.0 — Qt 6.4 is the minimum now. |
| White balance / exposure greyed out | Those go through the camera's standard UVC controls, so the app needs read/write access to the `/dev/videoN` node (`video` group). Check `v4l2-ctl -d /dev/video0 -L` lists `white_balance_automatic`. |
| Camera not detected (`--self-test` says NO DEVICE) | Check USB; ensure you're in the `video` group; close other apps holding the camera. |
| AppImage won't mount | Install FUSE2 or use `--appimage-extract-and-run`. |
| `QRhiGles2: Failed to create context` | `QT_QUICK_BACKEND=software ./…AppImage`. |
| Wants a specific platform | `QT_QPA_PLATFORM=xcb` (or `wayland`) before the binary. |
| Preview shows MJPEG warnings | Harmless camera-stream quirks; the app runs `ffplay -loglevel error` to hide them. |
