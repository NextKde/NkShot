// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QObject>
#include <QImage>
#include <QUrl>
#include <QPointer>
#include <QTimer>
class QProcess;
namespace NkShot {
// Validates a user-selected local media file. It never changes application media.
class LocalMediaLoader : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QUrl sourceUrl READ sourceUrl NOTIFY changed)
public:
    explicit LocalMediaLoader(QObject *parent = nullptr);
    ~LocalMediaLoader() override;
    bool busy() const { return m_busy; }
    QString error() const { return m_error; }
    QUrl sourceUrl() const { return m_source; }
    // Recheck immediately before committing a validated result. Not an atomic
    // guarantee against a same-user replacement after this check.
    bool validatedSourceUnchanged() const;
    Q_INVOKABLE void load(const QUrl &url);
    Q_INVOKABLE void cancel();
Q_SIGNALS:
    void changed();
    void imageReady(const QImage &image, const QUrl &source);
    void videoReady(const QUrl &source, qint64 durationMs, const QSize &frameSize, bool hasAudio);
private:
    enum class Stage { Probe, Decode };
    void stop();
    void fail(const QString &message);
    bool unchanged() const;
    void startImage(quint64 epoch);
    void launch(const QString &program, const QStringList &arguments, Stage stage);
    void completed(Stage stage);
    quint64 m_epoch = 0, m_device = 0, m_inode = 0;
    qint64 m_size = 0, m_modifiedNs = 0;
    bool m_busy = false, m_audio = false, m_validated = false, m_webm = false;
    QString m_error, m_ffmpeg;
    QUrl m_source;
    QSize m_dimensions;
    qint64 m_durationMs = 0;
    double m_fps = 0, m_startSeconds = 0;
    QPointer<QProcess> m_process;
    QByteArray m_output, m_diagnostics;
    QTimer m_timeout;
};
}
