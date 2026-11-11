// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QQuickView>
#include <QPointer>
class TranslationManager;

// Immutable recognition result; selection/copy stays local and never edits the image.
class OcrTextWindow : public QQuickView
{
    Q_OBJECT
    Q_PROPERTY(QObject *translation READ translation CONSTANT)
    Q_PROPERTY(bool translationEnabled READ translationEnabled CONSTANT)
    Q_PROPERTY(QString heading READ heading CONSTANT)
    Q_PROPERTY(QString description READ description CONSTANT)
    Q_PROPERTY(QString recognizedText READ recognizedText CONSTANT)
public:
    OcrTextWindow(const QString &text, QQmlEngine *engine, bool codeResult = false);
    ~OcrTextWindow() override;
    QObject *translation() const;
    bool translationEnabled() const { return !m_codeResult; }
    Q_INVOKABLE void copyTranslation();
    QString heading() const;
    QString description() const;
    Q_INVOKABLE void saveAs();
    QString recognizedText() const { return m_text; }
    Q_INVOKABLE void copyText();
Q_SIGNALS:
    void textCopied();
    void saveFinished(const QString &message, bool success);
protected:
    bool event(QEvent *event) override;
private:
    TranslationManager *m_translation;
    const QString m_text;
    const bool m_codeResult;
    QPointer<QObject> m_saveDialog;
};
