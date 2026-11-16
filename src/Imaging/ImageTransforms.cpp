// SPDX-License-Identifier: GPL-3.0-only
#include "ImageTransforms.h"
#include "DeviceFrame.h"
#include <QTransform>
#include <QtMath>
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
static qreal frameScale(int width)
{
    return qMin(2.0, width / 800.0);
}
static int frameHeaderHeight(int width, PresentationFrame frame)
{
    if (frame != PresentationFrame::MacOS && frame != PresentationFrame::Browser)
        return 0;
    return qMax(1, qRound((frame == PresentationFrame::MacOS ? 42 : 52) * frameScale(width)));
}
QSize presentationSize(const QSize &source, const PresentationOptions &options, qint64 maximumPixels)
{
    const auto style = int(options.style);
    const auto frame = int(options.frame);
    if (source.isEmpty() || options.padding < 0 || options.padding > 240 || style < 0 || style > 2 || frame < 0 || frame > int(PresentationFrame::VisionPro)
        || options.frameTitle.size() > 120 || options.watermark.size() > 120 || maximumPixels < 1)
        return {};
    const int footer = options.watermark.simplified().isEmpty() ? 0 : 36;
    const auto device = deviceFrameSize(source, options.frame);
    if (!device.isEmpty() && (device.width() > 8192 || device.height() > 32000))
        return {};
    const qint64 width = (device.isEmpty() ? qint64(source.width()) : qint64(qCeil(device.width()))) + options.padding * 2;
    const qint64 height = (device.isEmpty() ? qint64(source.height()) + frameHeaderHeight(source.width(), options.frame) : qint64(qCeil(device.height())))
        + options.padding * 2 + footer;
    if (width > 8192 || height > 32000 || width * height > maximumPixels)
        return {};
    return QSize(int(width), int(height));
}
static void drawFrameHeader(QPainter &painter, const QRectF &header, const PresentationOptions &options)
{
    const bool dark = options.style == PresentationStyle::Dark;
    const qreal scale = frameScale(qRound(header.width()));
    painter.save();
    painter.translate(header.topLeft());
    painter.scale(scale, scale);
    const qreal width = header.width() / scale;
    const qreal height = header.height() / scale;
    QLinearGradient chrome(0, 0, 0, height);
    chrome.setColorAt(0, QColor(dark ? "#34363d" : "#fafafa"));
    chrome.setColorAt(1, QColor(dark ? "#292b31" : "#ececf0"));
    painter.fillRect(QRectF(0, 0, width, height), chrome);
    painter.setPen(Qt::NoPen);
    const QColor colors[] = {QColor("#ff5f57"), QColor("#febc2e"), QColor("#28c840")};
    for (int i = 0; i < 3; ++i) {
        painter.setBrush(colors[i]);
        painter.drawEllipse(QPointF(20 + i * 20, height / 2), 5.5, 5.5);
    }
    QRectF label(92, 0, width - 184, height);
    if (options.frame == PresentationFrame::Browser) {
        painter.setPen(QPen(QColor(dark ? "#b4b6c1" : "#767985"), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        const qreal mid = height / 2;
        painter.drawPolyline(QPolygonF{QPointF(92, mid - 4), QPointF(88, mid), QPointF(92, mid + 4)});
        painter.drawPolyline(QPolygonF{QPointF(106, mid - 4), QPointF(110, mid), QPointF(106, mid + 4)});
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(dark ? "#1d1f25" : "#ffffff"));
        const QRectF address(126, 10, width - 146, height - 20);
        painter.drawRoundedRect(address, 7, 7);
        label = address.adjusted(14, 0, -14, 0);
    }
    QFont font;
    font.setPixelSize(13);
    font.setWeight(options.frame == PresentationFrame::MacOS ? QFont::Medium : QFont::Normal);
    painter.setFont(font);
    painter.setPen(QColor(dark ? "#d8dae2" : "#555963"));
    painter.drawText(label, Qt::AlignCenter, painter.fontMetrics().elidedText(options.frameTitle.simplified(), Qt::ElideRight, qMax(1, int(label.width()))));
    painter.setPen(QPen(QColor(dark ? "#181a20" : "#d8d9df"), 1));
    painter.drawLine(QPointF(0, height - 0.5), QPointF(width, height - 0.5));
    painter.restore();
}
static QImage renderPresentation(const QImage &source, const PresentationOptions &options, qint64 maximumPixels, int previewEdge)
{
    const QSize fullSize = presentationSize(source.size(), options, maximumPixels);
    if (source.isNull() || fullSize.isEmpty())
        return {};
    const auto style = int(options.style);
    const auto watermark = options.watermark.simplified();
    const int footer = watermark.isEmpty() ? 0 : 36;
    const int header = frameHeaderHeight(source.width(), options.frame);
    const int width = fullSize.width(), height = fullSize.height();
    const auto renderSize = previewEdge > 0 && qMax(width, height) > previewEdge
        ? fullSize.scaled(previewEdge, previewEdge, Qt::KeepAspectRatio).expandedTo(QSize(1, 1)) : fullSize;
    QImage image(renderSize, QImage::Format_ARGB32_Premultiplied);
    if (image.isNull()) return {};
    image.setColorSpace(QColorSpace::SRgb);
    QImage content = source.colorSpace().isValid() ? source.convertedToColorSpace(QColorSpace::SRgb) : source;
    if (content.isNull()) return {};
    content.setDevicePixelRatio(1); // Paint explicitly in physical pixels.
    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        if (previewEdge > 0) painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.scale(double(renderSize.width()) / width, double(renderSize.height()) / height);
        QLinearGradient gradient(0, 0, width, height);
        const bool dark = options.style == PresentationStyle::Dark;
        gradient.setColorAt(0, QColor(dark ? "#202126" : style == 2 ? "#c7dcff" : "#f5f5f7"));
        gradient.setColorAt(1, QColor(dark ? "#414553" : style == 2 ? "#ead5ff" : "#e9edf4"));
        painter.fillRect(QRectF(0, 0, width, height), gradient);
        if (!deviceFrameSize(source.size(), options.frame).isEmpty()) {
            painter.save();
            painter.translate(options.padding, options.padding);
            drawDeviceFrame(painter, content, options.frame);
            painter.restore();
        } else {
            const QRectF rect(options.padding, options.padding, source.width(), source.height() + header);
            const qreal radius = options.rounded ? (header ? 12 * frameScale(source.width()) : qMin(16, qMin(source.width(), source.height()) / 4)) : 0;
            if (options.padding >= 16) {
                for (int spread = 12; spread >= 1; --spread) {
                    painter.setPen(Qt::NoPen);
                    painter.setBrush(QColor(0, 0, 0, 3));
                    painter.drawRoundedRect(rect.translated(0, 5).adjusted(-spread, -spread, spread, spread), radius + spread, radius + spread);
                }
            }
            QPainterPath clip;
            clip.addRoundedRect(rect, radius, radius);
            painter.save();
            painter.setClipPath(clip);
            if (header)
                drawFrameHeader(painter, QRectF(rect.topLeft(), QSizeF(rect.width(), header)), options);
            painter.drawImage(QRectF(options.padding, options.padding + header, source.width(), source.height()), content, content.rect());
            painter.restore();
        }
        if (!watermark.isEmpty()) {
            QFont font; font.setPixelSize(16); painter.setFont(font);
            painter.setPen(QColor(dark ? "#f0f1f5" : "#343744"));
            const int inset = qMax(12, options.padding);
            const QRect label(inset, height - footer, qMax(1, width - inset * 2), footer);
            painter.drawText(label, Qt::AlignRight | Qt::AlignVCenter,
                painter.fontMetrics().elidedText(watermark, Qt::ElideRight, label.width()));
        }
    }
    image.setDevicePixelRatio(previewEdge > 0 ? 1 : source.devicePixelRatio());
    return image;
}
QImage composePresentation(const QImage &source, const PresentationOptions &options, qint64 maximumPixels)
{
    return renderPresentation(source, options, maximumPixels, 0);
}
QImage previewPresentation(const QImage &source, const PresentationOptions &options, int maximumEdge, qint64 maximumPixels)
{
    if (maximumEdge < 1 || maximumEdge > 2048) return {};
    return renderPresentation(source, options, maximumPixels, maximumEdge);
}
}
