// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QObject>
#include <QElapsedTimer>
#include <QTimer>

namespace NkShot {

// Owns preparation timing only. It never creates an output or opens a stream.
// The caller must recheck token + Starting immediately before its queued commit.
class RecordingPreparation : public QObject {
    Q_OBJECT
    Q_PROPERTY(Phase phase READ phase NOTIFY changed)
    Q_PROPERTY(qint64 remainingMilliseconds READ remainingMilliseconds NOTIFY changed)
    Q_PROPERTY(quint64 token READ token NOTIFY changed)
public:
    enum Phase { Idle, Selecting, Countdown, Starting, Recording };
    Q_ENUM(Phase)
    explicit RecordingPreparation(QObject *parent = nullptr);
    ~RecordingPreparation() override;
    Phase phase() const { return m_phase; }
    qint64 remainingMilliseconds() const;
    quint64 token() const { return m_token; }
    bool isCurrent(quint64 token) const { return token != 0 && token == m_token && m_phase != Idle; }
    quint64 begin(int delayMilliseconds);
    bool targetReady(quint64 token);
    bool cancel();
    bool markRecording(quint64 token);
    bool finish(quint64 token);
    bool fail(quint64 token) { return finish(token); }
Q_SIGNALS:
    void changed();
    void readyToStart(quint64 token);
private:
    void advance();
    void invalidate();
    QElapsedTimer m_clock;
    QTimer m_timer;
    Phase m_phase = Idle;
    quint64 m_token = 0;
    int m_delay = 0;
    qint64 m_deadline = 0;
};

} // namespace NkShot
