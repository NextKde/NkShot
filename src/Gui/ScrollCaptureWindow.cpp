// SPDX-License-Identifier: GPL-3.0-only
#include "ScrollCaptureWindow.h"
#include "Platforms/ScrollFrameKWin.h"
#include "PinnedImageWindow.h"
#include "ViewerWindow.h"
#include "OcrTextWindow.h"
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QScreen>
#include <QGuiApplication>
#include <QEvent>
#include <QShortcut>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <KLocalizedString>
using NkShot::Imaging::ScrollCaptureController;
ScrollCaptureWindow::ScrollCaptureWindow(QQmlEngine *engine)
    : QQuickView(engine,nullptr),m_engine(engine),m_backend(new ScrollFrameKWin(this)),
      m_controller(new ScrollCaptureController(m_backend,[this](quint64 ticket,QRect rect){return m_backend->request(ticket,m_screen,rect);},[this]{m_backend->cancel();},this))
{
    setObjectName(QStringLiteral("nkshotScrollPreview"));
    setTitle(i18nc("@title:window","Scrolling capture • Technical preview"));
    setFlags(Qt::Window|Qt::WindowStaysOnTopHint);
    setResizeMode(SizeRootObjectToView); setMinimumSize({360,260}); resize(460,300);
    for(auto screen:QGuiApplication::screens()) {m_screens.append(screen);m_outputs.append(screen->name().isEmpty()?i18nc("@item","Display %1",m_screens.size()):screen->name());}
    if(!m_screens.isEmpty()) m_screen=m_screens.first();
    connect(qGuiApp,&QGuiApplication::screenRemoved,this,[this](QScreen *screen){if(screen==m_screen) cancelPreview();});
    m_status=i18nc("@info","Checking compositor window exclusion…");
    loadView(this);
    new QShortcut(QKeySequence(Qt::Key_Escape),this,this,&ScrollCaptureWindow::cancelPreview);
    connect(m_backend,&ScrollFrameKWin::frameReady,this,[this](quint64 ticket,const QImage &image){m_controller->frameCaptured(ticket,currentLayout(),image);});
    connect(m_backend,&ScrollFrameKWin::requestFailed,m_controller,&ScrollCaptureController::frameFailed);
    connect(m_controller,&ScrollCaptureController::hideWindowsRequested,this,[this](quint64 ticket){
        // Preparation is not a timed repaint claim: the dedicated backend freshly
        // requires API5 and filters this process in the compositor render itself.
        QMetaObject::invokeMethod(this,[this,ticket]{m_controller->confirmHidden(ticket,currentLayout());},Qt::QueuedConnection);
    });
    connect(m_controller,&ScrollCaptureController::stateChanged,this,&ScrollCaptureWindow::changed);
    connect(m_controller,&ScrollCaptureController::failed,this,&ScrollCaptureWindow::setStatus);
    connect(m_controller,&ScrollCaptureController::frameAccepted,this,[this](int count){
        setStatus(i18ncp("@info","%1 frame accepted. Scroll the target, then capture next.","%1 frames accepted. Scroll the target, then capture next.",count));
    });
    connect(m_controller,&ScrollCaptureController::resultReady,this,[this](const QImage &image){
        m_result=image; showResult();
    });
    connect(QCoreApplication::instance(),&QCoreApplication::aboutToQuit,this,[this]{delete this;});
    if(!QDBusConnection::sessionBus().isConnected()) {
        setStatus(i18nc("@info","Unavailable: connect to a KDE desktop with KWin ScreenShot2 API 5 or later. No screenshot has been taken."));
        return;
    }
    auto query=QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin.ScreenShot2"),QStringLiteral("/org/kde/KWin/ScreenShot2"),QStringLiteral("org.freedesktop.DBus.Properties"),QStringLiteral("Get"));
    query.setArguments({QStringLiteral("org.kde.KWin.ScreenShot2"),QStringLiteral("Version")});
    auto watcher=new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(query,1000),this);
    connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,watcher]{
        const QDBusPendingReply<QDBusVariant> reply=*watcher; watcher->deleteLater();
        bool ok=false; const uint version=reply.isError()?0:reply.value().variant().toUInt(&ok);
        m_available=ok && version>=5;
        setStatus(m_available?i18nc("@info","API 5 window exclusion is available. Select one display region. This preview still needs native desktop acceptance testing.")
            :i18nc("@info","Unavailable: safe window exclusion requires KWin ScreenShot2 API 5 or later. No delay-based fallback is used."));
    });
}
void ScrollCaptureWindow::loadView(QQuickView *view)
{
    auto context=new QQmlContext(m_engine->rootContext(),view);
    context->setContextProperty(QStringLiteral("scrollWindow"),this);
    const QUrl source(QStringLiteral("qrc:/qt/qml/org/kde/spectacle/private/Gui/ScrollCapture.qml"));
    auto component=new QQmlComponent(m_engine,source,view);
    view->setContent(source,component,component->create(context));
}
ScrollCaptureWindow::~ScrollCaptureWindow()
{
    m_controller->cancel(); delete m_overlay.data(); restoreWindows();
}
void ScrollCaptureWindow::setStatus(const QString &text) {m_status=text;Q_EMIT changed();}
void ScrollCaptureWindow::setOutputIndex(int index)
{
    if(m_phase || index<0 || index>=m_screens.size()) return;
    m_outputIndex=index; m_screen=m_screens[index]; Q_EMIT changed();
}
bool ScrollCaptureWindow::busy() const
{
    const auto state=m_controller->state();
    return state==ScrollCaptureController::State::WaitingForHidden || state==ScrollCaptureController::State::Capturing;
}
int ScrollCaptureWindow::frameCount() const {return m_controller->frameCount();}
NkShot::Imaging::ScrollOutputLayout ScrollCaptureWindow::currentLayout() const
{
    if(!m_screen || !QGuiApplication::screens().contains(m_screen)) return {};
    const auto g=m_screen->geometry(); const auto d=m_screen->devicePixelRatio();
    return {m_screen->name(),g,QSize(qRound(g.width()*d),qRound(g.height()*d)),d};
}
void ScrollCaptureWindow::selectRegion()
{
    if(!m_available || m_phase || !m_screen) return;
    if(QGuiApplication::modalWindow()) {setStatus(i18nc("@info","Close the open dialog before selecting a region."));return;}
    for(auto w:QGuiApplication::allWindows()) {
        if(w!=this && w->isVisible() && w->visibility()!=QWindow::Minimized
           && (qobject_cast<ViewerWindow *>(w)||qobject_cast<PinnedImageWindow *>(w)||qobject_cast<OcrTextWindow *>(w))) {m_hidden.append({w,w->visibility()});w->hide();}
    }
    m_phase=1;
    m_status=i18nc("@info","Drag a region on this display. Esc cancels. Keep fixed headers and changing content outside the region.");
    Q_EMIT changed(); hide();
    auto overlay=new QQuickView(m_engine,nullptr);m_overlay=overlay;
    overlay->setObjectName(QStringLiteral("nkshotScrollSelector"));
    overlay->setResizeMode(SizeRootObjectToView);overlay->setColor(Qt::transparent);
    overlay->setFlags(Qt::Tool|Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint);
    overlay->setScreen(m_screen);overlay->setGeometry(m_screen->geometry());
    loadView(overlay);
    if(overlay->status()!=QQuickView::Ready) {
        m_overlay=nullptr;overlay->deleteLater();m_phase=0;restoreWindows();
        setStatus(i18nc("@info","The region selector could not open. Your original windows have been restored."));show();return;
    }
    overlay->installEventFilter(this);
    new QShortcut(QKeySequence(Qt::Key_Escape),overlay,this,&ScrollCaptureWindow::cancelPreview);
    overlay->showFullScreen();overlay->requestActivate();
}
void ScrollCaptureWindow::acceptRegion(const QRectF &local)
{
    if(m_phase!=1 || !m_screen) return;
    const auto layout=currentLayout();
    if(!m_controller->begin(layout,local.translated(layout.logicalGeometry.topLeft()))) {
        setStatus(i18nc("@info","This region is too small, too large, or no longer on this display. Drag a new region."));return;
    }
    m_phase=2;
    if(m_overlay) {auto overlay=m_overlay.data();m_overlay=nullptr;overlay->removeEventFilter(this);overlay->close();overlay->deleteLater();}
    setStatus(i18nc("@info","Capture the first frame, then scroll the target yourself. Finish creates a separate image; the original stays unchanged."));
    show();raise();requestActivate();
}
void ScrollCaptureWindow::captureNext()
{
    if(m_phase==2 && m_controller->captureNext()) setStatus(i18nc("@info","Capturing with compositor window exclusion…"));
}
void ScrollCaptureWindow::showResult()
{
    auto pin=new PinnedImageWindow(m_result,m_engine);
    if(pin->status()!=QQuickView::Ready) {
        delete pin;setStatus(i18nc("@info","The result preview could not open. Your result is retained; choose Finish to retry."));return;
    }
    close();pin->show();
}
void ScrollCaptureWindow::finishCapture() {if(m_phase==2) {if(!m_result.isNull()) showResult();else m_controller->finish();}}
void ScrollCaptureWindow::restoreWindows() {for(const auto &entry:m_hidden) if(entry.first) entry.first->setVisibility(entry.second); m_hidden.clear();}
void ScrollCaptureWindow::cancelPreview() {close();}
bool ScrollCaptureWindow::event(QEvent *event)
{
    const bool result=QQuickView::event(event);
    if(event->type()==QEvent::Close && event->isAccepted()) {
        m_controller->cancel();m_phase=0;
        if(m_overlay) {auto overlay=m_overlay.data();m_overlay=nullptr;overlay->removeEventFilter(this);overlay->close();overlay->deleteLater();}
        restoreWindows();deleteLater();
    }
    return result;
}
bool ScrollCaptureWindow::eventFilter(QObject *object,QEvent *event)
{
    if(object==m_overlay && event->type()==QEvent::Close && m_phase==1) QMetaObject::invokeMethod(this,&ScrollCaptureWindow::cancelPreview,Qt::QueuedConnection);
    return QQuickView::eventFilter(object,event);
}
