#include "UvcControls.h"
#include "V4l2Scan.h"

#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {

int xioctl(int fd, unsigned long req, void *arg) {
    int r;
    do { r = ::ioctl(fd, req, arg); } while (r == -1 && errno == EINTR);
    return r;
}

// RAII open of the control node for one ioctl sequence.
struct Node {
    int fd = -1;
    explicit Node(const QString &path) {
        if (!path.isEmpty())
            fd = ::open(path.toLocal8Bit().constData(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
    }
    ~Node() { if (fd >= 0) ::close(fd); }
    bool ok() const { return fd >= 0; }
};

QString errnoText() { return QString::fromLocal8Bit(std::strerror(errno)); }

} // namespace

UvcControls::UvcControls(QObject *parent) : QObject(parent) {
    m_wb.id        = V4L2_CID_AUTO_WHITE_BALANCE;
    m_wbTemp.id    = V4L2_CID_WHITE_BALANCE_TEMPERATURE;
    m_expAuto.id   = V4L2_CID_EXPOSURE_AUTO;
    m_expTime.id   = V4L2_CID_EXPOSURE_ABSOLUTE;
    m_backlight.id = V4L2_CID_BACKLIGHT_COMPENSATION;
    m_powerLine.id = V4L2_CID_POWER_LINE_FREQUENCY;
}

QString UvcControls::unavailableReason() const {
    if (m_path.isEmpty())
        return QStringLiteral("No OBSBOT video device found — UVC controls need the camera's V4L2 node.");
    return QStringLiteral("This camera's UVC node exposes no white-balance/exposure controls.");
}

void UvcControls::setDevicePath(const QString &preferred) {
    const QString node = v4l2scan::pickNode(preferred);
    if (node == m_path && !node.isEmpty()) { refresh(); return; }
    m_path = node;
    queryAll();
    if (m_path.isEmpty())
        emit logLine("sys", QStringLiteral("uvc: no video node — white balance/exposure controls off"));
    else
        emit logLine("sys", QStringLiteral("uvc: controls on %1 — wb=%2 temp=%3 exposure=%4 shutter=%5 backlight=%6 powerline=%7")
                                .arg(m_path)
                                .arg(m_wb.present ? "yes" : "no", m_wbTemp.present ? "yes" : "no",
                                     m_expAuto.present ? "yes" : "no", m_expTime.present ? "yes" : "no",
                                     m_backlight.present ? "yes" : "no", m_powerLine.present ? "yes" : "no"));
    emit deviceChanged();
    emit valuesChanged();
}

void UvcControls::refresh() {
    if (m_path.isEmpty()) return;
    queryAll();
    emit deviceChanged();
    emit valuesChanged();
}

void UvcControls::queryAll() {
    m_autoExposureMenu = -1;
    for (Ctrl *c : {&m_wb, &m_wbTemp, &m_expAuto, &m_expTime, &m_backlight, &m_powerLine}) {
        c->present = false;
        query(*c);
        if (c->present) read(*c);
    }
    // Which "auto" item does auto_exposure offer? UVC cameras differ: some list
    // 0 (Auto), most list 3 (Aperture Priority) — the Tiny 3 lists both.
    if (m_expAuto.present) {
        Node n(m_path);
        for (int cand : {V4L2_EXPOSURE_AUTO, V4L2_EXPOSURE_APERTURE_PRIORITY, V4L2_EXPOSURE_SHUTTER_PRIORITY}) {
            v4l2_querymenu qm{};
            qm.id = V4L2_CID_EXPOSURE_AUTO;
            qm.index = static_cast<__u32>(cand);
            if (n.ok() && xioctl(n.fd, VIDIOC_QUERYMENU, &qm) == 0) { m_autoExposureMenu = cand; break; }
        }
    }
}

void UvcControls::query(Ctrl &c) {
    Node n(m_path);
    if (!n.ok()) return;
    v4l2_queryctrl qc{};
    qc.id = c.id;
    if (xioctl(n.fd, VIDIOC_QUERYCTRL, &qc) != 0) return;
    if (qc.flags & V4L2_CTRL_FLAG_DISABLED) return;
    c.present = true;
    c.min = qc.minimum;
    c.max = qc.maximum;
    c.step = qc.step > 0 ? qc.step : 1;
    c.def = qc.default_value;
    c.inactive = (qc.flags & V4L2_CTRL_FLAG_INACTIVE) != 0;
    if (c.value < c.min || c.value > c.max) c.value = c.def;
}

bool UvcControls::read(Ctrl &c) {
    Node n(m_path);
    if (!n.ok()) return false;
    v4l2_control vc{};
    vc.id = c.id;
    if (xioctl(n.fd, VIDIOC_G_CTRL, &vc) != 0) return false;
    // While a control is inactive (e.g. colour temperature under auto WB) the
    // Tiny 3 reports 0 — outside the range. Keep the last real value so the
    // slider does not jump to an impossible position; the UI disables it.
    if (vc.value >= c.min && vc.value <= c.max) c.value = vc.value;
    else if (c.value < c.min || c.value > c.max) c.value = c.def;
    return true;
}

bool UvcControls::write(Ctrl &c, int value, const QString &label, const QString &shown) {
    if (!c.present) {
        emit logLine("warn", label + QStringLiteral(": not available on this device"));
        return false;
    }
    if (value < c.min) value = c.min;
    if (value > c.max) value = c.max;
    emit logLine("cmd", QStringLiteral("→ %1 %2 (uvc)").arg(label, shown));
    Node n(m_path);
    if (!n.ok()) {
        emit logLine("warn", QStringLiteral("%1: cannot open %2: %3").arg(label, m_path, errnoText()));
        return false;
    }
    v4l2_control vc{};
    vc.id = c.id;
    vc.value = value;
    const int rc = xioctl(n.fd, VIDIOC_S_CTRL, &vc);
    const int err = errno;
    // Readback is the truth (the driver may clamp, or the device may refuse
    // silently) — re-query flags too, since auto toggles flip the
    // active/inactive state of their manual partner.
    read(c);
    if (rc != 0) {
        emit logLine("warn", QStringLiteral("%1 %2  FAILED: %3 (device reports %4)")
                                 .arg(label, shown, QString::fromLocal8Bit(std::strerror(err))).arg(c.value));
        return false;
    }
    emit logLine(c.value == value ? "ok" : "warn",
                 QStringLiteral("%1 %2  rc=0 (device reports %3)").arg(label, shown).arg(c.value));
    return true;
}

int UvcControls::autoExposureValue() const {
    return m_autoExposureMenu >= 0 ? m_autoExposureMenu : V4L2_EXPOSURE_APERTURE_PRIORITY;
}

void UvcControls::setWbAuto(bool on) {
    write(m_wb, on ? 1 : 0, QStringLiteral("white balance"), on ? QStringLiteral("auto") : QStringLiteral("manual"));
    if (!on && m_wbTemp.present) {
        // Leaving auto: the device keeps whatever temperature it last had (the
        // Tiny 3 reports 0 while auto) — push the slider's value so the UI and
        // the camera agree from the first frame.
        write(m_wbTemp, m_wbTemp.value, QStringLiteral("color temp"), QStringLiteral("%1 K").arg(m_wbTemp.value));
    }
    refresh();
}

void UvcControls::setWbTemp(int kelvin) {
    if (m_wb.present && m_wb.value != 0) {
        emit logLine("warn", QStringLiteral("color temp: white balance is on auto — switch to manual first"));
        return;
    }
    const int step = wbTempStep();
    kelvin = m_wbTemp.min + ((kelvin - m_wbTemp.min) / step) * step;   // snap to the driver's step
    write(m_wbTemp, kelvin, QStringLiteral("color temp"), QStringLiteral("%1 K").arg(kelvin));
    emit valuesChanged();
}

void UvcControls::setExposureAuto(bool on) {
    const int v = on ? autoExposureValue() : V4L2_EXPOSURE_MANUAL;
    write(m_expAuto, v, QStringLiteral("exposure"), on ? QStringLiteral("auto") : QStringLiteral("manual"));
    if (!on && m_expTime.present)
        write(m_expTime, m_expTime.value, QStringLiteral("shutter"), QStringLiteral("%1").arg(m_expTime.value));
    refresh();
}

void UvcControls::setExposureTime(int units) {
    if (m_expAuto.present && m_expAuto.value != V4L2_EXPOSURE_MANUAL) {
        emit logLine("warn", QStringLiteral("shutter: exposure is on auto — switch to manual first"));
        return;
    }
    // UVC Exposure Time Absolute is in 100 µs units → 1/x s for the log.
    const QString shown = units > 0 ? QStringLiteral("1/%1 s").arg(qRound(10000.0 / units)) : QString::number(units);
    write(m_expTime, units, QStringLiteral("shutter"), shown);
    emit valuesChanged();
}

void UvcControls::setBacklight(int value) {
    write(m_backlight, value, QStringLiteral("backlight comp"), QString::number(value));
    emit valuesChanged();
}

void UvcControls::setPowerLine(int index) {
    static const char *names[] = {"off", "50 Hz", "60 Hz"};
    const QString shown = (index >= 0 && index < 3) ? QString::fromLatin1(names[index]) : QString::number(index);
    write(m_powerLine, index, QStringLiteral("power line"), shown);
    emit valuesChanged();
}
