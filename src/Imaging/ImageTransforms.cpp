// SPDX-License-Identifier: GPL-3.0-only
#include "ImageTransforms.h"
#include <QtMath>
namespace NkShot::Imaging {
QImage resizedToWidth(const QImage &source, int width, qint64 maximumPixels)
{
    if (source.isNull() || width < 1 || width > 8192 || maximumPixels < 1) return {};
    const qint64 height = qMax<qint64>(1, qRound64(double(source.height()) * width / source.width()));
    if (height > 32000 || height * width > maximumPixels) return {};
    return source.scaled(width, int(height), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}
}
