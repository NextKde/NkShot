// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QObject>
#include <QImage>
#include <QPointer>
#include <QProcess>
#include <QTimer>
#include <QUrl>
#include <QVector>
#include <sys/stat.h>
class QQmlEngine;
class FrameInspectionProvider;
class FrameInspectionSession final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(int frameCount READ frameCount NOTIFY changed)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY changed)
    Q_PROPERTY(int requestedIndex READ requestedIndex NOTIFY changed)
    Q_PROPERTY(qint64 positionMs READ positionMs NOTIFY changed)
    Q_PROPERTY(QUrl previewUrl READ previewUrl NOTIFY changed)
    Q_PROPERTY(QSize sourceSize READ sourceSize NOTIFY changed)
public:
    explicit FrameInspectionSession(QQmlEngine *,QObject *parent=nullptr);
    ~FrameInspectionSession() override;
    Q_INVOKABLE bool begin(const QUrl &,qint64 requestedPositionMs=0);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void cancel() {clear();}
    Q_INVOKABLE void requestFrame(int zeroBased);
    bool ready() const {return !m_image.isNull()&&!m_busy;}
    bool busy() const {return m_busy;}
    QString error() const {return m_error;}
    int frameCount() const {return m_pts.size();}
    int currentIndex() const {return ready()?m_current:-1;}
    int requestedIndex() const {return m_requested;}
    qint64 positionMs() const;
    QUrl previewUrl() const;
    QSize sourceSize() const {return m_size;}
    QImage snapshot() const {return ready()&&unchanged()?m_image:QImage();}
Q_SIGNALS:
    void changed();
private:
    enum Stage {Idle,Metadata,Index,Frame};
    void dispatch();
    void finished(int,QProcess::ExitStatus);
    void readOutput();
    void fail(const QString &);
    void resetImage();
    bool unchanged() const;
    bool parseMetadata();
    bool parseIndex();
    void queue(Stage);
    QPointer<QQmlEngine> m_engine;
    FrameInspectionProvider *m_provider=nullptr;
    QString m_providerId,m_path,m_error,m_ffmpeg,m_ffprobe,m_timeBase;
    QProcess *m_process;
    QTimer m_timeout;
    Stage m_stage=Idle,m_pending=Idle;
    quint64 m_epoch=0,m_runningEpoch=0;
    bool m_busy=false,m_webm=false;
    qint64 m_initialMs=0,m_num=0,m_den=1;
    int m_requested=-1,m_current=-1;
    QSize m_size;
    struct stat m_identity{};
    QVector<qint64> m_pts;
    QVector<int> m_keys;
    QByteArray m_output,m_diagnostics;
    QImage m_image;
};
