# AUR packaging (issue #5)

`PKGBUILD` builds OBSBOT4Linux from a release tag with the distro's Qt 6
packages. Because the OBSBOT SDK is proprietary it is **not** downloaded;
the user supplies it and the package is built locally:

```sh
OBSBOT_SDK_ROOT=/path/to/libdev_v2.1.0_8 makepkg -si
```

The app is installed to `/usr/bin/obsbot4linux`, the user's own copy of
`libdev.so` to `/usr/lib/obsbot4linux/` (the binary's rpath points there via
the `INSTALL_SDK=ON` CMake option), plus the `.desktop` file and icons.

Only this recipe may be published to the AUR; a built package contains the
SDK and must stay on the machine it was built on.

Verified in a clean `archlinux:latest` container (see docs/INSTALL.md).
