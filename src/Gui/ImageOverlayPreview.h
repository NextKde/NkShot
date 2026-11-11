// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QQuickPaintedItem>
#include <QImage>

// Reusable immutable image layer with an injected destination rectangle.
class ImageOverlayPreview : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QImage image READ image WRITE setImage NOTIFY imageChanged)
    Q_PROPERTY(QRectF displayRect READ displayRect WRITE setDisplayRect NOTIFY displayRectChanged)
public:
    explicit ImageOverlayPreview(QQuickItem *parent = nullptr) : QQuickPaintedItem(parent) {}
    QImage image() const { return m_image; }
    void setImage(const QImage &image) { if(m_image==image)return;m_image=image;update();Q_EMIT imageChanged(); }
    QRectF displayRect() const { return m_displayRect; }
    void setDisplayRect(const QRectF &rect) { if(m_displayRect==rect)return;m_displayRect=rect;update();Q_EMIT displayRectChanged(); }
    void paint(QPainter *painter) override;
Q_SIGNALS:
    void imageChanged();
    void displayRectChanged();
private:
    QImage m_image;
    QRectF m_displayRect;
};
