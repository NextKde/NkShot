// SPDX-License-Identifier: GPL-3.0-only
#include "ImageTransforms.h"
#include <QtMath>
#include <QTransform>
namespace NkShot::Imaging {
QImage orientedCopy(const QImage &source, int quarterTurns, bool mirror, int width, qint64 maximumPixels)
{
    if (source.isNull() || quarterTurns < 0 || quarterTurns > 3 || maximumPixels < 1
        || width < 1 || width > 8192) return {};
    if (quarterTurns == 0 && !mirror) return resizedToWidth(source, width, maximumPixels);
    if (qint64(source.width()) * source.height() > maximumPixels) return {};
    const auto size = quarterTurns % 2 ? source.size().transposed() : source.size();
    const qint64 height = qMax<qint64>(1, qRound64(double(size.height()) * width / size.width()));
    if (height > 32000 || width * height > maximumPixels) return {};
    auto result = quarterTurns ? source.transformed(QTransform().rotate(quarterTurns * 90), Qt::FastTransformation) : source;
    if (mirror) result = result.flipped(Qt::Horizontal);
    return resizedToWidth(result, width, maximumPixels);
}
QImage resizedToWidth(const QImage &source, int width, qint64 maximumPixels)
{
    if (source.isNull() || width < 1 || width > 8192 || maximumPixels < 1) return {};
    const qint64 height = qMax<qint64>(1, qRound64(double(source.height()) * width / source.width()));
    if (height > 32000 || height * width > maximumPixels) return {};
    return source.scaled(width, int(height), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}
}

#include <QPainter>
#include <QFont>
#include <QPainterPath>
#include <QColorSpace>
#include <QLinearGradient>
namespace NkShot::Imaging {
QImage composePresentation(const QImage &source, const PresentationOptions &options, qint64 maximumPixels)
{
    const auto style = int(options.style);
    if (source.isNull() || options.padding < 0 || options.padding > 240 || style < 0 || style > 2
        || options.watermark.size() > 120 || maximumPixels < 1) return {};
    const auto watermark = options.watermark.simplified();
    const int footer = watermark.isEmpty() ? 0 : 36;
    const qint64 width = qint64(source.width()) + options.padding * 2;
    const qint64 height = qint64(source.height()) + options.padding * 2 + footer;
    if (width > 8192 || height > 32000 || width * height > maximumPixels) return {};
    QImage image(int(width), int(height), QImage::Format_ARGB32_Premultiplied);
    if (image.isNull()) return {};
    image.setColorSpace(QColorSpace::SRgb);
    QImage content = source.colorSpace().isValid() ? source.convertedToColorSpace(QColorSpace::SRgb) : source;
    if (content.isNull()) return {};
    content.setDevicePixelRatio(1); // Paint explicitly in physical pixels.
    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        QLinearGradient gradient(0, 0, width, height);
        const bool dark = options.style == PresentationStyle::Dark;
        gradient.setColorAt(0, QColor(dark ? "#202126" : style == 2 ? "#c7dcff" : "#f5f5f7"));
        gradient.setColorAt(1, QColor(dark ? "#414553" : style == 2 ? "#ead5ff" : "#e9edf4"));
        painter.fillRect(image.rect(), gradient);
        const QRectF rect(options.padding, options.padding, source.width(), source.height());
        const qreal radius = options.rounded ? qMin(16, qMin(source.width(), source.height()) / 4) : 0;
        if (options.padding >= 16) {
            for (int spread = 12; spread >= 1; --spread) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(0, 0, 0, 3));
                painter.drawRoundedRect(rect.translated(0, 5).adjusted(-spread, -spread, spread, spread), radius + spread, radius + spread);
            }
        }
        QPainterPath clip; clip.addRoundedRect(rect, radius, radius);
        painter.save(); painter.setClipPath(clip);
        painter.drawImage(rect, content, content.rect());
        painter.restore();
        if (!watermark.isEmpty()) {
            QFont font; font.setPixelSize(16); painter.setFont(font);
            painter.setPen(QColor(dark ? "#f0f1f5" : "#343744"));
            const int inset = qMax(12, options.padding);
            const QRect label(inset, source.height() + options.padding * 2, qMax(1, int(width) - inset * 2), footer);
            painter.drawText(label, Qt::AlignRight | Qt::AlignVCenter,
                painter.fontMetrics().elidedText(watermark, Qt::ElideRight, label.width()));
        }
    }
    image.setDevicePixelRatio(source.devicePixelRatio());
    return image;
}
}
