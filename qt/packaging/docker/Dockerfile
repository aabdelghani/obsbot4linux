# Portable AppImage build environment for OBSBOT4Linux (issue #15).
#
# The AppImage inherits the glibc of the machine it is built on, and glibc is
# forward-compatible only: an image built on a rolling distro (glibc 2.43) will
# not start on Ubuntu 24.04 LTS (glibc 2.39). Building inside Ubuntu 22.04
# (glibc 2.35) makes the result run on every distro released since 2022,
# including Ubuntu 22.04/24.04, Debian 12, Fedora 36+, and current Arch.
#
# Qt comes from the official Qt online installer archive via aqtinstall (the
# distro's Qt 6.2 is too old), pinned to one version so builds are reproducible.
FROM ubuntu:22.04

ARG QT_VERSION=6.9.3
ENV DEBIAN_FRONTEND=noninteractive \
    QT_VERSION=${QT_VERSION} \
    QT_ROOT=/opt/qt

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build git curl ca-certificates file \
        python3 python3-pip patchelf desktop-file-utils zsync \
        libfuse2 fuse \
        libgl1-mesa-dev libegl1-mesa-dev libglib2.0-dev libfontconfig1-dev \
        libfreetype6-dev libdbus-1-3 libxkbcommon-dev libxkbcommon-x11-dev \
        libx11-dev libx11-xcb-dev libxcb1-dev libxcb-cursor-dev libxcb-glx0-dev \
        libxrandr-dev libxcursor-dev libxinerama-dev libxfixes-dev libxext-dev libxv-dev \
        libxcb-icccm4-dev libxcb-image0-dev libxcb-keysyms1-dev libxcb-randr0-dev \
        libxcb-render-util0-dev libxcb-render0-dev libxcb-shape0-dev libxcb-shm0-dev \
        libxcb-sync-dev libxcb-util-dev libxcb-xfixes0-dev libxcb-xinerama0-dev \
        libxcb-xinput-dev libxcb-xkb-dev libxrender-dev libxi-dev libsm-dev libice-dev \
        libwayland-dev libpulse-dev libasound2-dev \
        libjpeg-turbo8-dev zlib1g-dev libpng-dev \
    && rm -rf /var/lib/apt/lists/*

# Qt 6 (base, declarative, multimedia, wayland, shadertools) — the aqtinstall
# "desktop gcc_64" kit is built against an old glibc, so it links fine on 22.04.
RUN pip3 install --no-cache-dir aqtinstall==3.2.1 \
    && aqt install-qt linux desktop ${QT_VERSION} linux_gcc_64 \
         -m qtmultimedia qtshadertools -O ${QT_ROOT} \
    && rm -rf /root/.cache

ENV PATH=${QT_ROOT}/${QT_VERSION}/gcc_64/bin:${PATH} \
    CMAKE_PREFIX_PATH=${QT_ROOT}/${QT_VERSION}/gcc_64 \
    QMAKE=${QT_ROOT}/${QT_VERSION}/gcc_64/bin/qmake \
    APPIMAGE_EXTRACT_AND_RUN=1

WORKDIR /work
