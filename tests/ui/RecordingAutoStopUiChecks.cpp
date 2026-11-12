// SPDX-License-Identifier: GPL-3.0-only
// Synthetic acknowledgements and virtual recorded-time samples; no native capture.
#include "RecordingAutoStopUiChecks.h"
#include "SpectacleCore.h"
#include "RecordingPreparation.h"
#include "Platforms/ImagePlatform.h"
#include "Gui/ViewerWindow.h"
#include "settings.h"
#include <KQuickImageEditor/AnnotationDocument>
#include <QApplication>
#include <QClipboard>
#include <QQuickItem>
#include <QScopeGuard>
#include <QWheelEvent>
#include <QVariantAnimation>
#include <QtTest/QTest>
namespace {
class AutoStopPlatform final : public VideoPlatform {
public:
    int starts=0,stops=0;
    quint64 token=0;
    RecordingModes supportedRecordingModes() const override{return Region;}
    Formats supportedFormats() const override{return MP4_H264;}
    bool isRecordingAudio() const override{return false;}
    bool supportsPause() const override{return true;}
    void startRecording(const QUrl &,RecordingMode mode,const QVariantMap &,bool) override {
        ++starts;setRecordingMode(mode);token=preparation()->begin(0);preparation()->targetReady(token);
    }
    bool acknowledge(){
        if(!QTest::qWaitFor([&]{return preparation()->phase()==NkShot::RecordingPreparation::Starting;},500)
            ||!preparation()->markRecording(token))return false;
        setRecordingState(RecordingState::Recording);return true;
    }
    void pauseRecording() override{if(recordingState()==RecordingState::Recording)setRecordingState(RecordingState::Paused);}
    void resumeRecording() override{if(isPaused())setRecordingState(RecordingState::Recording);}
    void finishRecording() override{if(!isRecording())return;++stops;setRecordingState(RecordingState::Rendering);}
    void idle(){preparation()->finish(token);setRecordingState(RecordingState::NotRecording);}
};
QQuickItem *find(QQuickItem *root,const QString &name){
    if(!root)return nullptr;
    if(root->objectName()==name)return root;
    for(auto child:root->childItems())if(auto result=find(child,name))return result;
    return nullptr;
}
}
bool RecordingAutoStopUiChecks::run(QApplication &application,const QString &screenshot,QString *failure){
    auto core=SpectacleCore::instance();
    const auto fail=[&](const QString &message){if(failure)*failure=message;if(ViewerWindow::instance())ViewerWindow::instance()->grabWindow().save(screenshot+QStringLiteral(".auto-stop-failed.png"));return false;};
    if(!core||!ViewerWindow::instance()||core->videoPlatform()->supportedRecordingModes()!=VideoPlatform::NoRecordingModes)
        return fail(QStringLiteral("Auto-stop fixture requires Null backend; refusing native capture"));
    const auto originalSeconds=Settings::recordingAutoStopSeconds();
    if(originalSeconds!=0)return fail(QStringLiteral("Auto-stop requires clean profile with Off default"));
    const auto originalMode=ViewerWindow::instance()->mode();const auto originalGeometry=ViewerWindow::instance()->geometry();
    const auto originalVideoMode=core->videoMode();const auto originalVideo=core->currentVideo();
    const auto image=core->annotationDocument()->renderToImage();const auto depth=core->annotationDocument()->undoStackDepth();
    const auto clipboard=application.clipboard()->text();const auto startMode=core->m_startMode;
    const bool returnToViewer=core->m_returnToViewer;
    core->deleteWindows();auto actual=std::move(core->m_videoPlatform);
    auto fake=std::make_unique<AutoStopPlatform>();auto probe=fake.get();core->m_videoPlatform=std::move(fake);
    const auto restore=qScopeGuard([&]{
        core->m_delayAnimation->stop();core->m_returnToViewer=returnToViewer;
        core->m_recordingAutoStop.invalidate();probe->idle();core->m_recordingRestoreActive=false;core->m_recordingControlRequired=false;
        core->deleteWindows();core->m_videoPlatform=std::move(actual);core->m_startMode=startMode;
        Settings::setRecordingAutoStopSeconds(originalSeconds);core->m_videoMode=originalVideoMode;Q_EMIT core->videoModeChanged(originalVideoMode);
        core->initViewerWindow(originalMode);ViewerWindow::instance()->setGeometry(originalGeometry);ViewerWindow::instance()->show();
    });
    QObject::connect(probe,&VideoPlatform::recordingStateChanged,core,&SpectacleCore::updateRecordingAutoStop);
    QObject::connect(probe,&VideoPlatform::recordedTimeChanged,core,&SpectacleCore::updateRecordingAutoStop);
    core->m_startMode=SpectacleCore::StartMode::Gui;core->m_videoMode=false;Q_EMIT core->videoModeChanged(false);
    core->initViewerWindow(ViewerWindow::Dialog);
    auto viewer=ViewerWindow::instance();viewer->resize(qEnvironmentVariableIsSet("NKSHOT_TEST_COMPACT")?QSize(420,620):QSize(760,640));viewer->show();viewer->requestActivate();
    const auto item=[&](const char *name){return find(ViewerWindow::instance()->contentItem(),QString::fromLatin1(name));};
    const auto click=[&](const char *name){
        auto window=ViewerWindow::instance();QQuickItem *target=nullptr;
        if(!QTest::qWaitFor([&]{target=item(name);return target&&target->isVisible()&&target->isEnabled();},1500))return false;
        for(auto parent=target->parentItem();parent;parent=parent->parentItem()){
            if(!parent->property("contentY").isValid())continue;
            const QRectF bounds(parent->mapToScene(QPointF()),parent->size());
            for(int attempt=0;attempt<12&&!bounds.contains(QRectF(target->mapToScene(QPointF()),target->size()));++attempt){
                const auto at=parent->mapToScene(QPointF(parent->width()/2,parent->height()/2));
                QWheelEvent wheel(at,window->mapToGlobal(at.toPoint()),QPoint(),QPoint(0,target->mapToScene(QPointF()).y()<bounds.top()?120:-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
                QCoreApplication::sendEvent(window,&wheel);QTest::qWait(80);
            }
            if(!bounds.contains(QRectF(target->mapToScene(QPointF()),target->size())))return false;
            if(parent->property("moving").isValid()&&!QTest::qWaitFor([&]{return !parent->property("moving").toBool();},2000))return false;
        }
        const auto point=target->mapToScene(QPointF(target->width()/2,target->height()/2));
        if(!QRectF(QPointF(),window->size()).contains(point))return false;
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,point.toPoint());QTest::qWait(80);return true;
    };
    const auto shot=[&](const QString &suffix){QTest::qWait(120);return ViewerWindow::instance()->grabWindow().save(screenshot+suffix);};
    if(!click("nkshotSetupRecordingTab")||!click("nkshotRecordingAdvanced"))return fail(QStringLiteral("Actual recording More options unavailable"));
    auto combo=item("nkshotRecordingAutoStop");
    if(!combo||combo->property("currentIndex").toInt()!=0)return fail(QStringLiteral("Auto-stop UI default not Off"));
    const int seconds[]={0,60,300,600};
    for(int index:{1,2,3,0,1}){
        if(!click("nkshotRecordingAutoStop"))return fail(QStringLiteral("Auto-stop ratio selector unreachable"));
        QTest::keyClick(viewer,Qt::Key_Home);for(int i=0;i<index;++i)QTest::keyClick(viewer,Qt::Key_Down);QTest::keyClick(viewer,Qt::Key_Return);
        if(!QTest::qWaitFor([&]{return Settings::recordingAutoStopSeconds()==seconds[index];},500))return fail(QStringLiteral("Actual auto-stop selection failed for index %1").arg(index));
    }
    if(!shot(QStringLiteral(".auto-stop-options.png"))||!click("nkshotRecordingAdvanced"))return fail(QStringLiteral("Auto-stop options capture/collapse failed"));
    if(!item("nkshotRecordingAdvanced")->property("text").toString().contains(QStringLiteral("Auto-stop 1 min"))||item("nkshotRecordingAutoStop")->isVisible())return fail(QStringLiteral("Collapsed options lost the selected auto-stop hint"));
    if(!shot(QStringLiteral(".auto-stop-collapsed.png")))return fail(QStringLiteral("Collapsed evidence failed"));
    // Real Core start snapshots the setting. The fake acknowledges no stream/file.
    core->startRecording(VideoPlatform::Region,false,0,Settings::recordingAutoStopSeconds());
    const auto epoch=core->m_recordingAutoStopEpoch;const auto token=probe->token;
    if(probe->starts!=1||!core->recordingAutoStopEnabled()||core->recordingAutoStopActive()||core->recordingAutoStopRemaining()!=60000)return fail(QStringLiteral("GUI start snapshot did not remain inactive before acknowledgement"));
    Settings::setRecordingAutoStopSeconds(600);
    if(core->recordingAutoStopRemaining()!=60000)return fail(QStringLiteral("Setting changed an already-started session limit"));
    core->deleteWindows();core->initViewerWindow(ViewerWindow::Viewer);viewer=ViewerWindow::instance();viewer->resize(qEnvironmentVariableIsSet("NKSHOT_TEST_COMPACT")?QSize(420,520):QSize(760,540));viewer->show();
    if(!probe->acknowledge())return fail(QStringLiteral("Synthetic recording acknowledgement was not accepted"));
    // Publishable actual-window evidence before virtual elapsed-time injection:
    // both the platform elapsed label and the remaining limit use fresh time.
    if(!QTest::qWaitFor([&]{
        auto label=item("nkshotRecordingStatusDescription");
        return core->recordingAutoStopActive()&&label
            &&label->property("text").toString()==QStringLiteral("Auto-stop in 01:00");
    },500)||!shot(QStringLiteral(".auto-stop-ready.png")))
        return fail(QStringLiteral("Fresh recording auto-stop evidence failed"));
    // Fake state/time notifications use the same private Core hook as production.
    // Only long elapsed samples are injected at the policy seam; VideoPlatform's
    // clock stays private. The production stopRequested -> finish route is intact.
    core->m_recordingAutoStop.update(epoch,token,17000,false);
    const auto text=[&]{auto label=item("nkshotRecordingStatusDescription");return label?label->property("text").toString():QString();};
    const auto screenshotRoundTrip=[&] {
        const auto screenshotEpoch=core->m_captureEpoch;
        const auto recordingEpoch=core->m_recordingAutoStopEpoch;
        const auto serial=core->m_imageGrabSerial;
        const auto remaining=core->recordingAutoStopRemaining();
        const auto state=probe->recordingState();
        const bool previousReturn=core->m_returnToViewer;
        const auto previousGeometry=viewer->geometry();
        core->m_returnToViewer=true;
        const auto restoreRoundTrip=qScopeGuard([&]{
            core->m_delayAnimation->stop();core->m_returnToViewer=previousReturn;
            viewer->showNormal();viewer->setGeometry(previousGeometry);viewer->requestActivate();
        });
        // Real Core delayed request/cancel, canceled long before Null doGrab.
        // This verifies routing only, not native screenshot or encoder behavior.
        core->takeNewScreenshot(ImagePlatform::AllScreens,99000,false,true,true);
        if(!QTest::qWaitFor([&]{return core->m_delayAnimation->currentTime()>0;},500))return false;
        if(core->m_captureEpoch==screenshotEpoch||core->m_recordingAutoStopEpoch!=recordingEpoch
            ||!core->recordingAutoStopActive()||core->recordingAutoStopRemaining()!=remaining)return false;
        const auto requestEpoch=core->m_captureEpoch;
        core->cancelScreenshot();
        core->updateRecordingAutoStop();
        QTest::qWait(40);
        return core->m_captureEpoch!=requestEpoch&&core->m_recordingAutoStopEpoch==recordingEpoch
            &&core->m_imageGrabSerial==serial&&core->m_pendingImageGrabs==0
            &&probe->recordingState()==state&&probe->stops==0
            &&core->recordingAutoStopActive()&&core->recordingAutoStopRemaining()==remaining;
    };
    if(!screenshotRoundTrip())return fail(QStringLiteral("Recording screenshot request/cancel lost auto-stop or reached capture backend"));

    if(!QTest::qWaitFor([&]{return text()==QStringLiteral("Auto-stop in 00:43");},1000)||!item("nkshotRecordingStop")->isVisible()||!item("nkshotRecordingPause")->isVisible())return fail(QStringLiteral("Auto-stop remaining replaced or hid recording controls"));
    if(!shot(QStringLiteral(".auto-stop-recording.png"))||!click("nkshotRecordingPause")||!probe->isPaused())return fail(QStringLiteral("Actual Pause dispatch failed"));
    core->m_recordingAutoStop.update(epoch,token,17000,true);QTest::qWait(1100);
    if(!screenshotRoundTrip())return fail(QStringLiteral("Paused screenshot request/cancel lost auto-stop or reached capture backend"));
    if(core->recordingAutoStopRemaining()!=43000||!text().contains(QStringLiteral("00:43 · Paused"))||!item("nkshotRecordingStop")->isEnabled()||item("nkshotRecordingPause")->property("text").toString()!=QStringLiteral("Resume"))return fail(QStringLiteral("Paused auto-stop display or active-time freeze failed"));
    if(!shot(QStringLiteral(".auto-stop-paused.png"))||!click("nkshotRecordingPause")||probe->isPaused())return fail(QStringLiteral("Actual Resume dispatch failed"));
    core->m_recordingAutoStop.update(epoch,token,60000,false);
    core->m_recordingAutoStop.update(epoch,token,61000,false);
    if(!QTest::qWaitFor([&]{return probe->stops==1;},1000)||probe->recordingState()!=VideoPlatform::RecordingState::Rendering)return fail(QStringLiteral("Virtual deadline did not dispatch exactly one real Core finish"));
    QTest::qWait(80);
    if(text()!=QStringLiteral("Preparing your video for review…")||item("nkshotRecordingStop")->isVisible()||core->recordingAutoStopActive()||!shot(QStringLiteral(".auto-stop-finishing.png")))return fail(QStringLiteral("Auto-stop Rendering changed existing finalization feedback"));
    // Expiry queued for a retired generation must not stop its replacement.
    probe->idle();Settings::setRecordingAutoStopSeconds(60);core->startRecording(VideoPlatform::Region,false,0,Settings::recordingAutoStopSeconds());probe->acknowledge();
    const auto retiredEpoch=core->m_recordingAutoStopEpoch,retiredToken=probe->token;
    core->m_recordingAutoStop.update(retiredEpoch,retiredToken,60000,false);
    probe->idle();core->startRecording(VideoPlatform::Region,false,0,Settings::recordingAutoStopSeconds());probe->acknowledge();
    core->m_recordingAutoStop.update(retiredEpoch,retiredToken,90000,false);QTest::qWait(100);
    if(probe->stops!=1||!probe->isRecording()||core->recordingAutoStopRemaining()!=60000)return fail(QStringLiteral("Retired queued expiry leaked into replacement generation"));
    core->m_recordingAutoStop.update(core->m_recordingAutoStopEpoch,probe->token,60000,false);
    if(!QTest::qWaitFor([&]{return probe->stops==2;},1000))return fail(QStringLiteral("Replacement generation did not auto-stop"));
    probe->idle();Settings::setRecordingAutoStopSeconds(60);core->startRecording(VideoPlatform::Region,false,0);probe->acknowledge();
    core->m_recordingAutoStop.update(core->m_recordingAutoStopEpoch,probe->token,999999,false);QTest::qWait(80);
    if(core->recordingAutoStopEnabled()||probe->stops!=2||!probe->isRecording())return fail(QStringLiteral("Default external start inherited GUI auto-stop setting"));
    if(core->annotationDocument()->renderToImage()!=image||core->annotationDocument()->undoStackDepth()!=depth||core->currentVideo()!=originalVideo||application.clipboard()->text()!=clipboard)return fail(QStringLiteral("Auto-stop fixture modified source/history/clipboard"));
    qInfo()<<"113 actual options/status UI and Core finish route passed with virtual recorded time; no native recording or saved-file claim";
    return true;
}
