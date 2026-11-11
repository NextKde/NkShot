// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QObject>
#include <QUrl>
#include <QStringList>
#include <QPointer>
#include <QTimer>
#include <QDateTime>
#include <QSize>
#include <QRect>
class QProcess;
class VideoClipJob : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool cropAvailable READ cropAvailable NOTIFY changed)
    Q_PROPERTY(QString cropAspect READ cropAspect WRITE setCropAspect NOTIFY changed)
    Q_PROPERTY(QSize sourceSize READ sourceSize NOTIFY changed)
    Q_PROPERTY(QRect cropRect READ cropRect NOTIFY changed)
    Q_PROPERTY(QString outputFormat READ outputFormat WRITE setOutputFormat NOTIFY changed)
    Q_PROPERTY(QUrl sourceUrl READ sourceUrl NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(qint64 durationMs READ durationMs NOTIFY changed)
    Q_PROPERTY(bool hasAudio READ hasAudio NOTIFY changed)
    Q_PROPERTY(bool includeAudio READ includeAudio WRITE setIncludeAudio NOTIFY changed)
    Q_PROPERTY(qreal progress READ progress NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QUrl outputUrl READ outputUrl NOTIFY changed)
public:
    explicit VideoClipJob(QObject *parent = nullptr);
    ~VideoClipJob() override;
    bool cropAvailable() const { return m_mediaValid && m_squarePixels; }
    QString cropAspect() const { return m_cropAspect; }
    Q_INVOKABLE void setCropAspect(const QString &aspect);
    QSize sourceSize() const { return QSize(m_width, m_height); }
    QRect cropRect() const;
    QString outputFormat() const { return m_format; }
    Q_INVOKABLE void setOutputFormat(const QString &format);
    QUrl sourceUrl() const { return m_source; }
    bool busy() const { return m_busy; }
    bool ready() const { return m_ready; }
    qint64 durationMs() const { return m_duration; }
    bool hasAudio() const { return m_audio; }
    bool includeAudio() const { return m_includeAudio; }
    Q_INVOKABLE void setIncludeAudio(bool include);
    qreal progress() const { return m_progress; }
    QString error() const { return m_error; }
    QUrl outputUrl() const { return m_output; }
    Q_INVOKABLE void inspect(const QUrl &source);
    Q_INVOKABLE void exportClip(const QUrl &target, qint64 startMs, qint64 endMs);
    Q_INVOKABLE void cancel();
Q_SIGNALS:
    void changed();
    void exported(const QUrl &url);
private:
    enum Stage { None, Inspect, Encoders, Filters, Palette, Encode, Verify, Decode };
    void launch(const QString &program, const QStringList &args, Stage stage, int timeoutMs);
    void completed(Stage stage);
    void launchGifPass(bool palette);
    QString gifFilter() const;
    QString cropFilter() const;
    QString webmVideoFilter(int fps) const;
    void fail(const QString &message);
    void stop();
    void cleanup();
    bool sourceUnchanged() const;
    bool destinationUnchanged() const;
    QUrl m_source, m_output;
    QString m_ffmpeg, m_ffprobe, m_error, m_temp, m_target, m_palette;
    QString m_cropAspect = QStringLiteral("original");
    QString m_format = QStringLiteral("mp4");
    QByteArray m_stdout, m_stderr, m_progressBuffer;
    QPointer<QProcess> m_process;
    QTimer m_timeout;
    quint64 m_epoch = 0;
    Stage m_stage = None;
    bool m_webm = false;
    bool m_squarePixels = false;
    bool m_includeAudio = true, m_encodersChecked = false, m_x264Available = false, m_aacAvailable = false;
    bool m_busy = false, m_ready = false, m_audio = false, m_mediaValid = false;
    qint64 m_duration = 0, m_sourceSize = 0, m_clipDuration = 0, m_start = 0, m_end = 0;
    QDateTime m_sourceModified;
    quint64 m_sourceDevice=0,m_sourceInode=0;
    qreal m_progress = 0;
    int m_width = 0, m_height = 0;
    qreal m_frameMs=0;
};
