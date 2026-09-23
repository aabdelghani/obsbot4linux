#include "V4l2Scan.h"

#include <QDir>

#include <algorithm>
#include <cerrno>

#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {

int xioctl(int fd, unsigned long req, void *arg) {
    int r;
    do { r = ::ioctl(fd, req, arg); } while (r == -1 && errno == EINTR);
    return r;
}

// Query a node; returns true if it is a capture node and fills `card`.
bool queryCapture(const QString &path, QString *card) {
    const int fd = ::open(path.toLocal8Bit().constData(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return false;
    v4l2_capability cap{};
    const bool ok = (xioctl(fd, VIDIOC_QUERYCAP, &cap) == 0);
    ::close(fd);
    if (!ok) return false;
    const __u32 caps = (cap.capabilities & V4L2_CAP_DEVICE_CAPS) ? cap.device_caps
                                                                 : cap.capabilities;
    if (!(caps & V4L2_CAP_VIDEO_CAPTURE)) return false;
    if (card) *card = QString::fromLatin1(reinterpret_cast<const char *>(cap.card));
    return true;
}

} // namespace

namespace v4l2scan {

QStringList obsbotCaptureNodes() {
    QStringList nodes = QDir(QStringLiteral("/dev"))
                            .entryList({QStringLiteral("video*")}, QDir::System | QDir::Files);
    std::sort(nodes.begin(), nodes.end(), [](const QString &a, const QString &b) {
        return a.mid(5).toInt() < b.mid(5).toInt();
    });
    QStringList out;
    for (const QString &n : nodes) {
        const QString path = QStringLiteral("/dev/") + n;
        QString card;
        if (!queryCapture(path, &card)) continue;
        if (!card.contains(QLatin1String("OBSBOT"), Qt::CaseInsensitive)) continue;
        out << path;
    }
    return out;
}

bool isCaptureNode(const QString &path) {
    return !path.isEmpty() && queryCapture(path, nullptr);
}

QString pickNode(const QString &preferred) {
    if (isCaptureNode(preferred)) return preferred;
    const QStringList all = obsbotCaptureNodes();
    return all.isEmpty() ? QString() : all.first();
}

} // namespace v4l2scan
