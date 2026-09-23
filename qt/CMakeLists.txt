# OBSBOT Tiny 3 Command Center — Qt 6 / QML build.
#
# Repo-local SDK: links the vendored libdev.so from ../sdk/libdev_v2.1.0_8 with an
# rpath pointing at that directory. No global SDK install, no sudo. The launcher
# script also exports LD_LIBRARY_PATH as a belt-and-braces fallback.
cmake_minimum_required(VERSION 3.21)

project(obsbot4linux VERSION 0.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# ---- Vendored OBSBOT SDK (repo-local) --------------------------------------
# Overridable (-DSDK_ROOT=/abs/path, as docs/INSTALL.md says) — it was a plain
# set() before, so the documented override silently did nothing.
set(SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../sdk/libdev_v2.1.0_8" CACHE PATH
    "OBSBOT libdev SDK root (contains include/ and linux/x86_64-release/libdev.so)")
set(SDK_INC  "${SDK_ROOT}/include")
set(SDK_LIB  "${SDK_ROOT}/linux/x86_64-release")

if(NOT EXISTS "${SDK_LIB}/libdev.so")
    message(FATAL_ERROR "libdev.so not found at ${SDK_LIB} — is the repo-local SDK present?")
endif()

# ---- Qt 6 ------------------------------------------------------------------
# Minimum is Qt 6.4 so the LTS distros build from their own packages (Ubuntu
# 24.04 / Debian 12 ship 6.4.2 — issue #15). The two 6.5+ conveniences the code
# used (QQmlApplicationEngine::loadFromModule, QVideoFrame(QImage)) have
# version-guarded fallbacks in main.cpp / PreviewEngine.cpp.
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Quick Gui Qml QuickControls2 Multimedia Widgets)
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.5)
    qt_standard_project_setup(REQUIRES 6.5)
else()
    qt_standard_project_setup()
endif()

# QML sources live in the qml/ subdirectory. Opt into the modern qmldir handling
# when the running Qt knows this policy (6.8+); silences QTP0004 and keeps type
# resolution deterministic across Qt 6.8→6.10+. Guarded so 6.5–6.7 still build.
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

qt_add_executable(obsbot4linux
    src/main.cpp
    src/CameraController.cpp
    src/CameraWorker.cpp
    src/PreviewEngine.cpp
    src/Settings.cpp
    src/UvcControls.cpp
    src/V4l2Scan.cpp
    src/TrayController.cpp
)

# Window + tray icon, compiled in so the source build and the AppImage both
# have it without a theme lookup (:/icons/obsbot4linux-256.png).
qt_add_resources(obsbot4linux "appicons"
    PREFIX "/icons"
    BASE packaging/icons
    FILES packaging/icons/obsbot4linux-256.png packaging/icons/obsbot4linux.svg
)

# Theme.qml is a singleton (pragma Singleton).
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

# RESOURCE_PREFIX is pinned so the qrc URL of Main.qml is the same on every Qt
# version (6.5+ defaults to /qt/qml via QTP0001; 6.4 defaults to /) — the 6.4
# fallback in main.cpp loads it by URL.
qt_add_qml_module(obsbot4linux
    URI Obsbot
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/GlassPanel.qml
        qml/ActionButton.qml
        qml/ToggleChip.qml
        qml/Segmented.qml
        qml/Stepper.qml
        qml/StatusPill.qml
        qml/NavRail.qml
        qml/RingGlyph.qml
        qml/PtzPad.qml
        qml/Viewfinder.qml
        qml/LogView.qml
        qml/KeyValue.qml
        qml/SectionLabel.qml
        qml/ControlPage.qml
        qml/ImagePage.qml
        qml/TrackingPage.qml
        qml/PresetsPage.qml
        qml/SettingsPage.qml
        qml/LogPage.qml
)

target_include_directories(obsbot4linux PRIVATE ${SDK_INC})
target_compile_options(obsbot4linux PRIVATE -Wall -Wextra)
target_compile_definitions(obsbot4linux PRIVATE APP_VERSION="${PROJECT_VERSION}")

target_link_libraries(obsbot4linux PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Gui
    Qt6::Qml
    Qt6::Multimedia
    Qt6::Widgets      # QSystemTrayIcon / QMenu only (the UI itself is QML)
    "${SDK_LIB}/libdev.so"
    pthread
)

include(GNUInstallDirs)

# INSTALL_SDK=ON copies libdev.so next to the installed app
# (<prefix>/lib/obsbot4linux/) and points the rpath there. For LOCAL packaging
# only (e.g. the AUR PKGBUILD builds a package on the user's own machine from
# the SDK the user supplies) — the SDK is OBSBOT's and must never be shipped in
# a distributed package or repo. Off by default: the repo build keeps the
# rpath on the repo-local sdk/ dir.
option(INSTALL_SDK "Install libdev.so alongside the app (local packaging only, never redistribute)" OFF)
if(INSTALL_SDK)
    set(_sdk_rpath "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}/obsbot4linux")
else()
    set(_sdk_rpath "${SDK_LIB}")
endif()

# Repo-local runtime linking: find libdev.so.1 (SONAME) from the vendored dir.
set_target_properties(obsbot4linux PROPERTIES
    BUILD_WITH_INSTALL_RPATH FALSE
    BUILD_RPATH   "${SDK_LIB}"
    INSTALL_RPATH "${_sdk_rpath}"
)

# ---- Install rules (used to assemble the AppImage AppDir) ------------------
install(TARGETS obsbot4linux
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(INSTALL_SDK)
    file(GLOB _sdk_libs "${SDK_LIB}/libdev.so*")
    install(FILES ${_sdk_libs} DESTINATION ${CMAKE_INSTALL_LIBDIR}/obsbot4linux)
endif()
install(FILES packaging/obsbot4linux.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES packaging/icons/obsbot4linux-256.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
    RENAME obsbot4linux.png)
install(FILES packaging/icons/obsbot4linux-128.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/128x128/apps
    RENAME obsbot4linux.png)
install(FILES packaging/icons/obsbot4linux.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)

# ---- Hardware-free tests (no SDK, no camera) -------------------------------
option(BUILD_TESTS "Build hardware-free tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_executable(settings_roundtrip_test tests/settings_roundtrip_test.cpp src/Settings.cpp)
    target_link_libraries(settings_roundtrip_test PRIVATE Qt6::Core)
    add_test(NAME settings_roundtrip COMMAND settings_roundtrip_test)
    # Hardware smoke test for the UVC control backend (issue #16): needs an
    # OBSBOT camera on a V4L2 node, so it is built but NOT registered with
    # ctest — run it by hand:  ./qt/build/uvc_controls_smoke
    add_executable(uvc_controls_smoke tests/uvc_controls_smoke.cpp src/UvcControls.cpp src/V4l2Scan.cpp)
    target_link_libraries(uvc_controls_smoke PRIVATE Qt6::Core)
endif()
