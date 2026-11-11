// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QImage>
#include <QRect>
namespace NkShot::Imaging {
// Coordinates are physical pixels local to one output, not global desktop DIP.
// Never scale/pad mismatched frames: changing content coordinates must be rejected.
QImage cropScrollFrame(const QImage &screen, QSize expectedPixels, QRect localPixels, qreal expectedDpr);
}
