// SPDX-License-Identifier: GPL-3.0-only
#include "ColorSample.h"

#include <QColorSpace>

#include <algorithm>
#include <cmath>

namespace NkShot::Imaging
{
QImage colorSampleSnapshot(const QImage &source)
{
    constexpr qint64 maximumPixels = 20'000'000;
    if (source.isNull() || qint64(source.width()) * source.height() > maximumPixels) {
        return {};
    }

    const QColorSpace srgb(QColorSpace::SRgb);
    QImage normalized;
    if (source.colorSpace().isValid()) {
        normalized = source.convertedToColorSpace(srgb, QImage::Format_ARGB32);
    } else {
        normalized = source.convertToFormat(QImage::Format_ARGB32);
    }
    // Format conversion can return a shared image when no conversion is needed.
    // A copy is essential for images backed by externally owned mutable memory.
    QImage snapshot = normalized.copy();
    if (!snapshot.isNull()) {
        snapshot.setColorSpace(srgb);
        snapshot.setDevicePixelRatio(1.0);
    }
    return snapshot;
}

QPoint colorSamplePixelAt(QPointF position, QSizeF viewport, QSize imageSize)
{
    const QPoint invalid(-1, -1);
    if (!std::isfinite(position.x()) || !std::isfinite(position.y())
        || !std::isfinite(viewport.width()) || !std::isfinite(viewport.height())
        || viewport.width() <= 0 || viewport.height() <= 0
        || imageSize.width() <= 0 || imageSize.height() <= 0) {
        return invalid;
    }

    // Wide intermediates avoid overflowing or underflowing on finite inputs.
    const long double scale = std::min(static_cast<long double>(viewport.width()) / imageSize.width(),
                                       static_cast<long double>(viewport.height()) / imageSize.height());
    const long double width = imageSize.width() * scale;
    const long double height = imageSize.height() * scale;
    const long double left = (static_cast<long double>(viewport.width()) - width) / 2;
    const long double top = (static_cast<long double>(viewport.height()) - height) / 2;
    const long double x = static_cast<long double>(position.x()) - left;
    const long double y = static_cast<long double>(position.y()) - top;
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return invalid;
    }
    const int pixelX = static_cast<int>(std::floor(x / scale));
    const int pixelY = static_cast<int>(std::floor(y / scale));
    if (pixelX >= imageSize.width() || pixelY >= imageSize.height()) {
        return invalid;
    }
    return {pixelX, pixelY};
}

QImage colorSampleNeighborhood(const QImage &image, QPoint pixel)
{
    if (image.isNull() || !image.rect().contains(pixel)) {
        return {};
    }
    constexpr int radius = 5;
    QImage patch(2 * radius + 1, 2 * radius + 1, QImage::Format_ARGB32);
    patch.fill(Qt::transparent);
    patch.setColorSpace(image.colorSpace());
    for (int y = 0; y < patch.height(); ++y) {
        for (int x = 0; x < patch.width(); ++x) {
            const QPoint sourcePixel = pixel + QPoint(x - radius, y - radius);
            if (image.rect().contains(sourcePixel)) {
                patch.setPixelColor(x, y, image.pixelColor(sourcePixel));
            }
        }
    }
    return patch;
}

QString colorSampleHex(const QColor &color)
{
    if (!color.isValid()) {
        return {};
    }
    const QString rgb = color.name(QColor::HexRgb).toUpper();
    if (color.alpha() == 255) {
        return rgb;
    }
    return rgb + QStringLiteral("%1").arg(color.alpha(), 2, 16, QLatin1Char('0')).toUpper();
}

QString colorSampleRgb(const QColor &color)
{
    if (!color.isValid()) {
        return {};
    }
    const QString channels = QStringLiteral("%1, %2, %3").arg(color.red()).arg(color.green()).arg(color.blue());
    if (color.alpha() == 255) {
        return QStringLiteral("rgb(%1)").arg(channels);
    }
    QString alpha = QString::number(color.alpha() / 255.0, 'f', 3);
    while (alpha.endsWith(QLatin1Char('0'))) {
        alpha.chop(1);
    }
    if (alpha.endsWith(QLatin1Char('.'))) {
        alpha.chop(1);
    }
    return QStringLiteral("rgba(%1, %2)").arg(channels, alpha);
}
}
