// SPDX-License-Identifier: GPL-3.0-only
#include "SoftwareVideoPreview.h"
#include <QPainter>
#include <QVideoFrame>

SoftwareVideoPreview::SoftwareVideoPreview(QQuickItem *parent) : QQuickPaintedItem(parent) {}
void SoftwareVideoPreview::setSink(QVideoSink *sink)
{
    if (m_sink == sink) return;
    disconnect(m_connection);
    m_sink = sink;
    if (m_sink) m_connection = connect(m_sink, &QVideoSink::videoFrameChanged, this, [this] { updateFrame(); });
    updateFrame();
    Q_EMIT sinkChanged();
}
void SoftwareVideoPreview::setActive(bool active)
{
    if (m_active == active) return;
    m_active = active;
    updateFrame();
    Q_EMIT activeChanged();
}
void SoftwareVideoPreview::updateFrame()
{
    m_image = m_active && m_sink ? m_sink->videoFrame().toImage() : QImage();
    update();
    Q_EMIT frameChanged();
}
void SoftwareVideoPreview::paint(QPainter *painter)
{
    if (m_image.isNull()) return;
    const auto size = QSizeF(m_image.size()).scaled(QSizeF(width(), height()), Qt::KeepAspectRatio);
    const QRectF target((width() - size.width()) / 2, (height() - size.height()) / 2, size.width(), size.height());
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    painter->drawImage(target, m_image);
}
