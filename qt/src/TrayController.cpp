#include "TrayController.h"
#include "CameraController.h"

#include <QAction>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QMenu>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QVariantMap>
#include <QWindow>

TrayController::TrayController(CameraController *cam, QObject *parent)
    : QObject(parent), m_cam(cam) {
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;

    m_tray = new QSystemTrayIcon(QIcon(QStringLiteral(":/icons/obsbot4linux-256.png")), this);
    m_menu = new QMenu;   // top-level; QSystemTrayIcon does not take ownership

    m_showAct = m_menu->addAction(QStringLiteral("Show window"), this, &TrayController::toggleWindow);
    m_statusAct = m_menu->addAction(QStringLiteral("No camera"));
    m_statusAct->setEnabled(false);
    m_menu->addSeparator();
    m_wakeAct = m_menu->addAction(QStringLiteral("Wake camera"), m_cam, &CameraController::wake);
    m_sleepAct = m_menu->addAction(QStringLiteral("Sleep camera"), m_cam, &CameraController::sleep);
    m_presetMenu = m_menu->addMenu(QStringLiteral("Go to preset"));
    m_menu->addSeparator();
    m_menu->addAction(QStringLiteral("Quit"), this, &TrayController::quit);
    m_tray->setContextMenu(m_menu);

    // Left click toggles the window (XEmbed / KDE); StatusNotifier hosts such as
    // GNOME's AppIndicator open the menu instead, which is why "Show window" is
    // also the first menu entry.
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger || r == QSystemTrayIcon::DoubleClick)
            toggleWindow();
    });

    connect(m_cam, &CameraController::connStateChanged, this, &TrayController::refreshState);
    connect(m_cam, &CameraController::statusChanged, this, &TrayController::refreshState);
    connect(m_cam, &CameraController::identityChanged, this, &TrayController::refreshState);
    connect(m_cam, &CameraController::presetsChanged, this, &TrayController::rebuildPresetMenu);
    rebuildPresetMenu();
    refreshState();
    m_tray->show();
}

TrayController::~TrayController() {
    delete m_menu;
}

void TrayController::attachWindow(QWindow *window) {
    m_window = window;
    if (m_window) {
        connect(m_window, &QWindow::visibleChanged, this, [this]() {
            if (m_showAct)
                m_showAct->setText(windowVisible() ? QStringLiteral("Hide window")
                                                   : QStringLiteral("Show window"));
            emit windowVisibleChanged();
        });
    }
}

bool TrayController::windowVisible() const { return m_window && m_window->isVisible(); }

void TrayController::showWindow() {
    if (!m_window) return;
    m_window->show();
    m_window->raise();
    m_window->requestActivate();
}

void TrayController::hideWindow() {
    if (m_window) m_window->hide();
}

void TrayController::toggleWindow() {
    if (windowVisible()) hideWindow();
    else showWindow();
}

void TrayController::quit() {
    if (m_tray) m_tray->hide();
    QCoreApplication::quit();
}

void TrayController::rebuildPresetMenu() {
    if (!m_presetMenu) return;
    m_presetMenu->clear();
    const QVariantList presets = m_cam->presets();
    bool any = false;
    for (const QVariant &v : presets) {
        const QVariantMap p = v.toMap();
        const int idx = p.value("index").toInt();
        QAction *a = m_presetMenu->addAction(
            QStringLiteral("%1  —  %2").arg(p.value("name").toString(), p.value("summary").toString()));
        a->setEnabled(p.value("set").toBool() && m_cam->connected());
        any = any || p.value("set").toBool();
        connect(a, &QAction::triggered, m_cam, [this, idx]() { m_cam->goPreset(idx); });
    }
    m_presetMenu->setEnabled(any);
}

void TrayController::refreshState() {
    if (!m_tray) return;
    QString status;
    if (m_cam->connected()) {
        status = QStringLiteral("%1 · %2").arg(m_cam->property("product").toString(),
                                               m_cam->asleep() ? QStringLiteral("asleep")
                                                               : QStringLiteral("awake"));
    } else if (m_cam->discovering()) {
        status = QStringLiteral("Discovering…");
    } else {
        status = QStringLiteral("No camera");
    }
    m_statusAct->setText(status);
    m_tray->setToolTip(QStringLiteral("OBSBOT4Linux — %1").arg(status));
    m_wakeAct->setEnabled(m_cam->connected());
    m_sleepAct->setEnabled(m_cam->connected());
    // Preset entries depend on the connection too.
    if (m_presetMenu) {
        const QVariantList presets = m_cam->presets();
        const auto acts = m_presetMenu->actions();
        for (int i = 0; i < acts.size() && i < presets.size(); ++i)
            acts[i]->setEnabled(presets[i].toMap().value("set").toBool() && m_cam->connected());
    }
}

// ---------------------------------------------------------------------------
// Start with system (XDG autostart)
// ---------------------------------------------------------------------------
QString TrayController::autostartPath() const {
    QString base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    if (base.isEmpty()) base = QDir::homePath() + QStringLiteral("/.config");
    return base + QStringLiteral("/autostart/obsbot4linux.desktop");
}

bool TrayController::startWithSystem() const { return QFile::exists(autostartPath()); }

QString TrayController::launchCommand() const {
    // Inside an AppImage the real launcher is the image itself, not the
    // extracted binary in a temporary mount.
    const QByteArray appimage = qgetenv("APPIMAGE");
    QString exe = appimage.isEmpty() ? QCoreApplication::applicationFilePath()
                                     : QString::fromLocal8Bit(appimage);
    // Desktop-entry quoting: backslash-escape the reserved characters.
    exe.replace(QLatin1Char('\\'), QStringLiteral("\\\\")).replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QStringLiteral("\"%1\" --tray").arg(exe);
}

void TrayController::setStartWithSystem(bool on) {
    const QString path = autostartPath();
    if (on == startWithSystem()) { emit startWithSystemChanged(); return; }
    if (on) {
        const QFileInfo fi(path);
        if (!fi.dir().exists() && !QDir().mkpath(fi.absolutePath())) {
            emit logLine("warn", QStringLiteral("autostart: cannot create %1").arg(fi.absolutePath()));
            emit startWithSystemChanged();
            return;
        }
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            emit logLine("warn", QStringLiteral("autostart: cannot write %1").arg(path));
            emit startWithSystemChanged();
            return;
        }
        const QString body = QStringLiteral(
            "[Desktop Entry]\n"
            "Type=Application\n"
            "Version=1.0\n"
            "Name=OBSBOT4Linux\n"
            "Comment=OBSBOT camera control (starts hidden in the tray)\n"
            "Exec=%1\n"
            "Icon=obsbot4linux\n"
            "Terminal=false\n"
            "StartupNotify=false\n"
            "X-GNOME-Autostart-enabled=true\n"
            "X-KDE-autostart-after=panel\n").arg(launchCommand());
        f.write(body.toUtf8());
        f.close();
        emit logLine("ok", QStringLiteral("autostart: enabled — %1 (%2)").arg(path, launchCommand()));
    } else {
        if (QFile::remove(path))
            emit logLine("ok", QStringLiteral("autostart: disabled — removed %1").arg(path));
        else
            emit logLine("warn", QStringLiteral("autostart: could not remove %1").arg(path));
    }
    emit startWithSystemChanged();
}
