// SPDX-License-Identifier: GPL-3.0-only
#include "ScrollCaptureController.h"
#include <QtMath>
#include <QColorSpace>

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
class ScrollCaptureController::Transition {
public:
    explicit Transition(ScrollCaptureController *controller):m_controller(controller),m_previous(controller->m_transition){controller->m_transition=true;}
    ~Transition(){if(m_controller)m_controller->m_transition=m_previous;}
private:
    QPointer<ScrollCaptureController> m_controller;
    bool m_previous;
};
ScrollCaptureController::ScrollCaptureController(QObject *backend, Request request, Cancel cancel, QObject *parent)
    : QObject(parent), m_backend(backend), m_request(std::move(request)), m_cancel(std::move(cancel))
{
    m_collectionTimer.setSingleShot(true);
    connect(&m_collectionTimer,&QTimer::timeout,this,[this]{
        if(!m_collecting||m_state!=State::Ready)return;
        const QPointer<ScrollCaptureController> self(this);
        if(!captureNext()&&self&&m_collecting)stopCollection();
    });
    m_timeout.setSingleShot(true);
    connect(&m_timeout,&QTimer::timeout,this,[this] {
        rejectPending(QStringLiteral("Frame preparation or capture timed out. Accepted frames are unchanged."),true);
    });
    if (backend) connect(backend,&QObject::destroyed,this,[this] {
        m_request={}; m_cancel={};
        const QPointer<ScrollCaptureController> self(this);
        this->cancel();
        if(!self)return;
        Q_EMIT failed(QStringLiteral("The capture backend is no longer available."));
    });
}
ScrollCaptureController::~ScrollCaptureController()
{
    const bool active=m_state==State::Capturing;
    m_collectionTimer.stop(); m_collecting=false; ++m_collectionEpoch;
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
    Transition guard(this);
    const QPointer<ScrollCaptureController> self(this);
    if (!m_backend || !m_request || !m_cancel || m_state==State::Ready || m_state==State::WaitingForHidden || m_state==State::Capturing) return false;
    const auto pixels=scrollSelectionPixels(layout,selection);
    const ScrollCaptureSession::Region region{pixels,layout.id,layout.dpr};
    if (!pixels.isValid() || !m_session.begin(region)) return false;
    m_layout=layout; m_region=region; m_pending=0; m_lastAccepted=QImage();
    setState(State::Ready); return true;
}
bool ScrollCaptureController::captureNext()
{
    if (m_transition || m_state!=State::Ready || !m_backend) return false;
    Transition guard(this);
    const QPointer<ScrollCaptureController> self(this);
    const auto ticket=m_session.requestFrame();
    if (!ticket) return false;
    m_pending=*ticket;
    m_timeout.start(4000);
    setState(State::WaitingForHidden);
    if (!self || m_state!=State::WaitingForHidden || m_pending!=*ticket) return false;
    m_windowsHidden=true;
    Q_EMIT hideWindowsRequested(*ticket);
    return true;
}
bool ScrollCaptureController::confirmHidden(quint64 ticket, const ScrollOutputLayout &current)
{
    if (m_transition || m_state!=State::WaitingForHidden || ticket!=m_pending) return false;
    Transition guard(this);
    const QPointer<ScrollCaptureController> self(this);
    if (!(current==m_layout) || !m_backend) {
        rejectPending(QStringLiteral("The display configuration changed. Start a new region."),false);
        return false;
    }
    m_timeout.start(8000); setState(State::Capturing);
    if (!self || m_state!=State::Capturing || ticket!=m_pending || !m_backend || !m_request) return false;
    // A backend may synchronously fail; do not reject the same request twice.
    const auto request=m_request;
    const bool started=request(ticket,m_region.physicalRect);
    if(!self)return started;
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
    Transition guard(this);
    const QPointer<ScrollCaptureController> self(this);
    stopCollection();if(!self)return;
    if (abortBackend && active && m_backend && m_cancel) { const auto abort=m_cancel;abort(); }
    if (!self || m_state==State::Cancelled) return;
    setState(State::Ready);if(!self)return;
    restore();if(!self)return; Q_EMIT failed(reason);
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
    const bool automatic=m_collecting;
    const auto epoch=m_collectionEpoch;
    const bool identical=automatic&&!m_lastAccepted.isNull()&&frame.size()==m_lastAccepted.size()
        &&frame.devicePixelRatio()==m_lastAccepted.devicePixelRatio()&&frame.colorSpace()==m_lastAccepted.colorSpace()&&frame==m_lastAccepted;
    ScrollCaptureSession::Reply reply;
    if(identical){m_session.captureFailed(ticket);reply={ScrollCaptureSession::Error::StitchFailed,StitchError::NoProgress};}
    else reply=m_session.acceptFrame(ticket,m_region,frame);
    if(reply.accepted())m_lastAccepted=frame.copy();
    m_pending=0; m_timeout.stop();
    Transition guard(this);const QPointer<ScrollCaptureController> self(this);
    setState(State::Ready);if(!self)return;restore();
    if (!self || m_state!=State::Ready) return;
    if (reply.accepted()) {
        if(m_session.frameCount()>=16){stopCollection();if(!self)return;}
        Q_EMIT frameAccepted(m_session.frameCount());
        if(self&&m_collecting&&epoch==m_collectionEpoch)scheduleCollection(1000);
        return;
    }
    if(automatic&&reply.stitchError==StitchError::NoProgress) {
        Q_EMIT frameSkipped();
        if(self&&m_collecting&&epoch==m_collectionEpoch)scheduleCollection(1000);
        return;
    }
    stopCollection();if(!self)return;
    QString reason;
    if (reply.stitchError==StitchError::NoProgress) reason=QStringLiteral("No new content. Scroll farther, then capture the next frame.");
    else if (reply.stitchError==StitchError::AmbiguousOverlap || reply.stitchError==StitchError::NoOverlap)
        reason=QStringLiteral("The overlap could not be matched safely. Try a smaller scroll step.");
    else reason=QStringLiteral("This frame does not match the region or output limits. Accepted frames are unchanged.");
    Q_EMIT failed(reason);
}
void ScrollCaptureController::scheduleCollection(int delay)
{
    if(m_collecting&&m_state==State::Ready)m_collectionTimer.start(delay);
}
void ScrollCaptureController::stopCollection()
{
    m_collectionTimer.stop();++m_collectionEpoch;
    if(!m_collecting)return;
    m_collecting=false;Q_EMIT collectingChanged();
}
void ScrollCaptureController::setCollecting(bool collecting)
{
    if(collecting==m_collecting)return;
    if(collecting) {
        if(m_transition||m_state!=State::Ready||!m_backend||m_session.frameCount()>=16)return;
        m_collecting=true;const auto epoch=++m_collectionEpoch;
        const QPointer<ScrollCaptureController> self(this);Q_EMIT collectingChanged();
        if(self&&m_collecting&&epoch==m_collectionEpoch)scheduleCollection(0);
        return;
    }
    Transition guard(this);const QPointer<ScrollCaptureController> self(this);
    const bool active=m_state==State::Capturing;const auto ticket=m_pending;
    m_pending=0;m_timeout.stop();if(ticket)m_session.captureFailed(ticket);
    stopCollection();if(!self)return;
    if(active&&m_backend&&m_cancel){const auto abort=m_cancel;abort();}
    if(!self||m_state==State::Cancelled)return;
    if(ticket)setState(State::Ready);
    if(!self)return;
    restore();
}

bool ScrollCaptureController::finish()
{
    if (m_transition || m_state!=State::Ready) return false;
    Transition guard(this);
    const QPointer<ScrollCaptureController> self(this);
    stopCollection();if(!self)return false;
    const auto result=m_session.finish();
    if (result.isNull()) return false;
    setState(State::Finished);
    if (!self || m_state!=State::Finished) return false;
    Q_EMIT resultReady(result); return true;
}
void ScrollCaptureController::cancel()
{
    Transition guard(this);
    const QPointer<ScrollCaptureController> self(this);
    const bool abort=m_state==State::Capturing;
    m_pending=0; m_timeout.stop(); m_session.cancel();m_lastAccepted=QImage();
    stopCollection();if(!self)return;
    setState(State::Cancelled);if(!self)return;
    if (abort && m_backend && m_cancel){const auto cancel=m_cancel;cancel();}
    if(self)restore();
}
}
