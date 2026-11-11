// SPDX-License-Identifier: GPL-3.0-only
#include "ImageAdjustments.h"
#include <QColorSpace>
#include <algorithm>
#include <cmath>

namespace NkShot::Imaging {
QImage adjustedCopy(const QImage &source, const AdjustmentOptions &a)
{
    if (source.isNull() || source.width() > 8192 || source.height() > 32000
        || qint64(source.width()) * source.height() > 40000000
        || a.brightness < -100 || a.brightness > 100
        || a.contrast < -100 || a.contrast > 100
        || a.saturation < -100 || a.saturation > 100) return {};
    if (!a.brightness && !a.contrast && !a.saturation) return source.copy();
    auto normalized = source.colorSpace().isValid() ? source.convertedToColorSpace(QColorSpace::SRgb) : source;
    if (normalized.isNull()) return {};
    // Always detach even if the input already has this format/color space.
    QImage output = normalized.convertToFormat(QImage::Format_RGBA8888).copy();
    if (output.isNull()) return {};
    output.setColorSpace(QColorSpace::SRgb);
    output.setDevicePixelRatio(source.devicePixelRatio());
    const double offset = a.brightness / 100.0;
    const double contrast = 1.0 + a.contrast / 100.0;
    const double saturation = 1.0 + a.saturation / 100.0;
    const auto clamp = [](double c) { return std::clamp(c, 0.0, 1.0); };
    const auto byte = [&clamp](double c) { return static_cast<uchar>(std::lround(clamp(c) * 255.0 + 1e-9)); };
    for (int y = 0; y < output.height(); ++y) {
        auto row = output.scanLine(y);
        for (int x = 0; x < output.width(); ++x) {
            auto p = row + x * 4;
            if (!p[3]) { p[0] = p[1] = p[2] = 0; continue; }
            const double r = clamp((p[0] / 255.0 + offset - .5) * contrast + .5);
            const double g = clamp((p[1] / 255.0 + offset - .5) * contrast + .5);
            const double b = clamp((p[2] / 255.0 + offset - .5) * contrast + .5);
            const double grey = .2126 * r + .7152 * g + .0722 * b;
            p[0] = byte(grey + (r - grey) * saturation);
            p[1] = byte(grey + (g - grey) * saturation);
            p[2] = byte(grey + (b - grey) * saturation);
        }
    }
    return output;
}
}
