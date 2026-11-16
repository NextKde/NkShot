// SPDX-License-Identifier: GPL-3.0-only
#include "DeviceFrame.h"
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace NkShot::Imaging
{
namespace
{
struct DeviceLayout {
    QSizeF size;
    QRectF body;
    QRectF screen;
    qreal radius = 0;
    qreal scale = 1;
};
DeviceLayout layout(const QSize &source, PresentationFrame frame)
{
    if (source.isEmpty())
        return {};
    // Match the screenshot's aspect ratio. The bezel scales with its shorter edge.
    const qreal scale = qMax(.1, qMin(source.width(), source.height()) / 600.0);
    const QSizeF screen(source.width() / scale + 8, source.height() / scale + 8);
    qreal side, top, bottom, radius;
    switch (frame) {
    case PresentationFrame::MacDisplay:
        side = 12;
        top = 22;
        bottom = 16;
        radius = 18;
        break;
    case PresentationFrame::IPad:
        side = 7;
        top = 7;
        bottom = 7;
        radius = 15;
        break;
    case PresentationFrame::Surface:
        side = 10;
        top = 16;
        bottom = 10;
        radius = 12;
        break;
    case PresentationFrame::VisionPro:
        side = 14;
        top = 14;
        bottom = 14;
        radius = 26;
        break;
    default:
        return {};
    }
    const QRectF body(10, 10, screen.width() + side * 2, screen.height() + top + bottom);
    const QRectF display(body.x() + side, body.y() + top, screen.width(), screen.height());
    const qreal grip = frame == PresentationFrame::VisionPro ? 24 : 0;
    return {{body.width() + 20, body.height() + 20 + grip}, body, display, radius, scale};
}
QLinearGradient metal(const QRectF &rect)
{
    QLinearGradient gradient(rect.topLeft(), rect.bottomRight());
    gradient.setColorAt(0, QColor("#e3e6ec"));
    gradient.setColorAt(.3, QColor("#777f8e"));
    gradient.setColorAt(.55, QColor("#c8ced8"));
    gradient.setColorAt(1, QColor("#89919f"));
    return gradient;
}
void camera(QPainter &painter, QPointF center, qreal radius)
{
    painter.setPen(QPen(QColor("#414651"), .7));
    painter.setBrush(QColor("#080b11"));
    painter.drawEllipse(center, radius, radius);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#273b58"));
    painter.drawEllipse(center + QPointF(-radius * .2, -radius * .2), radius * .35, radius * .35);
}
void shadow(QPainter &painter, const QPainterPath &path)
{
    painter.save();
    painter.translate(0, 3);
    for (int spread = 10; spread >= 2; spread -= 2) {
        painter.setPen(QPen(QColor(0, 0, 0, 4), spread, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(QColor(0, 0, 0, 5));
        painter.drawPath(path);
    }
    painter.restore();
}
void screen(QPainter &painter, const QImage &source, const QRectF &rect, bool glass)
{
    painter.setPen(Qt::NoPen);
    painter.setBrush(glass ? QColor(255, 255, 255, 36) : QColor("#0b0d12"));
    painter.drawRoundedRect(rect, 7, 7);
    // Preserve every screenshot corner, with only a four-unit safe inset.
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(rect.adjusted(4, 4, -4, -4), source, source.rect());
}
void display(QPainter &painter, const QImage &source, const DeviceLayout &device, PresentationFrame frame)
{
    const auto body = device.body;
    QPainterPath outline;
    outline.addRoundedRect(body, device.radius, device.radius);
    shadow(painter, outline);
    painter.setPen(QPen(QColor("#8d94a1"), .8));
    painter.setBrush(metal(body));
    painter.drawPath(outline);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(frame == PresentationFrame::Surface ? "#242831" : "#17191e"));
    painter.drawRoundedRect(body.adjusted(1.8, 1.8, -1.8, -1.8), device.radius - 2, device.radius - 2);
    screen(painter, source, device.screen, false);
    if (frame == PresentationFrame::IPad) {
        // Keep the sensor and buttons in the narrow bezel, outside image pixels.
        const bool landscape = source.width() >= source.height();
        const QPointF lens = landscape ? QPointF(body.center().x(), body.top() + 4) : QPointF(body.right() - 4, body.center().y());
        camera(painter, lens, 1.8);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#858d9a"));
        const qreal length = qMin(35., body.width() / 5);
        painter.drawRoundedRect(QRectF(body.left() + body.width() * .15, body.top() - 1.5, length, 2), 1, 1);
    } else {
        const qreal top = frame == PresentationFrame::MacDisplay ? 10 : 8;
        camera(painter, {body.center().x(), body.top() + top}, 2.6);
        if (frame == PresentationFrame::Surface)
            camera(painter, {body.center().x() + 11, body.top() + top}, 1.6);
    }
}
void spatialWindow(QPainter &painter, const QImage &source, const DeviceLayout &device)
{
    const auto body = device.body;
    QPainterPath outline;
    outline.addRoundedRect(body, device.radius, device.radius);
    shadow(painter, outline);
    QLinearGradient frost(body.topLeft(), body.bottomRight());
    frost.setColorAt(0, QColor(255, 255, 255, 150));
    frost.setColorAt(.4, QColor(238, 242, 255, 75));
    frost.setColorAt(1, QColor(255, 255, 255, 115));
    painter.setBrush(frost);
    painter.setPen(QPen(QColor(255, 255, 255, 190), 1.5));
    painter.drawPath(outline);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(70, 80, 110, 35), .8));
    painter.drawRoundedRect(body.adjusted(2, 2, -2, -2), device.radius - 2, device.radius - 2);
    screen(painter, source, device.screen, true);
    const qreal gripWidth = qMin(90., body.width() * .28);
    const QRectF grip(body.center().x() - gripWidth / 2, body.bottom() + 15, gripWidth, 4);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(40, 45, 65, 30));
    painter.drawRoundedRect(grip.translated(0, 1), 2, 2);
    painter.setBrush(QColor(255, 255, 255, 220));
    painter.drawRoundedRect(grip, 2, 2);
    painter.setBrush(QColor(255, 255, 255, 140));
    painter.drawEllipse(QPointF(body.left() + 12, grip.center().y()), 5, 5);
    painter.setPen(QPen(QColor(80, 85, 105, 150), .9, Qt::SolidLine, Qt::RoundCap));
    const auto center = QPointF(body.left() + 12, grip.center().y());
    painter.drawLine(center + QPointF(-1.8, -1.8), center + QPointF(1.8, 1.8));
    painter.drawLine(center + QPointF(-1.8, 1.8), center + QPointF(1.8, -1.8));
}
}
QSizeF deviceFrameSize(const QSize &source, PresentationFrame frame)
{
    const auto device = layout(source, frame);
    return device.size * device.scale;
}
void drawDeviceFrame(QPainter &painter, const QImage &source, PresentationFrame frame)
{
    const auto device = layout(source.size(), frame);
    if (source.isNull() || device.size.isEmpty())
        return;
    painter.save();
    painter.scale(device.scale, device.scale);
    if (frame == PresentationFrame::VisionPro)
        spatialWindow(painter, source, device);
    else
        display(painter, source, device, frame);
    painter.restore();
}
}
