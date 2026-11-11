// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QImage>
#include <QPointer>
#include <QQuickView>

class ColorImageProvider;

// A private, immutable image sample. It never captures the desktop or edits the document.
class ColorPickerWindow : public QQuickView
{
    Q_OBJECT
    Q_PROPERTY(QUrl imageSource READ imageSource CONSTANT)
    Q_PROPERTY(QSize imageSize READ imageSize CONSTANT)
    Q_PROPERTY(QUrl neighborhoodSource READ neighborhoodSource NOTIFY sampleChanged)
    Q_PROPERTY(QPoint pixel READ pixel NOTIFY sampleChanged)
    Q_PROPERTY(QColor sampleColor READ sampleColor NOTIFY sampleChanged)
    Q_PROPERTY(QString hexValue READ hexValue NOTIFY sampleChanged)
    Q_PROPERTY(QString rgbValue READ rgbValue NOTIFY sampleChanged)
    Q_PROPERTY(bool hasSample READ hasSample NOTIFY sampleChanged)
public:
    ColorPickerWindow(const QImage &image, QQmlEngine *engine);
    ~ColorPickerWindow() override;
    bool valid() const { return !m_image.isNull(); }
    QUrl imageSource() const;
    QSize imageSize() const { return m_image.size(); }
    QUrl neighborhoodSource() const;
    QPoint pixel() const { return m_pixel; }
    QColor sampleColor() const;
    QString hexValue() const;
    QString rgbValue() const;
    bool hasSample() const { return m_image.rect().contains(m_pixel); }
    Q_INVOKABLE void sampleAt(qreal x, qreal y, qreal width, qreal height);
    Q_INVOKABLE void moveSample(int dx, int dy);
    Q_INVOKABLE void copyHex();
    Q_INVOKABLE void copyRgb();
Q_SIGNALS:
    void sampleChanged();
    void valueCopied(const QString &value);
protected:
    bool event(QEvent *event) override;
private:
    void selectPixel(QPoint pixel);
    const QImage m_image;
    QPointer<QQmlEngine> m_engine;
    ColorImageProvider *m_provider;
    const QString m_providerId;
    QPoint m_pixel{-1, -1};
    quint64 m_revision = 0;
};
