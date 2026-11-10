// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QQuickView>
#include <QPointer>

// Immutable recognition result; selection/copy stays local and never edits the image.
class OcrTextWindow : public QQuickView
{
    Q_OBJECT
    Q_PROPERTY(QString recognizedText READ recognizedText CONSTANT)
public:
    OcrTextWindow(const QString &text, QQmlEngine *engine);
    ~OcrTextWindow() override;
    Q_INVOKABLE void saveAs();
    QString recognizedText() const { return m_text; }
    Q_INVOKABLE void copyText();
Q_SIGNALS:
    void textCopied();
    void saveFinished(const QString &message, bool success);
protected:
    bool event(QEvent *event) override;
private:
    const QString m_text;
    QPointer<QObject> m_saveDialog;
};
