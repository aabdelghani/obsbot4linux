// Hardware smoke test for UvcControls (issue #16). Needs an OBSBOT camera on a
// V4L2 node; NOT part of ctest. It flips white balance and exposure to manual,
// writes a value, checks the driver readback, and restores the previous state.
//
//   ./qt/build/uvc_controls_smoke            # exit 0 = all round-trips matched
//                                            #      3 = no OBSBOT node
//                                            #      1 = a readback did not match
#include "../src/UvcControls.h"

#include <QCoreApplication>
#include <QObject>

#include <cstdio>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    UvcControls uvc;
    QObject::connect(&uvc, &UvcControls::logLine, [](const QString &k, const QString &m) {
        std::fprintf(stderr, "[%s] %s\n", qPrintable(k), qPrintable(m));
    });
    uvc.setDevicePath(QString());
    if (!uvc.available()) {
        std::fprintf(stdout, "[uvc-smoke] NO DEVICE: %s\n", qPrintable(uvc.unavailableReason()));
        return 3;
    }
    std::fprintf(stdout, "[uvc-smoke] node %s  wb=%d exposure=%d backlight=%d powerline=%d\n",
                 qPrintable(uvc.devicePath()), uvc.hasWhiteBalance(), uvc.hasExposure(),
                 uvc.hasBacklight(), uvc.hasPowerLine());

    int failures = 0;
    auto check = [&](const char *what, bool cond) {
        std::fprintf(stdout, "[uvc-smoke] %-28s %s\n", what, cond ? "ok" : "MISMATCH");
        if (!cond) ++failures;
    };

    if (uvc.hasWhiteBalance()) {
        const bool wasAuto = uvc.wbAuto();
        const int wasTemp = uvc.wbTemp();
        uvc.setWbAuto(false);
        check("wb -> manual", !uvc.wbAuto());
        const int target = uvc.wbTempMin() + ((uvc.wbTempMax() - uvc.wbTempMin()) / 4 / uvc.wbTempStep()) * uvc.wbTempStep();
        uvc.setWbTemp(target);
        check("wb temp write/readback", uvc.wbTemp() == target);
        if (!wasAuto) uvc.setWbTemp(wasTemp);
        uvc.setWbAuto(wasAuto);
        check("wb restored", uvc.wbAuto() == wasAuto);
    }
    if (uvc.hasExposure()) {
        const bool wasAuto = uvc.exposureAuto();
        const int wasTime = uvc.exposureTime();
        uvc.setExposureAuto(false);
        check("exposure -> manual", !uvc.exposureAuto());
        const int target = uvc.exposureTimeMin() + (uvc.exposureTimeMax() - uvc.exposureTimeMin()) / 5;
        uvc.setExposureTime(target);
        check("shutter write/readback", uvc.exposureTime() == target);
        if (!wasAuto) uvc.setExposureTime(wasTime);
        uvc.setExposureAuto(wasAuto);
        check("exposure restored", uvc.exposureAuto() == wasAuto);
    }
    if (uvc.hasPowerLine()) {
        const int was = uvc.powerLine();
        uvc.setPowerLine(was == 1 ? 2 : 1);
        check("power line write/readback", uvc.powerLine() == (was == 1 ? 2 : 1));
        uvc.setPowerLine(was);
        check("power line restored", uvc.powerLine() == was);
    }
    std::fprintf(stdout, "[uvc-smoke] %s\n", failures ? "FAILED" : "PASSED");
    return failures ? 1 : 0;
}
