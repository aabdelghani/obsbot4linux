// UvcControls — white balance, exposure and related image controls through
// the STANDARD UVC/V4L2 control interface (issue #16).
//
// The OBSBOT SDK's white-balance/exposure calls are documented for other
// models and unverified on the Tiny 3, so the app kept those controls gated
// off. The camera does, however, expose them as ordinary UVC Processing-Unit /
// Camera-Terminal controls that the uvcvideo driver publishes as V4L2 controls
// (white_balance_automatic, white_balance_temperature, auto_exposure,
// exposure_time_absolute, …) — hardware-verified with v4l2-ctl on a Tiny 3.
// This backend drives exactly those, with the same VIDIOC_S_CTRL/G_CTRL ioctls
// v4l2-ctl and guvcview use, and bypasses the SDK entirely.
//
// Honesty rules, same as the rest of the app:
//   * A control is offered only if VIDIOC_QUERYCTRL says the device has it;
//     ranges come from the driver, never from a hardcoded table.
//   * Every write is followed by a readback; the readback is what the UI shows
//     and what the activity log reports.
//   * The node is opened per call (open → ioctl → close): nothing is held open,
//     so a hot-unplug cannot leave a dead fd behind and the preview/other apps
//     are never blocked (UVC control ioctls do not take the streaming lock).
#pragma once

#include <QObject>
#include <QString>

#include <linux/videodev2.h>

class UvcControls : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString devicePath READ devicePath NOTIFY deviceChanged)
    Q_PROPERTY(bool available READ available NOTIFY deviceChanged)
    Q_PROPERTY(QString unavailableReason READ unavailableReason NOTIFY deviceChanged)

    // White balance (Processing Unit): auto on/off + colour temperature (K).
    Q_PROPERTY(bool hasWhiteBalance READ hasWhiteBalance NOTIFY deviceChanged)
    Q_PROPERTY(bool wbAuto READ wbAuto NOTIFY valuesChanged)
    Q_PROPERTY(int wbTemp READ wbTemp NOTIFY valuesChanged)
    Q_PROPERTY(int wbTempMin READ wbTempMin NOTIFY deviceChanged)
    Q_PROPERTY(int wbTempMax READ wbTempMax NOTIFY deviceChanged)
    Q_PROPERTY(int wbTempStep READ wbTempStep NOTIFY deviceChanged)

    // Exposure (Camera Terminal): auto/manual + shutter time in 100 µs units.
    Q_PROPERTY(bool hasExposure READ hasExposure NOTIFY deviceChanged)
    Q_PROPERTY(bool exposureAuto READ exposureAuto NOTIFY valuesChanged)
    Q_PROPERTY(int exposureTime READ exposureTime NOTIFY valuesChanged)
    Q_PROPERTY(int exposureTimeMin READ exposureTimeMin NOTIFY deviceChanged)
    Q_PROPERTY(int exposureTimeMax READ exposureTimeMax NOTIFY deviceChanged)

    // Backlight compensation and power-line (anti-flicker) frequency.
    Q_PROPERTY(bool hasBacklight READ hasBacklight NOTIFY deviceChanged)
    Q_PROPERTY(int backlight READ backlight NOTIFY valuesChanged)
    Q_PROPERTY(int backlightMin READ backlightMin NOTIFY deviceChanged)
    Q_PROPERTY(int backlightMax READ backlightMax NOTIFY deviceChanged)
    Q_PROPERTY(bool hasPowerLine READ hasPowerLine NOTIFY deviceChanged)
    Q_PROPERTY(int powerLine READ powerLine NOTIFY valuesChanged)   // 0=off, 1=50 Hz, 2=60 Hz

public:
    explicit UvcControls(QObject *parent = nullptr);

    QString devicePath() const { return m_path; }
    bool available() const { return !m_path.isEmpty() && (m_wb.present || m_expAuto.present); }
    QString unavailableReason() const;

    bool hasWhiteBalance() const { return m_wb.present && m_wbTemp.present; }
    bool wbAuto() const { return m_wb.value != 0; }
    int wbTemp() const { return m_wbTemp.value; }
    int wbTempMin() const { return m_wbTemp.min; }
    int wbTempMax() const { return m_wbTemp.max; }
    int wbTempStep() const { return m_wbTemp.step > 0 ? m_wbTemp.step : 1; }

    bool hasExposure() const { return m_expAuto.present && m_expTime.present; }
    bool exposureAuto() const { return m_expAuto.value != V4L2_EXPOSURE_MANUAL; }
    int exposureTime() const { return m_expTime.value; }
    int exposureTimeMin() const { return m_expTime.min; }
    int exposureTimeMax() const { return m_expTime.max; }

    bool hasBacklight() const { return m_backlight.present; }
    int backlight() const { return m_backlight.value; }
    int backlightMin() const { return m_backlight.min; }
    int backlightMax() const { return m_backlight.max; }
    bool hasPowerLine() const { return m_powerLine.present; }
    int powerLine() const { return m_powerLine.value; }

public slots:
    // Bind to a video node. `preferred` is normally the SDK's videoDevPath()
    // for the connected camera; empty falls back to the first OBSBOT capture
    // node, and an empty result means "no controls" (honestly disabled UI).
    void setDevicePath(const QString &preferred);
    void refresh();                    // re-query ranges + re-read values
    void setWbAuto(bool on);
    void setWbTemp(int kelvin);
    void setExposureAuto(bool on);
    void setExposureTime(int units);   // 100 µs units (UVC Exposure Time Absolute)
    void setBacklight(int value);
    void setPowerLine(int index);      // 0=off, 1=50 Hz, 2=60 Hz

signals:
    void deviceChanged();
    void valuesChanged();
    void logLine(const QString &kind, const QString &message);

private:
    struct Ctrl {
        __u32 id = 0;
        bool present = false;
        int min = 0, max = 0, step = 1, def = 0;
        int value = 0;
        bool inactive = false;   // V4L2_CTRL_FLAG_INACTIVE (e.g. temp while auto WB)
    };
    void queryAll();
    void query(Ctrl &c);
    bool read(Ctrl &c);
    // S_CTRL + G_CTRL readback; logs "<label> = <readback> …  rc=…" honestly.
    bool write(Ctrl &c, int value, const QString &label, const QString &shown);
    int autoExposureValue() const;   // the "auto" menu item this device supports

    QString m_path;
    Ctrl m_wb, m_wbTemp, m_expAuto, m_expTime, m_backlight, m_powerLine;
    int m_autoExposureMenu = -1;     // resolved from VIDIOC_QUERYMENU
};
