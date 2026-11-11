// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QQuickView>
#include <QImage>
#include <QPointer>
#include <atomic>
#include <memory>

// A local immutable snapshot, independent of the editable document.
class PinnedImageWindow : public QQuickView
{
    Q_OBJECT
    Q_PROPERTY(bool extractingText READ extractingText NOTIFY extractingTextChanged)
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
    bool extractingText() const {return m_extractingText;}
    Q_INVOKABLE void extractText(const QString &language = QString());
    Q_INVOKABLE void extractTextArea(const QRect &area);
    Q_INVOKABLE void copyImage();
    Q_INVOKABLE void saveAs();
Q_SIGNALS:
    void extractingTextChanged();
    void textExtractionStatus(const QString &message);
    void stayOnTopChanged();
    void imageCopied();
    void saveFinished(const QString &message, bool success);
protected:
    bool event(QEvent *event) override;
private:
    void showExtractedText(const QString &text);
    void extractTextRegion(const QRect &area, const QString &language);
    QRect m_pendingArea;
    QString m_pendingText;
    QStringList m_pendingLanguages;
    bool m_extractingText=false;
    bool m_closing=false;
    std::shared_ptr<std::atomic_bool> m_ocrCancelled;
    const QImage m_image;
    const QString m_providerId;
    QPointer<QQmlEngine> m_sharedEngine;
    QPointer<QObject> m_saveDialog;
};
