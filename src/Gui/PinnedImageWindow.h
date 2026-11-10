// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QQuickView>
#include <QImage>
#include <QPointer>

// A local immutable snapshot, independent of the editable document.
class PinnedImageWindow : public QQuickView
{
    Q_OBJECT
    Q_PROPERTY(QUrl imageSource READ imageSource CONSTANT)
    Q_PROPERTY(QSize snapshotSize READ snapshotSize CONSTANT)
    Q_PROPERTY(bool stayOnTop READ stayOnTop WRITE setStayOnTop NOTIFY stayOnTopChanged)
public:
    PinnedImageWindow(const QImage &image, QQmlEngine *engine);
    ~PinnedImageWindow() override;
    QUrl imageSource() const;
    QSize snapshotSize() const { return m_image.size(); }
    const QImage &snapshot() const { return m_image; }
    bool stayOnTop() const { return flags().testFlag(Qt::WindowStaysOnTopHint); }
    void setStayOnTop(bool value);
    Q_INVOKABLE void copyImage();
    Q_INVOKABLE void saveAs();
Q_SIGNALS:
    void stayOnTopChanged();
    void imageCopied();
    void saveFinished(const QString &message, bool success);
protected:
    bool event(QEvent *event) override;
private:
    const QImage m_image;
    const QString m_providerId;
    QPointer<QQmlEngine> m_sharedEngine;
    QPointer<QObject> m_saveDialog;
};
