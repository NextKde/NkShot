// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "screencasting.h"
#include <PipeWireBaseEncodedStream>
#include <QObject>
#include <QRect>
#include <memory>
namespace NkShot {
class RecordingRecorder : public QObject {
    Q_OBJECT
public:
    using State = PipeWireBaseEncodedStream::State;
    using Encoder = PipeWireBaseEncodedStream::Encoder;
    using QObject::QObject;
    ~RecordingRecorder() override = default;
    virtual State state() const = 0;
    virtual QList<Encoder> suggestedEncoders() const = 0;
    virtual QString output() const = 0;
    virtual bool recordSystemAudio() const = 0;
    virtual bool recordMicrophone() const = 0;
    virtual void setOutput(const QString &) = 0;
    virtual void setEncoder(Encoder) = 0;
    virtual void setRecordSystemAudio(bool) = 0;
    virtual void setRecordMicrophone(bool) = 0;
    virtual void setNodeId(uint) = 0;
    virtual void setMaxPendingFrames(int) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
Q_SIGNALS:
    void stateChanged();
    void errorFound(const QString &error);
};
class RecordingSource : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~RecordingSource() override = default;
    virtual quint32 nodeId() const = 0;
Q_SIGNALS:
    void created(quint32 nodeId);
    void failed(const QString &error);
    void closed();
};
class RecordingScreencast : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~RecordingScreencast() override = default;
    virtual bool isAvailable() const = 0;
    virtual bool isRegionAutoScaleSupported() const = 0;
    virtual RecordingSource *createOutputStream(QScreen *, Screencasting::CursorMode) = 0;
    virtual RecordingSource *createWindowStream(const QString &, Screencasting::CursorMode) = 0;
    virtual RecordingSource *createRegionStream(const QRect &, qreal, Screencasting::CursorMode) = 0;
Q_SIGNALS:
    void initialized();
    void removed();
    void sourcesChanged();
};
std::unique_ptr<RecordingRecorder> makeNativeRecordingRecorder();
RecordingScreencast *makeNativeRecordingScreencast(QObject *parent = nullptr);
} // namespace NkShot
