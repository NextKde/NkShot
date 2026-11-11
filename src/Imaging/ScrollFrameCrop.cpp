// SPDX-License-Identifier: GPL-3.0-only
#include "ScrollFrameCrop.h"
#include <QtMath>
namespace NkShot::Imaging {
QImage cropScrollFrame(const QImage &screen, QSize expectedPixels, QRect localPixels, qreal expectedDpr)
{
    if (screen.isNull() || screen.size() != expectedPixels || !qIsFinite(expectedDpr) || expectedDpr <= 0
        || screen.devicePixelRatio() != expectedDpr || !localPixels.isValid()
        || !screen.rect().contains(localPixels) || localPixels.width() > 8192 || localPixels.height() > 32000
        || qint64(localPixels.width()) * localPixels.height() > 40000000) return {};
    return screen.copy(localPixels);
}
}
