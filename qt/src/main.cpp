// OBSBOT Tiny 3 Command Center — Qt 6 / QML entry point.
//
// GUI mode: loads the QML "Command Center" UI and starts USB discovery off the
// UI thread (via CameraController's worker QThread).
//
// --self-test: runs headless (offscreen platform), performs discovery once, and
// exits with a code that reflects the DEVICE result only:
//     0 = device found, 3 = no device by timeout, 2 = app/init error.
// (CODE_REVIEW #11: a headless run no longer returns 0 for "no device".)
#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "CameraController.h"
#include "PreviewEngine.h"
#include "TrayController.h"
#include "UvcControls.h"

int main(int argc, char **argv) {
    int waitMs = 6000;
    bool selfTest = false;
    bool startInTray = false;   // --tray: start hidden (autostart entry uses it)

    for (int i = 1; i < argc; ++i) {
        const QString a = QString::fromLocal8Bit(argv[i]);
        if (a == "--self-test") {
            selfTest = true;
        } else if (a == "--tray") {
            startInTray = true;
        } else if (a == "--wait-ms" && i + 1 < argc) {
            waitMs = std::max(0, std::atoi(argv[++i]));
        } else if (a == "-h" || a == "--help") {
            std::printf("Usage: %s [--wait-ms N] [--tray] [--self-test]\n", argv[0]);
            std::printf("  --wait-ms N   USB discovery timeout in ms (default 6000)\n");
            std::printf("  --tray        Start hidden in the system tray (used by the autostart entry)\n");
            std::printf("  --self-test   Run discovery once headless, then exit\n");
            std::printf("                (exit 0 = device found, 3 = none, 2 = init error)\n");
            return 0;
        }
    }

    // Headless self-test must not require a display server (override whatever the
    // session / AppImage set, e.g. xcb).
    if (selfTest) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    } else if (qEnvironmentVariableIsSet("APPDIR") || qEnvironmentVariableIsSet("APPIMAGE")) {
        // Running from the AppImage, which bundles only the xcb platform plugin.
        // A Wayland session that exports QT_QPA_PLATFORM=wayland would make Qt
        // abort (no wayland plugin bundled), so steer to xcb — it runs fine on
        // Wayland via XWayland. Override with OBSBOT_QT_PLATFORM=… if desired.
        // (Gated on APPDIR/APPIMAGE so a normal source build can still go native.)
        const QByteArray override = qgetenv("OBSBOT_QT_PLATFORM");
        const QByteArray current = qgetenv("QT_QPA_PLATFORM");
        if (!override.isEmpty())
            qputenv("QT_QPA_PLATFORM", override);
        else if (current.isEmpty() || current.contains("wayland"))
            qputenv("QT_QPA_PLATFORM", "xcb");
    }

    // QApplication (not QGuiApplication) only for QSystemTrayIcon/QMenu; the
    // UI itself is pure QML.
    QApplication app(argc, argv);
    QApplication::setApplicationName("OBSBOT4Linux");
    QApplication::setOrganizationName("obsbot4linux");
    QApplication::setDesktopFileName("obsbot4linux");   // Wayland/GNOME icon + WM class association
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/obsbot4linux-256.png")));

    CameraController controller;

    // OBSBOT4LINUX_LOG_STDERR=1 mirrors the activity log to stderr in GUI mode
    // too (debugging / driving the app from a script); the self-test always does.
    if (!selfTest && qEnvironmentVariableIntValue("OBSBOT4LINUX_LOG_STDERR") > 0) {
        QObject::connect(&controller, &CameraController::logLine, &app,
                         [](const QString &k, const QString &m) {
                             std::fprintf(stderr, "[%s] %s\n", qPrintable(k), qPrintable(m));
                         });
    }

    if (selfTest) {
        int code = 3;   // default: no device
        QObject::connect(&controller, &CameraController::logLine, &app,
                         [](const QString &k, const QString &m) {
                             std::fprintf(stderr, "[%s] %s\n", qPrintable(k), qPrintable(m));
                         });
        QObject::connect(&controller, &CameraController::discoveryFinished, &app,
                         [&](bool found) {
                             if (found) {
                                 code = 0;
                                 std::fprintf(stdout,
                                     "[self-test] device: FOUND product=%s SN=%s fw=%s enum=%d\n",
                                     qPrintable(controller.property("product").toString()),
                                     qPrintable(controller.property("sn").toString()),
                                     qPrintable(controller.property("firmware").toString()),
                                     controller.property("enumId").toInt());
                             } else {
                                 std::fprintf(stdout, "[self-test] device: NO DEVICE (timeout %d ms)\n", waitMs);
                             }
                             app.quit();
                         });
        // Safety net if discovery never reports back.
        QTimer::singleShot(waitMs + 3000, &app, [&]() {
            std::fprintf(stderr, "[self-test] safety timeout — discovery did not report back\n");
            app.quit();
        });
        std::fprintf(stdout, "[self-test] UI backend: %s (offscreen headless)\n",
                     qPrintable(QGuiApplication::platformName()));
        controller.start(waitMs);
        app.exec();
        return code;
    }

    // Embedded preview (GUI mode only — the self-test path above never needs
    // video). Declared after the controller so it is destroyed FIRST, releasing
    // the UVC node before the controller's sleep-on-exit/shutdown sequence runs.
    PreviewEngine preview;
    // Preview events flow into the same activity log as everything else
    // (signal → signal chain).
    QObject::connect(&preview, &PreviewEngine::logLine,
                     &controller, &CameraController::logLine);
    // Keep the engine's capture mode synced to the persisted resolution choice.
    preview.setResIndex(controller.previewResIndex());
    QObject::connect(&controller, &CameraController::settingsChanged, &preview,
                     [&controller, &preview]() {
                         preview.setResIndex(controller.previewResIndex());
                     });

    // Issue #13: some models (Tiny 3 Lite) only come fully awake once a video
    // stream is open — Wake alone returns rc=0 and nothing happens. Either start
    // the preview with Wake (opt-in setting) or, if the preview is not running,
    // say so in the log so the user knows what to try.
    QObject::connect(&controller, &CameraController::wakeRequested, &app, [&]() {
        if (preview.active()) return;
        if (controller.wakeStartsPreview()) {
            emit controller.logLine("sys", QStringLiteral("wake: starting the preview too (setting: start preview on Wake)"));
            preview.start();
        } else {
            emit controller.logLine("sys", QStringLiteral(
                "wake: if the camera stays asleep, start the preview — some models (Tiny 3 Lite) "
                "only wake fully once a video stream is open (#13). Settings → \"Start preview on Wake\" automates this."));
        }
    });

    // Standard UVC controls (white balance / exposure / …, issue #16). Follows
    // the SDK-reported node of the connected camera; falls back to the first
    // OBSBOT node so it also works if the SDK is not bound yet.
    UvcControls uvc;
    QObject::connect(&uvc, &UvcControls::logLine, &controller, &CameraController::logLine);
    QObject::connect(&controller, &CameraController::videoDevPathChanged, &app,
                     [&](const QString &path) {
                         preview.setPreferredNode(path);
                         uvc.setDevicePath(path);
                     });
    // Deferred so the first "uvc: controls on …" line lands in the activity log
    // after the QML log model is connected (first event-loop pass).
    QTimer::singleShot(0, &uvc, [&uvc]() { uvc.setDevicePath(QString()); });

    // System tray + autostart. With a tray present the app stays resident:
    // closing the window hides it (setting), Quit is in the tray menu / Ctrl+Q.
    TrayController tray(&controller);
    QObject::connect(&tray, &TrayController::logLine, &controller, &CameraController::logLine);
    app.setQuitOnLastWindowClosed(!tray.available());
    if (startInTray && !tray.available())
        std::fprintf(stderr, "--tray: no system tray available — showing the window instead.\n");

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("cam", &controller);
    engine.rootContext()->setContextProperty("preview", &preview);
    engine.rootContext()->setContextProperty("uvc", &uvc);
    engine.rootContext()->setContextProperty("tray", &tray);
    engine.rootContext()->setContextProperty("startHidden", startInTray && tray.available());
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    engine.loadFromModule("Obsbot", "Main");
#else
    // Qt 6.4 (Ubuntu 24.04 / Debian 12): no loadFromModule — load the module's
    // main file by its qrc URL (RESOURCE_PREFIX is pinned in CMakeLists.txt) and
    // make the module importable from the same prefix.
    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Obsbot/qml/Main.qml")));
#endif
    if (engine.rootObjects().isEmpty()) {
        std::fprintf(stderr, "Failed to load QML UI (no display, or missing Qt Quick runtime).\n");
        return 2;
    }
    tray.attachWindow(qobject_cast<QQuickWindow *>(engine.rootObjects().first()));

    controller.start(waitMs);
    return app.exec();
}
