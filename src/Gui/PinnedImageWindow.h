// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QQuickView>
#include <QImage>
#include <QPointer>
#include <QVariantList>
#include "Imaging/LocalTextExtraction.h"
#include <atomic>
#include <memory>

// A local immutable snapshot, independent of the editable document.
class PinnedImageWindow : public QQuickView
{
    Q_OBJECT
    Q_PROPERTY(bool textSelectionActive READ textSelectionActive NOTIFY textSelectionActiveChanged)
    Q_PROPERTY(QVariantList textWords READ textWords NOTIFY textWordsChanged)
    Q_PROPERTY(QString selectedText READ selectedText NOTIFY textSelectionChanged)
    Q_PROPERTY(int selectionFirst READ selectionFirst NOTIFY textSelectionChanged)
    Q_PROPERTY(int selectionLast READ selectionLast NOTIFY textSelectionChanged)
    Q_PROPERTY(int activeWord READ activeWord NOTIFY textSelectionChanged)
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
    bool extractingText() const {return m_extractingText || m_inlineBusy;}
    Q_INVOKABLE void extractText(const QString &language = QString());
    Q_INVOKABLE void extractTextArea(const QRect &area);
    bool textSelectionActive() const { return m_textSelectionActive; }
    QVariantList textWords() const;
    QString selectedText() const;
    int selectionFirst() const { return qMin(m_anchorWord, m_activeWord); }
    int selectionLast() const { return qMax(m_anchorWord, m_activeWord); }
    int activeWord() const { return m_activeWord; }
    Q_INVOKABLE void selectTextOnImage();
    Q_INVOKABLE void cancelTextSelection();
    Q_INVOKABLE void selectWordAt(qreal x, qreal y, bool extend = false);
    Q_INVOKABLE void moveTextSelection(int direction, bool extend = false);
    Q_INVOKABLE void selectAllText();
    Q_INVOKABLE void copySelectedText();
    Q_INVOKABLE void copyImage();
    Q_INVOKABLE void saveAs();
Q_SIGNALS:
    void textSelectionActiveChanged();
    void textWordsChanged();
    void textSelectionChanged();
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
    void setWordSelection(int index, bool extend);
    QVector<NkShot::Imaging::TextWord> m_words;
    bool m_textSelectionActive = false;
    bool m_inlineBusy = false;
    int m_anchorWord = -1;
    int m_activeWord = -1;
    quint64 m_inlineGeneration = 0;
    std::shared_ptr<std::atomic_bool> m_inlineCancelled;
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
