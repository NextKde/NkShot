// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QQuickPaintedItem>
#include <QPointer>
#include <QVideoSink>
#include <QImage>

// CPU-only presentation fallback. The existing MediaPlayer owns decoding/audio.
class SoftwareVideoPreview : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVideoSink *sink READ sink WRITE setSink NOTIFY sinkChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(bool hasFrame READ hasFrame NOTIFY frameChanged)
public:
    explicit SoftwareVideoPreview(QQuickItem *parent = nullptr);
    QVideoSink *sink() const { return m_sink; }
    void setSink(QVideoSink *sink);
    bool active() const { return m_active; }
    void setActive(bool active);
    bool hasFrame() const { return !m_image.isNull(); }
    void paint(QPainter *painter) override;
Q_SIGNALS:
    void sinkChanged();
    void activeChanged();
    void frameChanged();
private:
    void updateFrame();
    QPointer<QVideoSink> m_sink;
    QMetaObject::Connection m_connection;
    QImage m_image;
    bool m_active = false;
};
