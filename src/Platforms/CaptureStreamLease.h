// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QObject>
#include <QPointer>
namespace NkShot {
// Disconnecting a Qt signal alone does not retract already queued callbacks.
// The generation also invalidates those callbacks before the stream is deleted.
class CaptureStreamLease {
public:
    explicit CaptureStreamLease(QObject *receiver) : m_receiver(receiver) {}
    ~CaptureStreamLease() { retire(); }
    quint64 adopt(QObject *stream) { retire();m_stream=stream;return m_generation; }
    bool accepts(const QObject *stream,quint64 generation) const {
        return stream && m_stream && m_stream.data()==stream && generation==m_generation;
    }
    void quiesce() {
        ++m_generation;
        if(m_stream) QObject::disconnect(m_stream,nullptr,m_receiver,nullptr);
    }
    void retire() {
        quiesce();
        auto stream=m_stream;m_stream.clear();
        if(stream) stream->deleteLater();
    }
private:
    QObject *m_receiver;
    QPointer<QObject> m_stream;
    quint64 m_generation=0;
};
}
