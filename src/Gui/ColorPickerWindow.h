// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QImage>
#include <QPointer>
#include <QQuickView>
#include <QFutureWatcher>

class ColorImageProvider;
class ScreenColorPicker;

// Immutable screenshot sampling and explicit desktop-portal color requests.
// Neither source modifies the document or copies without an explicit action.
class ColorPickerWindow : public QQuickView
{
    Q_OBJECT
    Q_PROPERTY(bool hasImage READ hasImage CONSTANT)
    Q_PROPERTY(bool screenMode READ screenMode WRITE setScreenMode NOTIFY sourceModeChanged)
    Q_PROPERTY(bool screenPending READ screenPending NOTIFY screenStateChanged)
    Q_PROPERTY(QString screenStatus READ screenStatus NOTIFY screenStateChanged)
    Q_PROPERTY(QObject *screenPicker READ screenPicker CONSTANT)
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
    bool hasImage() const { return !m_image.isNull(); }
    bool screenMode() const { return m_screenMode; }
    void setScreenMode(bool screen);
    bool screenPending() const;
    QString screenStatus() const;
    QObject *screenPicker() const;
    Q_INVOKABLE void pickScreenColor();
    Q_INVOKABLE void cancelScreenPick();
    QUrl imageSource() const;
    QSize imageSize() const { return m_image.size(); }
    QUrl neighborhoodSource() const;
    QPoint pixel() const { return m_pixel; }
    QColor sampleColor() const;
    QString hexValue() const;
    QString rgbValue() const;
    bool hasSample() const;
    Q_INVOKABLE void sampleAt(qreal x, qreal y, qreal width, qreal height);
    Q_INVOKABLE void moveSample(int dx, int dy);
    Q_INVOKABLE void copyHex();
    Q_INVOKABLE void copyRgb();
Q_SIGNALS:
    void sampleChanged();
    void sourceModeChanged();
    void screenStateChanged();
    void valueCopied(const QString &value);
protected:
    bool event(QEvent *event) override;
private:
    void selectPixel(QPoint pixel);
    void requestScreenColor(const QString &parentIdentifier);
    void releaseParentExport();
    bool imageHasSample() const { return m_image.rect().contains(m_pixel); }
    const QImage m_image;
    QPointer<QQmlEngine> m_engine;
    ColorImageProvider *m_provider;
    const QString m_providerId;
    QPoint m_pixel{-1, -1};
    quint64 m_revision = 0;
    ScreenColorPicker *m_screenPicker = nullptr;
    QPointer<QFutureWatcher<QString>> m_parentWatcher;
    bool m_screenMode = false;
    bool m_screenResultValid = false;
    bool m_screenRequestActive = false;
    bool m_waitingForParent = false;
    bool m_parentExported = false;
    quint64 m_screenEpoch = 0;
    QString m_screenNotice;
};
