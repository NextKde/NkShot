// SPDX-License-Identifier: GPL-3.0-only
#include "RecordingBackend.h"
#include <PipeWireRecord>
namespace NkShot {
namespace {
class NativeRecorder final : public RecordingRecorder {
public:
    NativeRecorder() {
        connect(&m_native, &PipeWireRecord::stateChanged, this, &RecordingRecorder::stateChanged);
        connect(&m_native, &PipeWireRecord::errorFound, this, &RecordingRecorder::errorFound);
    }
    State state() const override { return m_native.state(); }
    QList<Encoder> suggestedEncoders() const override { return m_native.suggestedEncoders(); }
    QString output() const override { return m_native.output(); }
    bool recordSystemAudio() const override { return m_native.recordSystemAudio(); }
    bool recordMicrophone() const override { return m_native.recordMicrophone(); }
    void setOutput(const QString &value) override { m_native.setOutput(value); }
    void setEncoder(Encoder value) override { m_native.setEncoder(value); }
    void setRecordSystemAudio(bool value) override { m_native.setRecordSystemAudio(value); }
    void setRecordMicrophone(bool value) override { m_native.setRecordMicrophone(value); }
    void setNodeId(uint value) override { m_native.setNodeId(value); }
    void setMaxPendingFrames(int value) override { m_native.setMaxPendingFrames(value); }
    void start() override { m_native.start(); }
    void stop() override { m_native.stop(); }
private:
    PipeWireRecord m_native;
};
class NativeSource final : public RecordingSource {
public:
    NativeSource(ScreencastingStream *stream, QObject *parent) : RecordingSource(parent), m_native(stream) {
        stream->setParent(this);
        setObjectName(stream->objectName());
        connect(stream, &ScreencastingStream::created, this, &RecordingSource::created);
        connect(stream, &ScreencastingStream::failed, this, &RecordingSource::failed);
        connect(stream, &ScreencastingStream::closed, this, &RecordingSource::closed);
    }
    quint32 nodeId() const override { return m_native->nodeId(); }
private:
    ScreencastingStream *m_native;
};
class NativeScreencast final : public RecordingScreencast {
public:
    explicit NativeScreencast(QObject *parent) : RecordingScreencast(parent), m_native(this) {
        connect(&m_native, &Screencasting::initialized, this, &RecordingScreencast::initialized);
        connect(&m_native, &Screencasting::removed, this, &RecordingScreencast::removed);
        connect(&m_native, &Screencasting::sourcesChanged, this, &RecordingScreencast::sourcesChanged);
    }
    bool isAvailable() const override { return m_native.isAvailable(); }
    bool isRegionAutoScaleSupported() const override { return m_native.isRegionAutoScaleSupported(); }
    RecordingSource *createOutputStream(QScreen *screen, Screencasting::CursorMode mode) override { return wrap(m_native.createOutputStream(screen, mode)); }
    RecordingSource *createWindowStream(const QString &uuid, Screencasting::CursorMode mode) override { return wrap(m_native.createWindowStream(uuid, mode)); }
    RecordingSource *createRegionStream(const QRect &rect, qreal scale, Screencasting::CursorMode mode) override { return wrap(m_native.createRegionStream(rect, scale, mode)); }
private:
    RecordingSource *wrap(ScreencastingStream *source) { return source ? new NativeSource(source, this) : nullptr; }
    Screencasting m_native;
};
}
std::unique_ptr<RecordingRecorder> makeNativeRecordingRecorder() { return std::make_unique<NativeRecorder>(); }
RecordingScreencast *makeNativeRecordingScreencast(QObject *parent) { return new NativeScreencast(parent); }
} // namespace NkShot
