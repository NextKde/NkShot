// SPDX-License-Identifier: GPL-3.0-only
#include "CodeScanner.h"

#include <Prison/ImageScanner>
#include <Prison/ScanResult>
#include <QPainter>
#include <utility>

namespace NkShot::Imaging
{

CodeScanResult scanImageCode(const QImage &image)
{
    constexpr qint64 maximumPixels = 20'000'000;
    if (image.isNull() || qint64(image.width()) * image.height() > maximumPixels) {
        return {CodeScanResult::InvalidImage, {}, {}};
    }

    // Deep-copy even images backed by external storage. The scanner only sees
    // our private physical-pixel snapshot, never the caller's mutable buffer.
    auto snapshot = image.copy();
    if (snapshot.isNull()) {
        return {CodeScanResult::InvalidImage, {}, {}};
    }
    snapshot.setDevicePixelRatio(1);
    if (snapshot.hasAlphaChannel()) {
        QImage opaque(snapshot.size(), QImage::Format_RGB32);
        if (opaque.isNull()) {
            return {CodeScanResult::InvalidImage, {}, {}};
        }
        opaque.fill(Qt::white);
        {
            QPainter painter(&opaque);
            painter.drawImage(QPoint(), snapshot);
        }
        snapshot = std::move(opaque);
    }

    const auto result = Prison::ImageScanner::scan(snapshot);
    if (result.hasText()) {
        return {CodeScanResult::Text, result.text(), {}};
    }
    if (result.hasBinaryData()) {
        return {CodeScanResult::Binary, {}, result.binaryData()};
    }
    return {CodeScanResult::NotFound, {}, {}};
}

}
