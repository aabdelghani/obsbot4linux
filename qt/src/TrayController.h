// TrayController — system tray icon, close-to-tray, and "start with system".
//
// The camera is a fixture people reach for many times a day (wake, a preset,
// sleep), so the app can stay resident: closing the window hides it to the
// tray (opt-out setting), the tray menu offers the frequent actions without
// opening the window, and an XDG autostart entry (~/.config/autostart) can
// launch it hidden at login with the --tray flag.
//
// QSystemTrayIcon speaks the StatusNotifier D-Bus protocol on modern desktops
// (KDE natively, GNOME via the AppIndicator extension Ubuntu ships) and falls
// back to XEmbed. Where no tray exists at all, `available` is false, closing
// the window quits as before, and the Settings toggles say why they are off.
#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

class CameraController;
class QAction;
class QMenu;
class QSystemTrayIcon;
class QWindow;

class TrayController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    // Truth = the autostart .desktop file exists (not a persisted flag), so the
    // toggle always shows what the desktop will actually do at next login.
    Q_PROPERTY(bool startWithSystem READ startWithSystem WRITE setStartWithSystem NOTIFY startWithSystemChanged)
    Q_PROPERTY(QString autostartPath READ autostartPath CONSTANT)
    Q_PROPERTY(bool windowVisible READ windowVisible NOTIFY windowVisibleChanged)

public:
    explicit TrayController(CameraController *cam, QObject *parent = nullptr);
    ~TrayController() override;

    // Called once the QML root window exists; enables show/hide/toggle.
    void attachWindow(QWindow *window);

    bool available() const { return m_tray != nullptr; }
    bool startWithSystem() const;
    QString autostartPath() const;
    bool windowVisible() const;

public slots:
    void setStartWithSystem(bool on);
    void showWindow();
    void hideWindow();
    void toggleWindow();
    void quit();

signals:
    void startWithSystemChanged();
    void windowVisibleChanged();
    void logLine(const QString &kind, const QString &message);

private:
    void rebuildPresetMenu();
    void refreshState();          // tooltip + enabled state from the camera
    QString launchCommand() const; // what the autostart entry should Exec

    CameraController *m_cam = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_menu = nullptr;
    QMenu *m_presetMenu = nullptr;
    QAction *m_showAct = nullptr;
    QAction *m_wakeAct = nullptr;
    QAction *m_sleepAct = nullptr;
    QAction *m_statusAct = nullptr;
    QPointer<QWindow> m_window;
};
