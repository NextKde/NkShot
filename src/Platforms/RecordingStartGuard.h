// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QtTypes>
#include <limits>
namespace NkShot {
// A recorder may have started its producer before its public state leaves Idle.
// Register before start(); cancellation alone is never proof of quiescence.
// Single-threaded owner only. Keep the associated source lease until a matching
// recorder Idle completion is observed, even if Recording arrives after cancel.
class RecordingStartGuard {
public:
    bool allowsNewAttempt() const { return !m_outstanding && m_token != std::numeric_limits<quint64>::max(); }
    bool outstanding() const { return m_outstanding; }
    quint64 token() const { return m_token; }
    bool cancellationRequested() const { return m_cancelled; }
    bool recordingWasObserved() const { return m_recording; }
    quint64 markStartIssued() {
        if (!allowsNewAttempt()) return 0;
        ++m_token; m_outstanding = true; m_cancelled = false; m_recording = false;
        return m_token;
    }
    bool cancel(quint64 attempt) {
        if (!matches(attempt)) return false;
        m_cancelled = true;
        return true;
    }
    bool recordingObserved(quint64 attempt) {
        if (!matches(attempt)) return false;
        m_recording = true;
        return true;
    }
    bool recorderIdleObserved(quint64 attempt) {
        if (!matches(attempt)) return false;
        m_outstanding = false;
        return true;
    }
private:
    bool matches(quint64 attempt) const { return m_outstanding && attempt != 0 && attempt == m_token; }
    quint64 m_token = 0;
    bool m_outstanding = false, m_cancelled = false, m_recording = false;
};
} // namespace NkShot
