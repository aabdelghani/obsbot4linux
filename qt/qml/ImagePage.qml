// Image & Exposure.
//   * Brightness / Contrast / Saturation / Sharpness — WIRED (SDK 0–100, live
//     values read on connect, applied on release).
//   * HDR — NOT available on Tiny 3 (SDK HDR/WDR is for tiny4k/tiny2/meet/tail-
//     air; Tiny 3 reports hdr_support=0 in every mode). Shown as an honest note.
//   * White balance / Color temp / Exposure / Backlight / Power line — WIRED
//     through the STANDARD UVC controls (context property `uvc`, issue #16),
//     not the OBSBOT SDK: the Tiny 3 exposes them as ordinary V4L2 controls
//     (hardware-verified). Ranges and values come from the driver; every write
//     is read back. The section is disabled (with the reason) only when the
//     camera's node reports no such controls.
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import Obsbot

RowLayout {
    id: root
    spacing: Theme.s4

    // A live, coral-styled slider. Applies to the device on release (few SDK
    // calls), and reflects the device's current value via `boundValue`.
    component ImageSlider: RowLayout {
        id: row
        property string label: ""
        property string param: ""        // SDK image param (brightness/…); empty = custom apply()
        property int boundValue: 50
        property real from: 0
        property real to: 100
        property real stepSize: 1
        property bool active: cam.connected
        // Value → text for the pill (default: the plain number).
        property var format: function (v) { return Math.round(v) }
        // Emitted on release when `param` is empty (UVC controls route here).
        signal apply(int value)
        spacing: 10
        Text { text: row.label; color: Theme.fg; font.family: Theme.mono; font.pixelSize: 13; Layout.preferredWidth: 96 }
        Slider {
            id: sl
            Layout.fillWidth: true
            enabled: row.active
            from: row.from; to: row.to; stepSize: row.stepSize
            value: row.boundValue
            onPressedChanged: {
                if (pressed) return
                const v = Math.round(value)
                if (row.param !== "") cam.setImageParam(row.param, v)
                else row.apply(v)
            }
            opacity: enabled ? 1 : 0.4

            background: Rectangle {
                x: sl.leftPadding
                y: sl.topPadding + sl.availableHeight / 2 - height / 2
                width: sl.availableWidth; height: 5; radius: 2.5
                color: Qt.rgba(1, 1, 1, 0.12)
                Rectangle {
                    width: sl.position * parent.width; height: parent.height; radius: 2.5
                    color: Theme.accent
                }
            }
            handle: Rectangle {
                x: sl.leftPadding + sl.position * (sl.availableWidth - width)
                y: sl.topPadding + sl.availableHeight / 2 - height / 2
                width: 18; height: 18; radius: 9
                color: sl.pressed ? Theme.accentDeep : Theme.accentSoft
                border.width: 1; border.color: Theme.accentDeep
            }
        }
        // Value pill — prominent, coral when dragging, tabular so it doesn't jitter.
        Rectangle {
            Layout.preferredWidth: Math.max(40, pill.implicitWidth + 12); Layout.preferredHeight: 24
            radius: Theme.rControl
            color: sl.pressed ? Theme.accentTint : Qt.rgba(1, 1, 1, 0.04)
            border.width: 1; border.color: sl.pressed ? Theme.accentRing : Theme.border
            Text {
                id: pill
                anchors.centerIn: parent
                text: row.format(sl.value)
                color: sl.pressed ? Theme.accentSoft : Theme.fg
                font.family: Theme.mono; font.pixelSize: 13
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.maximumWidth: 560
        Layout.alignment: Qt.AlignTop
        spacing: Theme.s3

        GlassPanel {
            Layout.fillWidth: true
            implicitHeight: col.implicitHeight + 28
            ColumnLayout {
                id: col
                anchors.fill: parent
                anchors.margins: 14
                spacing: Theme.s3
                RowLayout {
                    Layout.fillWidth: true
                    SectionLabel { text: "Picture" }
                    Item { Layout.fillWidth: true }
                    ActionButton {
                        text: "Reset defaults"; variant: "secondary"
                        enabled: cam.connected
                        onClicked: cam.resetImageDefaults()
                    }
                }
                ImageSlider { label: "Brightness"; param: "brightness"; boundValue: cam.brightness }
                ImageSlider { label: "Contrast";   param: "contrast";   boundValue: cam.contrast }
                ImageSlider { label: "Saturation"; param: "saturation"; boundValue: cam.saturation }
                ImageSlider { label: "Sharpness";  param: "sharpness";  boundValue: cam.sharpness }

                Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

                // HDR — the SDK's HDR/WDR control is documented for tiny4k/tiny2/
                // meet/tail-air, NOT tiny3, and the Tiny 3 reports hdr_support=0
                // in every mode (incl. 1080p30). So HDR is not exposed on Tiny 3
                // via this SDK; shown as an honest note rather than a dead toggle.
                // (Tip: 1080p30 already looks crisper than 1080p60 simply because
                //  30fps compresses less — that's framerate, not HDR.)
                RowLayout {
                    spacing: 10
                    Text { text: "HDR"; color: Theme.dim; font.family: Theme.mono; font.pixelSize: 12; Layout.preferredWidth: 96 }
                    Text {
                        text: "not available on the Tiny 3 (not exposed by the SDK)"
                        color: Theme.dimmer; font.family: Theme.mono; font.pixelSize: 11
                        Layout.fillWidth: true; wrapMode: Text.WordWrap
                    }
                }
            }
        }

        // White balance / exposure via the standard UVC controls (issue #16).
        // Disabled with the reason ONLY when the camera's V4L2 node has none.
        Rectangle {
            visible: !uvc.available
            Layout.fillWidth: true
            radius: Theme.rControl
            color: Qt.rgba(Theme.degraded.r, Theme.degraded.g, Theme.degraded.b, 0.10)
            border.width: 1
            border.color: Qt.rgba(Theme.degraded.r, Theme.degraded.g, Theme.degraded.b, 0.4)
            implicitHeight: banner.implicitHeight + 20
            Text {
                id: banner
                anchors.fill: parent; anchors.margins: 10
                text: "White balance and exposure are disabled — " + uvc.unavailableReason
                color: Theme.degraded
                font.family: Theme.mono; font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
        }
        GlassPanel {
            Layout.fillWidth: true
            implicitHeight: gcol.implicitHeight + 28
            // Re-read the driver whenever this page comes on screen: another
            // app (guvcview, v4l2-ctl, a browser) may have changed these since.
            onVisibleChanged: if (visible && uvc.available) uvc.refresh()
            ColumnLayout {
                id: gcol
                anchors.fill: parent
                anchors.margins: 14
                spacing: Theme.s3
                enabled: uvc.available
                opacity: enabled ? 1 : 0.45
                RowLayout {
                    Layout.fillWidth: true
                    SectionLabel { text: "White balance & exposure" }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: "standard UVC controls · " + (uvc.devicePath !== "" ? uvc.devicePath : "no node")
                        color: Theme.dimmer; font.family: Theme.mono; font.pixelSize: 11
                    }
                }
                RowLayout {
                    spacing: 10
                    visible: uvc.hasWhiteBalance
                    Text { text: "White balance"; color: Theme.fg; font.family: Theme.mono; font.pixelSize: 13; Layout.preferredWidth: 96 }
                    Segmented {
                        options: ["Auto", "Manual"]
                        currentIndex: uvc.wbAuto ? 0 : 1
                        onActivated: (i) => uvc.setWbAuto(i === 0)
                    }
                }
                ImageSlider {
                    visible: uvc.hasWhiteBalance
                    label: "Color temp"
                    active: uvc.available && !uvc.wbAuto
                    from: uvc.wbTempMin; to: uvc.wbTempMax; stepSize: uvc.wbTempStep
                    boundValue: uvc.wbTemp
                    format: function (v) { return Math.round(v) + " K" }
                    onApply: (v) => uvc.setWbTemp(v)
                }
                RowLayout {
                    spacing: 10
                    visible: uvc.hasExposure
                    Text { text: "Exposure"; color: Theme.fg; font.family: Theme.mono; font.pixelSize: 13; Layout.preferredWidth: 96 }
                    Segmented {
                        options: ["Auto", "Manual"]
                        currentIndex: uvc.exposureAuto ? 0 : 1
                        onActivated: (i) => uvc.setExposureAuto(i === 0)
                    }
                }
                // UVC exposure time is in 100 µs units; shown as a shutter fraction.
                ImageSlider {
                    visible: uvc.hasExposure
                    label: "Shutter"
                    active: uvc.available && !uvc.exposureAuto
                    from: uvc.exposureTimeMin; to: uvc.exposureTimeMax; stepSize: 1
                    boundValue: uvc.exposureTime
                    format: function (v) { return v > 0 ? "1/" + Math.round(10000 / v) : "—" }
                    onApply: (v) => uvc.setExposureTime(v)
                }
                ImageSlider {
                    visible: uvc.hasBacklight
                    label: "Backlight"
                    active: uvc.available
                    from: uvc.backlightMin; to: uvc.backlightMax; stepSize: 1
                    boundValue: uvc.backlight
                    onApply: (v) => uvc.setBacklight(v)
                }
                RowLayout {
                    spacing: 10
                    visible: uvc.hasPowerLine
                    Text { text: "Anti-flicker"; color: Theme.fg; font.family: Theme.mono; font.pixelSize: 13; Layout.preferredWidth: 96 }
                    Segmented {
                        options: ["Off", "50 Hz", "60 Hz"]
                        currentIndex: uvc.powerLine
                        onActivated: (i) => uvc.setPowerLine(i)
                    }
                }
                Text {
                    text: "These go through the camera's standard UVC controls (the same ones v4l2-ctl and guvcview use), "
                        + "not the OBSBOT SDK, which does not document them for the Tiny 3. Every value shown is the driver's readback."
                    color: Theme.dimmer; font.family: Theme.sans; font.pixelSize: 12
                    wrapMode: Text.WordWrap; Layout.fillWidth: true
                }
            }
        }
    }

    // reference preview — fixed size, roomy enough to judge image tweaks by
    // (300x220 was too tiny; fill-the-page swallowed the controls — don't).
    GlassPanel {
        Layout.preferredWidth: 460
        Layout.maximumWidth: 480
        Layout.preferredHeight: 310
        Layout.alignment: Qt.AlignTop
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 8
            SectionLabel { text: "Reference" }
            Viewfinder { Layout.fillWidth: true; Layout.fillHeight: true }
        }
    }
}
