// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "ScrollCaptureSession.h"
#include <QObject>
#include <QPointer>
#include <QRectF>
#include <QTimer>
#include <functional>

namespace NkShot::Imaging {
struct ScrollOutputLayout {
    QString id;
    QRectF logicalGeometry;
    QSize nativePixels;
    qreal dpr = 1;
    bool operator==(const ScrollOutputLayout &) const = default;
};
// Only a selection fully contained in one output is accepted. Rounded edges
// define a single fixed physical rectangle; the native frame must later match.
QRect scrollSelectionPixels(const ScrollOutputLayout &, const QRectF &globalLogicalSelection);

// GUI/controller-thread only. Manual and opt-in timed collection use the same
// strict region/metadata and hide-barrier checks; no scroll input is injected.
// Backend callbacks feed frameCaptured/frameFailed with their original ticket.
// The callback owner must outlive its functions, monitored via backendLifetime.
class ScrollCaptureController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool collecting READ collecting WRITE setCollecting NOTIFY collectingChanged)
public:
    enum class State { Idle, Ready, WaitingForHidden, Capturing, Finished, Cancelled };
    using Request = std::function<bool(quint64, QRect)>;
    using Cancel = std::function<void()>;
    ScrollCaptureController(QObject *backendLifetime, Request request, Cancel cancel, QObject *parent = nullptr);
    ~ScrollCaptureController() override;
    bool begin(const ScrollOutputLayout &, const QRectF &globalLogicalSelection);
    bool captureNext();
    void setCollecting(bool collecting);
    bool collecting() const { return m_collecting; }
    // A UI/compositor adapter must establish actual exclusion/repaint, NOT a
    // guessed sleep, and re-read output geometry before confirming this ticket.
    bool confirmHidden(quint64 ticket, const ScrollOutputLayout &current);
    void frameCaptured(quint64 ticket, const ScrollOutputLayout &current, const QImage &frame);
    void frameFailed(quint64 ticket, const QString &reason);
    bool finish();
    void cancel();
    State state() const { return m_state; }
    int frameCount() const { return m_session.frameCount(); }
    QImage preview() const { return m_session.preview(); }
    QVector<int> seams() const { return m_session.seams(); }
Q_SIGNALS:
    void stateChanged();
    void collectingChanged();
    void frameSkipped();
    void hideWindowsRequested(quint64 ticket);
    void restoreWindowsRequested();
    void frameAccepted(int count);
    void failed(const QString &reason);
    void resultReady(const QImage &result);
private:
    class Transition;
    void scheduleCollection(int delay);
    void stopCollection();
    void setState(State);
    void restore();
    void rejectPending(const QString &reason, bool abortBackend);
    QPointer<QObject> m_backend;
    Request m_request;
    Cancel m_cancel;
    ScrollCaptureSession m_session;
    ScrollOutputLayout m_layout;
    ScrollCaptureSession::Region m_region;
    State m_state = State::Idle;
    QTimer m_timeout;
    QTimer m_collectionTimer;
    QImage m_lastAccepted;
    quint64 m_collectionEpoch = 0;
    bool m_collecting = false;
    quint64 m_pending = 0;
    bool m_windowsHidden = false;
    bool m_transition = false;
};
}
