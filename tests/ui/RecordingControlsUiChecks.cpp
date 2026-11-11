// SPDX-License-Identifier: GPL-3.0-only
// Test-only session-bus host. No native panel, stream, or desktop capture.
#include "RecordingControlsUiChecks.h"
#include "SpectacleCore.h"
#include "RecordingPreparation.h"
#include "Platforms/StatusNotifierAvailability.h"
#include "ShortcutActions.h"
#include "CaptureModeModel.h"
#include "Gui/ViewerWindow.h"
#include "Gui/InlineMessageModel.h"
#include <KQuickImageEditor/AnnotationDocument>
#include <QApplication>
#include <QAction>
#include <QQuickItem>
#include <QClipboard>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusContext>
#include <QDBusPendingReply>
#include <QDBusArgument>
#include <QDBusServiceWatcher>
#include <QSignalSpy>
#include <QScopeGuard>
#include <QtTest/QTest>

class RecordingTestWatcher final : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.StatusNotifierWatcher")
    Q_PROPERTY(bool IsStatusNotifierHostRegistered READ available)
    Q_PROPERTY(QStringList RegisteredStatusNotifierItems READ items)
    Q_PROPERTY(int ProtocolVersion READ version)
public:
    RecordingTestWatcher() : serviceWatcher(this) {
        serviceWatcher.setConnection(QDBusConnection::sessionBus());
        serviceWatcher.setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
        connect(&serviceWatcher,&QDBusServiceWatcher::serviceUnregistered,this,[this](const QString &service) {
            const auto old=registered;
            for(const auto &item:old) if(item.startsWith(service+u'/')) {
                registered.removeAll(item); Q_EMIT StatusNotifierItemUnregistered(item);
            }
            serviceWatcher.removeWatchedService(service);
        });
    }
    QDBusServiceWatcher serviceWatcher;
    bool host = true;
    QStringList registered;
    bool available() const { return host; }
    QStringList items() const { return registered; }
    int version() const { return 0; }
    void loseHost() { host=false; Q_EMIT StatusNotifierHostUnregistered(); }
    void restoreHost() { host=true; Q_EMIT StatusNotifierHostRegistered(); }
public Q_SLOTS:
    void RegisterStatusNotifierItem(const QString &service)
    {
        const QString item=service.startsWith(u'/') ? QString(message().service()+service) : QString(service+QStringLiteral("/StatusNotifierItem"));
        if (!registered.contains(item)) registered.append(item);
        Q_EMIT StatusNotifierItemRegistered(item);
    }
    void RegisterStatusNotifierHost(const QString &) { restoreHost(); }
Q_SIGNALS:
    void StatusNotifierHostRegistered();
    void StatusNotifierHostUnregistered();
    void StatusNotifierItemRegistered(const QString &item);
    void StatusNotifierItemUnregistered(const QString &item);
};

namespace {
// Controlled acknowledgement source for Core/QML dispatch only. This does not
// create a stream or claim to validate the encoder's audio/video timeline.
class PauseControlsPlatform final : public VideoPlatform
{
public:
    int pauses=0, resumes=0, stops=0;
    bool pending=false, stopping=false;
    bool supportsPause() const override { return true; }
    bool pausePending() const override { return pending; }
    RecordingModes supportedRecordingModes() const override { return Region; }
    Formats supportedFormats() const override { return MP4_H264; }
    bool isRecordingAudio() const override { return false; }
    void startRecording(const QUrl &,RecordingMode,const QVariantMap &,bool) override {}
    void begin() { setRecordingMode(Region); setRecordingState(RecordingState::Recording); }
    void pauseRecording() override {
        if(stopping||pending||recordingState()!=RecordingState::Recording)return;
        ++pauses;pending=true;Q_EMIT pausePendingChanged();
    }
    void resumeRecording() override {
        if(stopping||pending||!isPaused())return;
        ++resumes;pending=true;Q_EMIT pausePendingChanged();
    }
    void acknowledge(RecordingState state) {
        if(stopping)return;
        pending=false;setRecordingState(state);Q_EMIT pausePendingChanged();
    }
    void finishRecording() override {
        if(stopping||!isRecording())return;
        ++stops;stopping=true;pending=false;Q_EMIT pausePendingChanged();
        setRecordingState(RecordingState::Rendering);
    }
    void idle() { setRecordingState(RecordingState::NotRecording); }
};
// A preguard-only substitute: never connected to Core or treated as capable.
class DrainingControlsPlatform final : public VideoPlatform
{
public:
    explicit DrainingControlsPlatform(bool rendering) {
        if(rendering)setRecordingState(RecordingState::Rendering);
    }
    bool startCalled=false;
    RecordingModes supportedRecordingModes() const override {return NoRecordingModes;}
    Formats supportedFormats() const override {return NoFormat;}
    bool isRecordingAudio() const override {return false;}
    bool readyForRecording() const override {return false;}
    void startRecording(const QUrl &,RecordingMode,const QVariantMap &,bool) override {startCalled=true;}
    void finishRecording() override {}
};
}

