// V4l2Scan — locate the OBSBOT camera's V4L2 capture node.
//
// Shared by the embedded preview (PreviewEngine) and the UVC control backend
// (UvcControls). The node is found by the driver-reported card name
// (VIDIOC_QUERYCAP), never by a hardcoded /dev/video0. When the SDK reports a
// path for the bound device (Device::videoDevPath) that path is preferred, so
// with several OBSBOT cameras attached (issue #14) preview and UVC controls
// follow the camera that is actually selected.
#pragma once

#include <QString>
#include <QStringList>

namespace v4l2scan {

// All capture-capable nodes whose card name contains "OBSBOT", in numeric
// /dev/videoN order (video2 before video10).
QStringList obsbotCaptureNodes();

// True if `path` is a capture-capable V4L2 node (any card name).
bool isCaptureNode(const QString &path);

// The node to use: `preferred` when it is a valid capture node, else the first
// OBSBOT capture node, else empty.
QString pickNode(const QString &preferred);

} // namespace v4l2scan
