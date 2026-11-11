// SPDX-License-Identifier: GPL-3.0-only
#include "RecordingPreparation.h"
#include <QPointer>
#include <limits>
namespace NkShot {
RecordingPreparation::RecordingPreparation(QObject *parent) : QObject(parent) {
    m_clock.start();
    m_timer.setSingleShot(true);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &RecordingPreparation::advance);
}
RecordingPreparation::~RecordingPreparation() { m_timer.stop(); }
qint64 RecordingPreparation::remainingMilliseconds() const {
    return m_phase == Countdown ? qMax(qint64(0), m_deadline - m_clock.elapsed()) : 0;
}
quint64 RecordingPreparation::begin(int delayMilliseconds) {
    // Never recycle an epoch, even after theoretical quint64 exhaustion.
    if (m_phase != Idle || delayMilliseconds < 0 || delayMilliseconds > 10000 || m_token == std::numeric_limits<quint64>::max()) return 0;
    const auto attempt = ++m_token;
    m_delay = delayMilliseconds;
    m_phase = Selecting;
    Q_EMIT changed();
    return attempt;
}
bool RecordingPreparation::targetReady(quint64 attempt) {
    if (!isCurrent(attempt) || m_phase != Selecting) return false;
    m_deadline = m_clock.elapsed() + m_delay;
    m_phase = Countdown;
    const QPointer<RecordingPreparation> guard(this);
    Q_EMIT changed();
    if (!guard || !guard->isCurrent(attempt) || guard->m_phase != Countdown) return true;
    // Even zero delay emits readiness in a later event-loop turn.
    guard->m_timer.start(0);
    return true;
}
void RecordingPreparation::advance() {
    if (m_phase != Countdown) return;
    const auto attempt = m_token;
    const auto remaining = remainingMilliseconds();
    if (remaining > 0) {
        // Use the absolute monotonic deadline, not the number of timer callbacks.
        // Delayed event delivery can postpone starting, but can never shorten it.
        m_timer.start(int(qMin(qint64(40), remaining)));
        Q_EMIT changed();
        return;
    }
    m_timer.stop();
    m_phase = Starting;
    const QPointer<RecordingPreparation> guard(this);
    Q_EMIT changed();
    if (!guard || !guard->isCurrent(attempt) || guard->m_phase != Starting) return;
    Q_EMIT guard->readyToStart(attempt);
}
void RecordingPreparation::invalidate() {
    m_timer.stop();
    if (m_token != std::numeric_limits<quint64>::max()) ++m_token;
    m_phase = Idle;
    m_delay = 0;
    m_deadline = 0;
    Q_EMIT changed();
}
bool RecordingPreparation::cancel() {
    if (m_phase != Selecting && m_phase != Countdown && m_phase != Starting) return false;
    invalidate();
    return true;
}
bool RecordingPreparation::markRecording(quint64 attempt) {
    if (!isCurrent(attempt) || m_phase != Starting) return false;
    m_phase = Recording;
    Q_EMIT changed();
    return true;
}
bool RecordingPreparation::finish(quint64 attempt) {
    if (!isCurrent(attempt)) return false;
    invalidate();
    return true;
}

} // namespace NkShot