bool RecordingControlsUiChecks::run(QApplication &application, const QString &screenshot, QString *failure)
{
    const auto fail=[&](const QString &message){if(failure)*failure=message;return false;};
    auto core=SpectacleCore::instance();
    if(!core||!ViewerWindow::instance()||core->videoPlatform()->supportedRecordingModes()!=VideoPlatform::NoRecordingModes)
        return fail(QStringLiteral("Requires real Null platform and visible viewer"));
    auto bus=QDBusConnection::sessionBus();
    const QString watcherService=QStringLiteral("org.kde.StatusNotifierWatcher");
    if(!bus.isConnected()||bus.interface()->isServiceRegistered(watcherService))
        return fail(QStringLiteral("Requires isolated session bus without another watcher"));
    RecordingTestWatcher watcher;
    if(!bus.registerObject(QStringLiteral("/StatusNotifierWatcher"),&watcher,QDBusConnection::ExportAllSlots|QDBusConnection::ExportAllSignals|QDBusConnection::ExportAllProperties)
        ||!bus.registerService(watcherService))
        return fail(QStringLiteral("Could not register test-only StatusNotifierWatcher"));
    const auto cleanup=qScopeGuard([&]{
        core->cancelPendingRecording(); core->cancelScreenshot();
        bus.unregisterService(watcherService);bus.unregisterObject(QStringLiteral("/StatusNotifierWatcher"));
    });
    using Prep=NkShot::RecordingPreparation;
    auto prep=core->videoPlatform()->preparation();
    const auto document=core->annotationDocument();
    const QImage image=document->baseImage();
    const auto depth=document->undoStackDepth();
    const bool videoMode=core->videoMode();
    const auto video=core->currentVideo();
    const auto geometry=ViewerWindow::instance()->geometry();
    const auto mode=ViewerWindow::instance()->mode();
    const QString clipboard=application.clipboard()->text();
    QSignalSpy failed(core->videoPlatform(),&VideoPlatform::recordingFailed);
    const auto unchanged=[&]{
        auto viewer=ViewerWindow::instance();
        return viewer&&viewer->isVisible()&&viewer->mode()==mode&&viewer->size()==geometry.size()
            &&core->videoMode()==videoMode&&core->currentVideo()==video
            &&document->baseImage()==image&&document->undoStackDepth()==depth
            &&application.clipboard()->text()==clipboard&&failed.isEmpty();
    };
    const auto begin=[&](Prep::Phase phase)->quint64 {
        core->snapshotRecordingPreparation();
        core->m_recordingControlRequired=true;
        core->m_lastRecordingMode=VideoPlatform::Region;
        const auto token=prep->begin(phase==Prep::Starting?0:10000);
        if(phase!=Prep::Selecting)prep->targetReady(token);
        return token;
    };
    // Await real host registration/availability processing before asking for a
    // pending item. The assertion below requires the actual item registration.
    if(!QTest::qWaitFor([&]{return core->m_statusNotifierAvailability && core->m_statusNotifierAvailability->hostAvailable();},2000))
        return fail(QStringLiteral("Real Core availability monitor did not observe test host"));
    if(qEnvironmentVariableIsSet("NKSHOT_TEST_RECORDING_PAUSE")) {
        // CONSTANT videoPlatform bindings require a new real viewer after the
        // swap. Restore the actual platform only after destroying fixture QML.
        core->deleteWindows();
        auto actualPlatform=std::move(core->m_videoPlatform);
        auto controlled=std::make_unique<PauseControlsPlatform>();
        auto probe=controlled.get();
        core->m_videoPlatform=std::move(controlled);
        const auto restore=qScopeGuard([&]{
            probe->idle();
            core->deleteWindows();
            core->m_videoPlatform=std::move(actualPlatform);
            core->m_videoMode=videoMode;
            Q_EMIT core->videoModeChanged(core->m_videoMode);
            core->initViewerWindow(mode);
            ViewerWindow::instance()->setGeometry(geometry);
            ViewerWindow::instance()->show();
        });
        QObject::connect(probe,&VideoPlatform::recordingStateChanged,core,&SpectacleCore::updateRecordingStateUi);
        QObject::connect(probe,&VideoPlatform::pausePendingChanged,core,&SpectacleCore::updateRecordingPauseTray);
        QObject::connect(probe,&VideoPlatform::recordedTimeChanged,core,&SpectacleCore::recordedTimeChanged);
        core->m_videoMode=true;
        Q_EMIT core->videoModeChanged(core->m_videoMode);
        core->initViewerWindow(ViewerWindow::Viewer);
        auto viewer=ViewerWindow::instance();
        viewer->resize(qEnvironmentVariableIsSet("NKSHOT_TEST_COMPACT")?QSize(420,520):QSize(760,540));
        viewer->show();
        const auto invariant=[&]{return document->baseImage()==image&&document->undoStackDepth()==depth
            &&core->currentVideo()==video&&application.clipboard()->text()==clipboard;};
        const auto find=[&](const QString &name){return viewer->rootObject()->findChild<QQuickItem *>(name);};
        const auto click=[&](const QString &name) {
            QQuickItem *control=nullptr;
            if(!QTest::qWaitFor([&]{control=find(name);return control&&control->isVisible()&&control->isEnabled()
                    &&control->width()>10&&control->height()>10;},1500))return false;
            const auto point=control->mapToScene(QPointF(control->width()/2,control->height()/2));
            if(!QRectF(QPointF(),viewer->size()).contains(point))return false;
            QTest::mouseClick(viewer,Qt::LeftButton,Qt::NoModifier,point.toPoint());return true;
        };
        const auto shot=[&](const QString &suffix) {
            QSignalSpy frame(viewer,&QQuickWindow::frameSwapped);viewer->update();
            return QTest::qWaitFor([&]{return !frame.isEmpty();},1000)&&viewer->grabWindow().save(screenshot+suffix);
        };
        const auto properties=[&](const QString &item) {
            const auto slash=item.indexOf(u'/');
            auto call=QDBusMessage::createMethodCall(item.left(slash),item.mid(slash),
                QStringLiteral("org.freedesktop.DBus.Properties"),QStringLiteral("GetAll"));
            call.setArguments({QStringLiteral("org.kde.StatusNotifierItem")});
            auto pending=bus.asyncCall(call,1000);
            if(!QTest::qWaitFor([&]{return pending.isFinished();},1500))return QVariantMap{};
            const auto reply=pending.reply();
            if(reply.type()!=QDBusMessage::ReplyMessage||reply.arguments().isEmpty()) {
                qWarning()<<"Pause SNI properties"<<item<<reply.errorName()<<reply.errorMessage();return QVariantMap{};
            }
            return qdbus_cast<QVariantMap>(reply.arguments().first());
        };
        const auto tooltipTitle=[](const QVariantMap &values) {
            const auto arg=values.value(QStringLiteral("ToolTip")).value<QDBusArgument>();
            QString icon,title,subtitle;
            if(arg.currentSignature()!=QStringLiteral("(sa(iiay)ss)"))return title;
            arg.beginStructure();arg>>icon;arg.beginArray();
            while(!arg.atEnd()){int w,h;QByteArray bytes;arg.beginStructure();arg>>w>>h>>bytes;arg.endStructure();}
            arg.endArray();arg>>title>>subtitle;arg.endStructure();return title;
        };
        QSignalSpy registrations(&watcher,&RecordingTestWatcher::StatusNotifierItemRegistered);
        probe->begin();
        QString item;
        QElapsedTimer search;search.start();
        while(item.isEmpty()&&search.elapsed()<2500) {
            for(const auto &candidate:watcher.registered) {
                const auto data=properties(candidate);
                if(data.value(QStringLiteral("Status")).toString()==QStringLiteral("Active")
                    &&tooltipTitle(data).contains(QStringLiteral("Recording"),Qt::CaseInsensitive)) {item=candidate;break;}
            }
            if(item.isEmpty())QTest::qWait(20);
        }
        if(item.isEmpty())return fail(QStringLiteral("Controlled Recording did not publish actual active SNI"));
        const int initialRegistrations=registrations.size();
        auto pauseAction=core->m_recordingPauseAction;
        if(!pauseAction||pauseAction->objectName()!=QStringLiteral("nkshotPauseRecording")||!pauseAction->isEnabled())
            return fail(QStringLiteral("Actual SNI Pause action unavailable"));
        QTest::qWait(150);
        if(!click(QStringLiteral("nkshotRecordingPause"))||probe->pauses!=1||!probe->pausePending()||probe->isPaused())
            return fail(QStringLiteral("Actual QML Pause did not request acknowledgement"));
        if(!QTest::qWaitFor([&]{return !find(QStringLiteral("nkshotRecordingPause"))->isEnabled();},500)
            ||pauseAction->isEnabled()||!find(QStringLiteral("nkshotRecordingStop"))->isEnabled())
            return fail(QStringLiteral("Pending pause did not disable Pause while preserving Stop"));
        if(!shot(QStringLiteral(".pending.png")))return fail(QStringLiteral("Pending screenshot failed"));
        probe->acknowledge(VideoPlatform::RecordingState::Paused);
        const auto frozen=probe->recordedTime();QTest::qWait(1100);
        if(frozen<100||probe->recordedTime()!=frozen||!probe->isRecording()||!probe->isPaused()
            ||!tooltipTitle(properties(item)).contains(QStringLiteral("paused"),Qt::CaseInsensitive))
            return fail(QStringLiteral("Pause acknowledgement did not freeze time/update same SNI"));
        if(!shot(QStringLiteral(".paused.png")))return fail(QStringLiteral("Paused screenshot failed"));
        // Trigger the real item menu's QAction, not a direct Core slot.
        if(!pauseAction->isEnabled()||!pauseAction->text().contains(QStringLiteral("Resume")))
            return fail(QStringLiteral("Actual SNI menu did not become Resume"));
        pauseAction->trigger();
        if(probe->resumes!=1||!probe->pausePending()||!probe->isPaused())return fail(QStringLiteral("SNI Resume skipped acknowledgement"));
        probe->acknowledge(VideoPlatform::RecordingState::Recording);
        QTest::qWait(150);
        if(!click(QStringLiteral("nkshotRecordingPause")))return fail(QStringLiteral("Second QML Pause unavailable"));
        probe->acknowledge(VideoPlatform::RecordingState::Paused);
        if(probe->recordedTime()<frozen+100||registrations.size()!=initialRegistrations
            ||core->m_recordingPauseAction!=pauseAction||properties(item).isEmpty())
            return fail(QStringLiteral("Resume reset time or recreated actual SNI"));
        // Also exercise the real QML Resume button, then pause through SNI.
        if(!click(QStringLiteral("nkshotRecordingPause"))||probe->resumes!=2||!probe->pausePending())
            return fail(QStringLiteral("Actual QML Resume did not dispatch"));
        probe->acknowledge(VideoPlatform::RecordingState::Recording);
        pauseAction->trigger();probe->acknowledge(VideoPlatform::RecordingState::Paused);
        if(!shot(QString())||!invariant())return fail(QStringLiteral("Paused UI evidence/source isolation failed"));
        if(!click(QStringLiteral("nkshotRecordingStop"))||probe->stops!=1
            ||probe->recordingState()!=VideoPlatform::RecordingState::Rendering)
            return fail(QStringLiteral("Actual QML Stop while Paused did not finish"));
        if(!QTest::qWaitFor([&]{return find(QStringLiteral("nkshotRecordingFinalizing"))
            &&find(QStringLiteral("nkshotRecordingFinalizing"))->isVisible()
            &&!find(QStringLiteral("nkshotRecordingStop"))->isVisible();},500)
            ||!shot(QStringLiteral(".finishing.png")))
            return fail(QStringLiteral("Finalizing did not replace capture actions with safe progress feedback"));
        probe->acknowledge(VideoPlatform::RecordingState::Recording);
        if(probe->isRecording()||core->m_recordingTrayAction->isEnabled()||!invariant())
            return fail(QStringLiteral("Late acknowledgement resurrected stopped UI or altered source"));
        const int retiredPauses=probe->pauses, retiredResumes=probe->resumes;
        QPointer<QAction> retiredPause=pauseAction;
        probe->idle(); // synchronously retire, before deleteLater delivery
        if(retiredPause)retiredPause->trigger();
        if(core->m_recordingPauseAction||probe->pauses!=retiredPauses||probe->resumes!=retiredResumes)
            return fail(QStringLiteral("Retired Pause QAction remained live or dispatched a request"));
        qInfo()<<"084 controlled Core/QML/SNI pause dispatch passed; no native encoder or saved-media claim";
        return true;
    }
    const auto first=begin(Prep::Countdown);
    if(!QTest::qWaitFor([&]{return !watcher.registered.isEmpty();},2000))
        return fail(QStringLiteral("Core pending notifier never registered with real test bus host"));
    if(!prep->isCurrent(first))return fail(QStringLiteral("Pending request canceled despite registered host"));
    core->cancelPendingRecording();
    if(!QTest::qWaitFor(unchanged,1000))return fail(QStringLiteral("Initial notifier cancellation changed prior state"));
    const QList<QAction*> actions{ShortcutActions::self()->recordRegionAction(),ShortcutActions::self()->recordScreenAction(),ShortcutActions::self()->recordWindowAction()};
    // No event processing while swapped: retain the actual connected platform,
    // verify the public action preguard, then immediately restore ownership.
    for(bool rendering:{false,true}) {
        const auto originalStartMode=core->m_startMode;
        const auto originalGeometry=ViewerWindow::instance()->geometry();
        auto actualPlatform=std::move(core->m_videoPlatform);
        auto draining=std::make_unique<DrainingControlsPlatform>(rendering);
        auto probe=draining.get();
        core->m_videoPlatform=std::move(draining);
        bool intact=true;
        for(auto action:actions) {
            core->activateAction(action->objectName(),QVariant());
            intact=intact&&!probe->startCalled&&core->m_startMode==originalStartMode
                &&core->videoMode()==videoMode&&core->currentVideo()==video
                &&ViewerWindow::instance()&&ViewerWindow::instance()->geometry()==originalGeometry
                &&document->baseImage()==image&&document->undoStackDepth()==depth
                &&application.clipboard()->text()==clipboard;
        }
        core->m_videoPlatform=std::move(actualPlatform);
        if(!intact)return fail(rendering?QStringLiteral("Rendering record shortcuts altered the user's existing flow")
            :QStringLiteral("Not-ready draining record shortcuts altered the user's existing flow"));
    }
    for(auto phase:{Prep::Selecting,Prep::Countdown,Prep::Starting})for(auto action:actions){
        const auto token=begin(phase);
        if(!QTest::qWaitFor([&]{return prep->phase()==phase;},1000))
            return fail(QStringLiteral("Pending phase not reached"));
        core->activateAction(action->objectName(),QVariant());
        if(!QTest::qWaitFor([&]{return prep->phase()==Prep::Idle&&unchanged();},1000))
            return fail(QStringLiteral("Record shortcut failed cancel-only pending contract"));
        prep->targetReady(token);
        if(prep->phase()!=Prep::Idle)return fail(QStringLiteral("Old target resurrected canceled token"));
    }
    const auto dbusCall=[&](const QString &item,const QString &interface,const QString &method,const QVariantList &args)->QDBusMessage{
        const auto slash=item.indexOf(u'/');
        if(slash<0)return {};
        auto call=QDBusMessage::createMethodCall(item.left(slash),item.mid(slash),interface,method);
        call.setArguments(args);auto pending=bus.asyncCall(call,1000);
        if(!QTest::qWaitFor([&]{return pending.isFinished();},1500))return {};
        return pending.reply();
    };
    begin(Prep::Starting);
    if(!QTest::qWaitFor([&]{return prep->phase()==Prep::Starting;},1500))
        return fail(QStringLiteral("Starting phase unavailable"));
    // Rapid cancellation may leave earlier Register messages queued. Select by
    // live D-Bus state, not list position or registration count.
    QString item;
    QVariantMap values;
    QString title,subtitle;
    QElapsedTimer lookup;lookup.start();
    while(item.isEmpty()&&lookup.elapsed()<2500) {
        const auto candidates=watcher.registered;
        for(const auto &candidate:candidates) {
            const auto properties=dbusCall(candidate,QStringLiteral("org.freedesktop.DBus.Properties"),QStringLiteral("GetAll"),{QStringLiteral("org.kde.StatusNotifierItem")});
            if(properties.type()!=QDBusMessage::ReplyMessage||properties.arguments().isEmpty()) {
                qWarning()<<"SNI GetAll rejected"<<candidate<<properties.errorName()<<properties.errorMessage()
                    <<"registered"<<watcher.registered;
                continue;
            }
            auto candidateValues=qdbus_cast<QVariantMap>(properties.arguments().first());
            const auto tooltip=candidateValues.value(QStringLiteral("ToolTip")).value<QDBusArgument>();
            if(tooltip.currentSignature()!=QStringLiteral("(sa(iiay)ss)")) {
                qWarning()<<"Unexpected actual SNI tooltip signature"<<candidate<<tooltip.currentSignature();
                continue;
            }
            QString icon,candidateTitle,candidateSubtitle;
            tooltip.beginStructure();tooltip>>icon;tooltip.beginArray();
            while(!tooltip.atEnd()){int width,height;QByteArray bytes;tooltip.beginStructure();tooltip>>width>>height>>bytes;tooltip.endStructure();}
            tooltip.endArray();tooltip>>candidateTitle>>candidateSubtitle;tooltip.endStructure();
            qInfo()<<"Actual SNI candidate"<<candidate<<candidateValues.value(QStringLiteral("Status"))<<candidateTitle<<candidateSubtitle;
            if(candidateValues.value(QStringLiteral("Status")).toString()==QStringLiteral("Active")
                &&candidateTitle.contains(QStringLiteral("Starting"),Qt::CaseInsensitive)) {
                item=candidate;values=candidateValues;title=candidateTitle;subtitle=candidateSubtitle;break;
            }
        }
        if(item.isEmpty())QTest::qWait(25);
    }
    if(item.isEmpty())return fail(QStringLiteral("No live Active Starting notifier; see actual DBus reply diagnostics"));
    qInfo()<<"Synthetic Starting SNI"<<item<<values.value(QStringLiteral("Title"))<<title<<subtitle<<values.value(QStringLiteral("Status"));
    const auto activated=dbusCall(item,QStringLiteral("org.kde.StatusNotifierItem"),QStringLiteral("Activate"),{0,0});
    if(activated.type()!=QDBusMessage::ReplyMessage||!QTest::qWaitFor([&]{return prep->phase()==Prep::Idle&&unchanged();},1000))
        return fail(QStringLiteral("Actual DBus Activate did not cancel Starting"));
    dbusCall(item,QStringLiteral("org.kde.StatusNotifierItem"),QStringLiteral("Activate"),{0,0});
    if(prep->phase()!=Prep::Idle)return fail(QStringLiteral("Repeated old Activate reopened recording"));
    begin(Prep::Countdown);
    if(!core->m_recordingTrayAction || !core->m_recordingTrayAction->isEnabled()
        || !core->m_recordingTrayAction->text().contains(QStringLiteral("Cancel"),Qt::CaseInsensitive))
        return fail(QStringLiteral("Actual pending tray QAction is not an enabled Cancel action"));
    core->m_recordingTrayAction->trigger();
    if(!QTest::qWaitFor([&]{return prep->phase()==Prep::Idle&&unchanged();},1000))
        return fail(QStringLiteral("Actual tray Cancel QAction did not restore viewer"));
    begin(Prep::Countdown);
    watcher.loseHost();
    if(!QTest::qWaitFor([&]{return prep->phase()==Prep::Idle&&unchanged();},1500))
        return fail(QStringLiteral("Lost host did not cancel and restore pending recording"));
    InlineMessageModel::instance()->clear();
    begin(Prep::Starting);
    if(!QTest::qWaitFor([&]{return prep->phase()==Prep::Idle&&unchanged();},1500))
        return fail(QStringLiteral("Missing host did not cancel Starting"));
    {
        auto restoredViewer=ViewerWindow::instance();
        auto notifications=restoredViewer->rootObject()->findChild<QQuickItem *>(QStringLiteral("nkshotNotifications"));
        if(!QTest::qWaitFor([&]{
                if(!notifications||!notifications->isVisible()||notifications->height()<20)return false;
                const auto extent=qMin(notifications->property("contentHeight").toReal(),notifications->property("maximumHeight").toReal());
                return qAbs(notifications->height()-extent)<1
                    &&restoredViewer->rootObject()->size()==QSizeF(restoredViewer->size());
            },1500))return fail(QStringLiteral("Actual missing-host warning did not settle visibly"));
        QSignalSpy warningFrame(restoredViewer,&QQuickWindow::frameSwapped);
        restoredViewer->update();
        if(!QTest::qWaitFor([&]{return !warningFrame.isEmpty();},1000)
            ||!restoredViewer->grabWindow().save(screenshot+QStringLiteral(".unavailable.png")))
            return fail(QStringLiteral("Could not capture actual missing-host recovery warning"));
    }
    watcher.restoreHost();
    if(!QTest::qWaitFor([&]{return core->m_statusNotifierAvailability->hostAvailable();},1500))
        return fail(QStringLiteral("Host return was not observed"));
    begin(Prep::Countdown);
    core->takeNewScreenshot(int(CaptureModeModel::AllScreens),99000,false,true,true);
    if(prep->phase()!=Prep::Idle)return fail(QStringLiteral("New screenshot intent left pending recording"));
    core->cancelScreenshot();
    if(!QTest::qWaitFor(unchanged,1500))return fail(QStringLiteral("New screenshot cancellation failed original-state restore"));
    QSignalSpy frame(ViewerWindow::instance(),&QQuickWindow::frameSwapped);
    ViewerWindow::instance()->update();
    if(!QTest::qWaitFor([&]{return !frame.isEmpty();},1000)||!ViewerWindow::instance()->grabWindow().save(screenshot))
        return fail(QStringLiteral("Could not render restored synthetic controls evidence"));
    return true;
}
#include "RecordingControlsUiChecks.moc"
