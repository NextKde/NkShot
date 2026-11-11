// SPDX-License-Identifier: GPL-3.0-only
#include "ScrollCaptureController.h"
#include <QtMath>
#include <QScopedValueRollback>

namespace NkShot::Imaging {
namespace {
bool finiteRect(const QRectF &r) {
    return qIsFinite(r.x()) && qIsFinite(r.y()) && qIsFinite(r.width()) && qIsFinite(r.height());
}
}
QRect scrollSelectionPixels(const ScrollOutputLayout &layout, const QRectF &selection)
{
    if (layout.id.isEmpty() || !qIsFinite(layout.dpr) || layout.dpr <= 0 || layout.dpr > 16
        || !finiteRect(layout.logicalGeometry) || !finiteRect(selection)
        || !layout.logicalGeometry.isValid() || !selection.isValid()
        || !layout.logicalGeometry.contains(selection) || layout.nativePixels.width() < 1
        || layout.nativePixels.height() < 1 || layout.nativePixels.width() > 32000 || layout.nativePixels.height() > 32000
        || qAbs(layout.logicalGeometry.width()*layout.dpr-layout.nativePixels.width()) > 0.51
        || qAbs(layout.logicalGeometry.height()*layout.dpr-layout.nativePixels.height()) > 0.51) return {};
    const auto local = selection.translated(-layout.logicalGeometry.topLeft());
    const int left=qRound(local.left()*layout.dpr), top=qRound(local.top()*layout.dpr);
    const int right=qRound(local.right()*layout.dpr), bottom=qRound(local.bottom()*layout.dpr);
    const QRect pixels(left, top, right-left, bottom-top);
    return pixels.isValid() && QRect(QPoint(),layout.nativePixels).contains(pixels) ? pixels : QRect{};
}
ScrollCaptureController::ScrollCaptureController(QObject *backend, Request request, Cancel cancel, QObject *parent)
    : QObject(parent), m_backend(backend), m_request(std::move(request)), m_cancel(std::move(cancel))
{
    m_timeout.setSingleShot(true);
    connect(&m_timeout,&QTimer::timeout,this,[this] {
        rejectPending(QStringLiteral("Frame preparation or capture timed out. Accepted frames are unchanged."),true);
    });
    if (backend) connect(backend,&QObject::destroyed,this,[this] {
        m_request={}; m_cancel={};
        this->cancel();
        Q_EMIT failed(QStringLiteral("The capture backend is no longer available."));
    });
}
ScrollCaptureController::~ScrollCaptureController()
{
    const bool active=m_state==State::Capturing;
    m_timeout.stop(); m_pending=0; m_state=State::Cancelled;
    m_request={}; auto abort=std::move(m_cancel);
    if(active && m_backend && abort) abort();
    restore();
}
void ScrollCaptureController::setState(State state)
{
    if (m_state==state) return;
    m_state=state; Q_EMIT stateChanged();
}
bool ScrollCaptureController::begin(const ScrollOutputLayout &layout, const QRectF &selection)
{
    if (m_transition) return false;
    QScopedValueRollback guard(m_transition,true);
    if (!m_backend || !m_request || !m_cancel || m_state==State::Ready || m_state==State::WaitingForHidden || m_state==State::Capturing) return false;
    const auto pixels=scrollSelectionPixels(layout,selection);
    const ScrollCaptureSession::Region region{pixels,layout.id,layout.dpr};
    if (!pixels.isValid() || !m_session.begin(region)) return false;
    m_layout=layout; m_region=region; m_pending=0;
    setState(State::Ready); return true;
}
bool ScrollCaptureController::captureNext()
{
    if (m_transition || m_state!=State::Ready || !m_backend) return false;
    QScopedValueRollback guard(m_transition,true);
    const auto ticket=m_session.requestFrame();
    if (!ticket) return false;
    m_pending=*ticket;
    m_timeout.start(4000);
    setState(State::WaitingForHidden);
    if (m_state!=State::WaitingForHidden || m_pending!=*ticket) return false;
    m_windowsHidden=true;
    Q_EMIT hideWindowsRequested(*ticket);
    return true;
}
bool ScrollCaptureController::confirmHidden(quint64 ticket, const ScrollOutputLayout &current)
{
    if (m_transition || m_state!=State::WaitingForHidden || ticket!=m_pending) return false;
    QScopedValueRollback guard(m_transition,true);
    if (!(current==m_layout) || !m_backend) {
        rejectPending(QStringLiteral("The display configuration changed. Start a new region."),false);
        return false;
    }
    m_timeout.start(8000); setState(State::Capturing);
    if (m_state!=State::Capturing || ticket!=m_pending || !m_backend || !m_request) return false;
    // A backend may synchronously fail; do not reject the same request twice.
    const bool started=m_request(ticket,m_region.physicalRect);
    if (!started && m_pending==ticket && m_state==State::Capturing)
        rejectPending(QStringLiteral("This frame could not be started. Accepted frames are unchanged."),false);
    return started;
}
void ScrollCaptureController::restore()
{
    if (!m_windowsHidden) return;
    m_windowsHidden=false;
    Q_EMIT restoreWindowsRequested();
}
void ScrollCaptureController::rejectPending(const QString &reason, bool abortBackend)
{
    if (!m_pending) return;
    const bool active=m_state==State::Capturing;
    const auto ticket=m_pending; m_pending=0; m_timeout.stop();
    m_session.captureFailed(ticket);
    // Keep new user requests out until abort and restore are complete.
    QScopedValueRollback guard(m_transition,true);
    if (abortBackend && active && m_backend && m_cancel) m_cancel();
    if (m_state==State::Cancelled) return;
    setState(State::Ready);
    restore(); Q_EMIT failed(reason);
}
void ScrollCaptureController::frameFailed(quint64 ticket,const QString &reason)
{
    if (m_state!=State::Capturing || ticket!=m_pending) return;
    rejectPending(reason,false);
}
void ScrollCaptureController::frameCaptured(quint64 ticket, const ScrollOutputLayout &current, const QImage &frame)
{
    if (m_state!=State::Capturing || ticket!=m_pending) return;
    if (!(current==m_layout)) {
        rejectPending(QStringLiteral("The display configuration changed. No pixels were added."),false); return;
    }
    const auto reply=m_session.acceptFrame(ticket,m_region,frame);
    m_pending=0; m_timeout.stop();
    QScopedValueRollback guard(m_transition,true);
    setState(State::Ready); restore();
    if (m_state!=State::Ready) return;
    if (reply.accepted()) { Q_EMIT frameAccepted(m_session.frameCount()); return; }
    QString reason;
    if (reply.stitchError==StitchError::NoProgress) reason=QStringLiteral("No new content. Scroll farther, then capture the next frame.");
    else if (reply.stitchError==StitchError::AmbiguousOverlap || reply.stitchError==StitchError::NoOverlap)
        reason=QStringLiteral("The overlap could not be matched safely. Try a smaller scroll step.");
    else reason=QStringLiteral("This frame does not match the region or output limits. Accepted frames are unchanged.");
    Q_EMIT failed(reason);
}
bool ScrollCaptureController::finish()
{
    if (m_transition || m_state!=State::Ready) return false;
    QScopedValueRollback guard(m_transition,true);
    const auto result=m_session.finish();
    if (result.isNull()) return false;
    setState(State::Finished);
    if (m_state!=State::Finished) return false;
    Q_EMIT resultReady(result); return true;
}
void ScrollCaptureController::cancel()
{
    const bool abort=m_state==State::Capturing;
    m_pending=0; m_timeout.stop(); m_session.cancel();
    setState(State::Cancelled);
    if (abort && m_backend && m_cancel) m_cancel();
    restore();
}
}
