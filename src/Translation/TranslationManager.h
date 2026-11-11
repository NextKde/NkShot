// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QTimer>

// One immutable OCR window owns one local translation job. No clipboard access.
class TranslationManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString translatedText READ translatedText NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
public:
    explicit TranslationManager(QObject *parent = nullptr, int timeoutMs = 60000);
    ~TranslationManager() override;
    bool busy() const { return !m_process.isNull(); }
    QString translatedText() const { return m_text; }
    QString status() const { return m_status; }
    Q_INVOKABLE void translate(const QString &text, const QString &direction);
    Q_INVOKABLE void cancel();
Q_SIGNALS:
    void changed();
private:
    void fail(const QString &code);
    void stop();
    QPointer<QProcess> m_process;
    QTimer m_timeout;
    QByteArray m_output;
    qsizetype m_stderrBytes = 0;
    quint64 m_epoch = 0;
    QString m_text, m_status;
};
