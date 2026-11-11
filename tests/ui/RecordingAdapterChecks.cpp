// SPDX-License-Identifier: GPL-3.0-only
#include "RecordingAdapterChecks.h"
#include "Platforms/RecordingBackend.h"
#include "Platforms/VideoPlatformWayland.h"
#include "settings.h"
#include "ExportManager.h"
#include <QFile>
#include <QtTest/QTest>
#include <QtTest/QSignalSpy>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QPointer>
#include <QScopeGuard>
#include <QGuiApplication>
#include <QScreen>
#include <QDir>
#include <QFileInfo>
#include <memory>
namespace {
using Recorder = NkShot::RecordingRecorder;
using Prep = NkShot::RecordingPreparation;
struct Observations {
    int outputs=0,outputTargets=0,streams=0,starts=0,stops=0,retired=0;
    bool synchronousError=false,autoCreated=false;
    QString output;
    bool audio=false,microphone=false;
    Recorder::Encoder encoder=Recorder::Encoder::NoEncoder;
    Screencasting::CursorMode cursor=Screencasting::Hidden;
};
class FakeRecorder final : public Recorder {
public:
    explicit FakeRecorder(std::shared_ptr<Observations> observations):data(std::move(observations)){}
    State state() const override {return current;}
    QList<Encoder> suggestedEncoders() const override {return {Encoder::H264Main,Encoder::VP9,Encoder::Gif,Encoder::WebP};}
    QString output() const override {return data->output;}
    bool recordSystemAudio() const override {return data->audio;}
    bool recordMicrophone() const override {return data->microphone;}
    void setOutput(const QString &value) override {++data->outputs;if(!value.isEmpty())++data->outputTargets;data->output=value;}
    void setEncoder(Encoder value) override {data->encoder=value;}
    void setRecordSystemAudio(bool value) override {data->audio=value;}
    void setRecordMicrophone(bool value) override {data->microphone=value;}
    void setNodeId(uint value) override {node=value;}
    void setMaxPendingFrames(int) override {}
    void start() override {++data->starts;if(data->synchronousError){Q_EMIT errorFound(QStringLiteral("synthetic start failure"));current=PipeWireBaseEncodedStream::Idle;Q_EMIT stateChanged();}}
    void stop() override {++data->stops;}
    void report(State state) {current=state;Q_EMIT stateChanged();}
    uint node=0;
private:
    std::shared_ptr<Observations> data;
    State current=PipeWireBaseEncodedStream::Idle;
};
class FakeSource final : public NkShot::RecordingSource {
public:
    FakeSource(std::shared_ptr<Observations> observations,QObject *parent):RecordingSource(parent),data(std::move(observations)){}
    ~FakeSource() override {++data->retired;}
    quint32 nodeId() const override {return 42;}
private:
    std::shared_ptr<Observations> data;
};
class FakeScreencast final : public NkShot::RecordingScreencast {
public:
    explicit FakeScreencast(std::shared_ptr<Observations> observations):data(std::move(observations)){}
    bool isAvailable() const override {return available;}
    bool isRegionAutoScaleSupported() const override {return true;}
    NkShot::RecordingSource *createOutputStream(QScreen *,Screencasting::CursorMode mode) override {return make(mode);}
    NkShot::RecordingSource *createWindowStream(const QString &,Screencasting::CursorMode mode) override {return make(mode);}
    NkShot::RecordingSource *createRegionStream(const QRect &,qreal,Screencasting::CursorMode mode) override {return make(mode);}
    bool available=true;
    QPointer<FakeSource> source;
private:
    NkShot::RecordingSource *make(Screencasting::CursorMode mode){++data->streams;data->cursor=mode;source=new FakeSource(data,this);if(data->autoCreated)QTimer::singleShot(0,source,[source=source]{if(source)Q_EMIT source->created(42);});return source;}
    std::shared_ptr<Observations> data;
};
struct Fixture {
    std::shared_ptr<Observations> data=std::make_shared<Observations>();
    FakeScreencast *caster=new FakeScreencast(data);
    QPointer<FakeRecorder> recorder;
    std::unique_ptr<VideoPlatformWayland> adapter;
    explicit Fixture(int timeout=30000,bool available=true){caster->available=available;adapter=std::make_unique<VideoPlatformWayland>(caster,[this]{auto result=std::make_unique<FakeRecorder>(data);recorder=result.get();return result;},nullptr,timeout);}
    bool initialized(){return QTest::qWaitFor([this]{return !recorder.isNull();},2000);}
};
}
bool runRecordingAdapterChecks(QJsonObject *report) {
    QJsonObject result;QJsonArray passed;
    const auto fail=[&](const QString &reason){result.insert(QStringLiteral("passed"),false);result.insert(QStringLiteral("failure"),reason);result.insert(QStringLiteral("checks"),passed);if(report)*report=result;return false;};
#define CHECK_ADAPTER(condition, explanation) do {if(!(condition))return fail(QStringLiteral(explanation));} while(false)
    QTemporaryDir folder;CHECK_ADAPTER(folder.isValid(),"temporary output directory");CHECK_ADAPTER(qGuiApp->primaryScreen(),"fixture needs a software QScreen");
    const QRectF rect(qGuiApp->primaryScreen()->geometry().topLeft(),QSizeF(64,48));
    const bool oldAudio=Settings::videoRecordSystemAudio(),oldMic=Settings::videoRecordMicrophone();const auto oldFormat=Settings::preferredVideoFormat();
    const auto restore=qScopeGuard([&]{Settings::setVideoRecordSystemAudio(oldAudio);Settings::setVideoRecordMicrophone(oldMic);Settings::setPreferredVideoFormat(oldFormat);});
    {
        Fixture f;CHECK_ADAPTER(f.initialized(),"adapter initialization");QSignalSpy saved(f.adapter.get(),&VideoPlatform::recordingSaved);QSignalSpy requested(f.adapter.get(),&VideoPlatform::regionRequested);
        Settings::setVideoRecordSystemAudio(true);Settings::setVideoRecordMicrophone(false);Settings::setPreferredVideoFormat(VideoPlatform::MP4_H264);
        const auto output=QUrl::fromLocalFile(folder.filePath(QStringLiteral("post-selection/clip.mp4")));
        f.adapter->startRecording(output,VideoPlatform::Region,{{QStringLiteral("startDelay"),3}},true);
        CHECK_ADAPTER(QTest::qWaitFor([&]{return requested.size()==1;},2000),"region selection request");QTest::qWait(150);
        CHECK_ADAPTER(f.data->streams==0&&f.data->outputs==0&&f.data->starts==0,"no output or stream while Selecting");
        const auto token=f.adapter->preparation()->token();QElapsedTimer clock;clock.start();f.adapter->provideRecordingRegion(rect,token);
        Settings::setVideoRecordSystemAudio(false);Settings::setVideoRecordMicrophone(true);Settings::setPreferredVideoFormat(VideoPlatform::Gif);
        QTest::qWait(160);CHECK_ADAPTER(f.data->streams==0&&f.data->outputs==0&&!QDir(folder.filePath(QStringLiteral("post-selection"))).exists(),"no pre-deadline output side effect");
        CHECK_ADAPTER(QTest::qWaitFor([&]{return f.data->streams==1;},4500),"post-selection countdown reaches commit");CHECK_ADAPTER(clock.elapsed()>=3000,"countdown cannot finish early");
        CHECK_ADAPTER(f.data->audio&&!f.data->microphone&&f.data->encoder==Recorder::Encoder::H264Main&&f.data->cursor==Screencasting::Embedded,"frozen audio/format/pointer snapshot");
        CHECK_ADAPTER(f.caster->source,"source created");Q_EMIT f.caster->source->created(42);CHECK_ADAPTER(f.data->starts==1,"created source starts exactly once");
        CHECK_ADAPTER(!f.adapter->isRecording(),"start issued is not Recording");f.adapter->cancelPendingRecording();CHECK_ADAPTER(!f.adapter->readyForRecording(),"cancelled outstanding producer keeps readyForRecording false");CHECK_ADAPTER(f.data->retired==0,"cancel retains outstanding source");
        f.adapter->startRecording(output,VideoPlatform::Region,{{QStringLiteral("rect"),rect}},false);QTest::qWait(50);CHECK_ADAPTER(f.data->streams==1,"retry blocked until producer Idle");
        f.recorder->report(PipeWireBaseEncodedStream::Recording);CHECK_ADAPTER(!f.adapter->isRecording()&&saved.isEmpty(),"late Recording after cancel is ignored");CHECK_ADAPTER(f.data->retired==0,"late Recording retains source");
        f.recorder->report(PipeWireBaseEncodedStream::Idle);CHECK_ADAPTER(QTest::qWaitFor([&]{return f.data->retired==1&&f.adapter->readyForRecording();},1000),"Idle retires source and restores readyForRecording");CHECK_ADAPTER(saved.isEmpty(),"canceled attempt never saved");
        f.adapter->startRecording(output,VideoPlatform::Region,{{QStringLiteral("rect"),rect}},false);CHECK_ADAPTER(QTest::qWaitFor([&]{return f.data->streams==2;},2000),"retry after explicit Idle");f.adapter->cancelPendingRecording();
        passed.append(QStringLiteral("real-adapter post-selection deadline, frozen settings, starting-cancel/late-state/Idle retry"));
    }
    {
        Fixture f;CHECK_ADAPTER(f.initialized(),"cancel fixture initialized");QSignalSpy saved(f.adapter.get(),&VideoPlatform::recordingSaved);const auto output=QUrl::fromLocalFile(folder.filePath(QStringLiteral("cancel/clip.mp4")));
        f.adapter->startRecording(output,VideoPlatform::Region,{{QStringLiteral("rect"),rect},{QStringLiteral("startDelay"),3}},false);
        CHECK_ADAPTER(QTest::qWaitFor([&]{return f.adapter->preparation()->phase()==Prep::Countdown;},2000),"cancel enters Countdown");const auto stale=f.adapter->preparation()->token();f.adapter->cancelPendingRecording();f.adapter->provideRecordingRegion(rect,stale);QTest::qWait(3100);
        CHECK_ADAPTER(f.data->outputs==0&&f.data->streams==0&&f.data->starts==0&&saved.isEmpty(),"countdown cancel + late target has no side effects");
        passed.append(QStringLiteral("real-adapter cancel before commit and stale target"));
    }
    {
        Fixture f;CHECK_ADAPTER(f.initialized(),"sync failure fixture initialized");f.data->synchronousError=true;QSignalSpy saved(f.adapter.get(),&VideoPlatform::recordingSaved);QSignalSpy errors(f.adapter.get(),&VideoPlatform::recordingFailed);
        f.adapter->startRecording(QUrl::fromLocalFile(folder.filePath(QStringLiteral("sync-error.mp4"))),VideoPlatform::Region,{{QStringLiteral("rect"),rect}},false);
        CHECK_ADAPTER(QTest::qWaitFor([&]{return bool(f.caster->source);},2000),"sync failure source");Q_EMIT f.caster->source->created(42);QTest::qWait(50);
        CHECK_ADAPTER(errors.size()==1&&!f.adapter->isRecording()&&saved.isEmpty(),"synchronous start failure never recording/saved");CHECK_ADAPTER(f.data->retired==1,"synchronous Idle retires source once");
        passed.append(QStringLiteral("real-adapter synchronous recorder failure"));
    }
    {
        Fixture f;CHECK_ADAPTER(f.initialized(),"invalid target fixture initialized");QSignalSpy errors(f.adapter.get(),&VideoPlatform::recordingFailed);QSignalSpy saved(f.adapter.get(),&VideoPlatform::recordingSaved);
        f.adapter->startRecording(QUrl::fromLocalFile(folder.filePath(QStringLiteral("invalid-target.mp4"))),VideoPlatform::Region,{{QStringLiteral("rect"),rect},{QStringLiteral("startDelay"),3}},false);
        CHECK_ADAPTER(QTest::qWaitFor([&]{return f.adapter->preparation()->phase()==Prep::Countdown;},2000),"target invalidation countdown");f.caster->available=false;Q_EMIT f.caster->removed();
        CHECK_ADAPTER(QTest::qWaitFor([&]{return !errors.isEmpty();},4500),"source availability invalidation reported");CHECK_ADAPTER(f.data->outputs==0&&f.data->streams==0&&saved.isEmpty(),"invalid target never opens output/stream");
        passed.append(QStringLiteral("real-adapter target unavailable before commit"));
    }
    {
        Fixture f;CHECK_ADAPTER(f.initialized(),"success fixture initialized");QSignalSpy saved(f.adapter.get(),&VideoPlatform::recordingSaved);QSignalSpy errors(f.adapter.get(),&VideoPlatform::recordingFailed);
        const auto output=QUrl::fromLocalFile(folder.filePath(QStringLiteral("successful-state-flow.mp4")));
        f.adapter->startRecording(output,VideoPlatform::Region,{{QStringLiteral("rect"),rect}},false);
        CHECK_ADAPTER(QTest::qWaitFor([&]{return bool(f.caster->source);},1000),"success source prepared");
        CHECK_ADAPTER(f.adapter->preparation()->phase()==Prep::Starting&&!f.adapter->isRecording()&&f.adapter->recordedTime()==0,"preparation not counted as recording");
        Q_EMIT f.caster->source->created(42);QTest::qWait(180);
        CHECK_ADAPTER(f.data->starts==1&&!f.adapter->isRecording()&&f.adapter->recordedTime()==0&&saved.isEmpty(),"recorder acknowledgement wait not counted");
        f.recorder->report(PipeWireBaseEncodedStream::Recording);
        CHECK_ADAPTER(f.adapter->isRecording()&&f.adapter->preparation()->phase()==Prep::Recording,"actual recorder acknowledgement enters Recording");
        CHECK_ADAPTER(QTest::qWaitFor([&]{return f.adapter->recordedTime()>0;},1500),"elapsed recording clock starts after acknowledgement");
        f.adapter->finishRecording();CHECK_ADAPTER(f.data->stops>0&&f.data->retired==0,"finish requests stop but retains draining source");
        f.recorder->report(PipeWireBaseEncodedStream::Rendering);
        CHECK_ADAPTER(f.adapter->recordingState()==VideoPlatform::RecordingState::Rendering&&f.data->retired==0&&saved.isEmpty(),"Rendering retains source and does not save early");
        f.recorder->report(PipeWireBaseEncodedStream::Idle);
        CHECK_ADAPTER(QTest::qWaitFor([&]{return f.data->retired==1&&f.adapter->readyForRecording();},1000),"completed Idle retires source and allows restart");
        CHECK_ADAPTER(saved.size()==1&&saved.first().first().toUrl()==output&&errors.isEmpty(),"exactly one saved result with correct output");
        f.recorder->report(PipeWireBaseEncodedStream::Idle);CHECK_ADAPTER(saved.size()==1,"duplicate Idle cannot save twice");
        const QPointer<FakeRecorder> retiredRecorder=f.recorder;
        const auto previousStreams=f.data->streams;const auto previousOutputs=f.data->outputs;
        f.adapter->startRecording(QUrl::fromLocalFile(folder.filePath(QStringLiteral("isolated-countdown.mp4"))),VideoPlatform::Region,{{QStringLiteral("rect"),rect},{QStringLiteral("startDelay"),3}},false);
        CHECK_ADAPTER(QTest::qWaitFor([&]{return f.adapter->preparation()->phase()==Prep::Countdown;},1000),"new attempt Countdown after prior Idle");
        const auto newToken=f.adapter->preparation()->token();
        QTimer::singleShot(0,retiredRecorder,[retiredRecorder]{if(retiredRecorder){Q_EMIT retiredRecorder->errorFound(QStringLiteral("stale queued error after Idle"));retiredRecorder->report(PipeWireBaseEncodedStream::Idle);}});
        QTest::qWait(100);
        CHECK_ADAPTER(f.adapter->preparation()->phase()==Prep::Countdown&&f.adapter->preparation()->token()==newToken&&errors.isEmpty()&&!f.adapter->isRecording(),"retired recorder error/state cannot poison new Countdown");
        CHECK_ADAPTER(f.data->streams==previousStreams&&f.data->outputs==previousOutputs&&saved.size()==1,"old recorder callbacks cannot commit or save new attempt");
        f.adapter->cancelPendingRecording();
        f.adapter->startRecording(QUrl::fromLocalFile(folder.filePath(QStringLiteral("success-restart.mp4"))),VideoPlatform::Region,{{QStringLiteral("rect"),rect}},false);
        CHECK_ADAPTER(QTest::qWaitFor([&]{return f.data->streams==2;},1000),"successful recording permits next attempt");f.adapter->cancelPendingRecording();
        passed.append(QStringLiteral("real-adapter positive Recording/Rendering/Idle clock, saved-once and restart (fake media producer)"));
    }
    {
        const auto oldTemplate=Settings::videoFilenameTemplate();
        const auto restoreTemplate=qScopeGuard([&]{Settings::setVideoFilenameTemplate(oldTemplate);});
        Settings::setVideoFilenameTemplate(QStringLiteral("../escape076"));Settings::setPreferredVideoFormat(VideoPlatform::MP4_H264);
        const auto temporary=ExportManager::instance()->temporaryDir();CHECK_ADAPTER(temporary,"private temporary directory exists");
        const auto outside=QDir::cleanPath(temporary->path()+QStringLiteral("/../escape076.mp4"));
        CHECK_ADAPTER(!QFileInfo::exists(outside),"outside fixture path initially absent");
        Fixture f;CHECK_ADAPTER(f.initialized(),"unsafe template fixture initialized");QSignalSpy errors(f.adapter.get(),&VideoPlatform::recordingFailed);QSignalSpy saved(f.adapter.get(),&VideoPlatform::recordingSaved);
        f.adapter->startRecording(QUrl(),VideoPlatform::Region,{{QStringLiteral("rect"),rect}},false);
        CHECK_ADAPTER(QTest::qWaitFor([&]{return !errors.isEmpty();},2000),"escaping GUI template rejected at commit");
        CHECK_ADAPTER(f.data->outputTargets==0&&f.data->streams==0&&f.data->starts==0&&saved.isEmpty()&&!QFileInfo::exists(outside),"escaping template has no output or stream side effects");
        passed.append(QStringLiteral("real-adapter GUI template containment"));
    }
    {
        const auto existing=folder.filePath(QStringLiteral("keep-existing.mp4"));QFile file(existing);CHECK_ADAPTER(file.open(QIODevice::WriteOnly),"existing output fixture open");const QByteArray bytes("original recording bytes stay intact");CHECK_ADAPTER(file.write(bytes)==bytes.size(),"existing output fixture write");file.close();
        Fixture f;CHECK_ADAPTER(f.initialized(),"existing output fixture initialized");
        f.adapter->startRecording(QUrl::fromLocalFile(existing),VideoPlatform::Region,{{QStringLiteral("rect"),rect},{QStringLiteral("startDelay"),3}},false);
        CHECK_ADAPTER(QTest::qWaitFor([&]{return f.adapter->preparation()->phase()==Prep::Countdown;},1000),"existing output remains in Countdown");f.adapter->cancelPendingRecording();QTest::qWait(60);
        CHECK_ADAPTER(f.data->outputs==0&&f.data->streams==0&&f.data->starts==0,"existing output cancel before any recorder configuration");CHECK_ADAPTER(file.open(QIODevice::ReadOnly)&&file.readAll()==bytes,"existing output bytes preserved on Countdown cancel");
        passed.append(QStringLiteral("real-adapter existing output preserved before commit cancellation"));
    }
    {
        Fixture f(30000,false);CHECK_ADAPTER(f.initialized(),"availability fixture initialized");
        CHECK_ADAPTER(!f.adapter->supportedRecordingModes()&&!f.adapter->supportedFormats(),"initial unavailable capabilities empty");
        QSignalSpy modes(f.adapter.get(),&VideoPlatform::supportedRecordingModesChanged);QSignalSpy formats(f.adapter.get(),&VideoPlatform::supportedFormatsChanged);
        f.caster->available=true;Q_EMIT f.caster->initialized();
        CHECK_ADAPTER(QTest::qWaitFor([&]{return !modes.isEmpty()&&!formats.isEmpty();},1000),"availability notifications");
        CHECK_ADAPTER(f.adapter->supportedRecordingModes().testFlag(VideoPlatform::Region)&&f.adapter->supportedFormats().testFlag(VideoPlatform::MP4_H264),"availability restores capabilities");
        QSignalSpy errors(f.adapter.get(),&VideoPlatform::recordingFailed);
        f.adapter->startRecording(QUrl::fromLocalFile(folder.filePath(QStringLiteral("removed.mp4"))),VideoPlatform::Region,{{QStringLiteral("rect"),rect},{QStringLiteral("startDelay"),3}},false);
        CHECK_ADAPTER(QTest::qWaitFor([&]{return f.adapter->preparation()->phase()==Prep::Countdown;},1000),"removed countdown begins");
        modes.clear();formats.clear();f.caster->available=false;Q_EMIT f.caster->removed();
        CHECK_ADAPTER(QTest::qWaitFor([&]{return !errors.isEmpty()&&!modes.isEmpty()&&!formats.isEmpty();},1000),"removed cancels countdown and notifies capabilities");
        CHECK_ADAPTER(f.adapter->preparation()->phase()==Prep::Idle&&f.data->streams==0&&f.data->outputs==0,"removed cancels before output");
        passed.append(QStringLiteral("real-adapter provider availability and removal notifications"));
    }
    {
        Fixture f(40);CHECK_ADAPTER(f.initialized(),"watchdog fixture initialized");f.data->autoCreated=true;QSignalSpy errors(f.adapter.get(),&VideoPlatform::recordingFailed);QSignalSpy saved(f.adapter.get(),&VideoPlatform::recordingSaved);
        f.adapter->startRecording(QUrl::fromLocalFile(folder.filePath(QStringLiteral("watchdog.mp4"))),VideoPlatform::Region,{{QStringLiteral("rect"),rect}},false);
        CHECK_ADAPTER(QTest::qWaitFor([&]{return f.data->starts==1;},1000),"watchdog producer start issued");
        CHECK_ADAPTER(QTest::qWaitFor([&]{return !errors.isEmpty();},1000),"real Starting watchdog fires");
        CHECK_ADAPTER(!f.adapter->isRecording()&&saved.isEmpty()&&f.data->retired==0&&!f.adapter->readyForRecording(),"watchdog retains outstanding producer until Idle");
        f.recorder->report(PipeWireBaseEncodedStream::Idle);CHECK_ADAPTER(QTest::qWaitFor([&]{return f.adapter->readyForRecording()&&f.data->retired==1;},1000),"watchdog Idle releases retry");
        passed.append(QStringLiteral("real-adapter Starting watchdog with injected 40ms deadline"));
    }
    {
        // Real patched KPipeWire via native wrapper, but deliberately impossible
        // configuration: no live devices/server/desktop frames are requested.
        const bool hadDir=qEnvironmentVariableIsSet("PIPEWIRE_CONFIG_DIR"),hadName=qEnvironmentVariableIsSet("PIPEWIRE_CONFIG_NAME");
        const auto oldDir=qgetenv("PIPEWIRE_CONFIG_DIR"),oldName=qgetenv("PIPEWIRE_CONFIG_NAME");
        const auto restorePipeWire=qScopeGuard([&]{if(hadDir)qputenv("PIPEWIRE_CONFIG_DIR",oldDir);else qunsetenv("PIPEWIRE_CONFIG_DIR");if(hadName)qputenv("PIPEWIRE_CONFIG_NAME",oldName);else qunsetenv("PIPEWIRE_CONFIG_NAME");});
        qputenv("PIPEWIRE_CONFIG_DIR",folder.path().toUtf8());qputenv("PIPEWIRE_CONFIG_NAME","nkshot-adapter-deliberately-missing.conf");
        auto data=std::make_shared<Observations>();auto caster=new FakeScreencast(data);
        auto adapter=std::make_unique<VideoPlatformWayland>(caster,[]{return NkShot::makeNativeRecordingRecorder();});
        CHECK_ADAPTER(QTest::qWaitFor([&]{return adapter->supportedFormats().testFlag(VideoPlatform::MP4_H264);},2000),"native recorder encoder availability");
        QSignalSpy errors(adapter.get(),&VideoPlatform::recordingFailed);QSignalSpy saved(adapter.get(),&VideoPlatform::recordingSaved);
        for(int attempt=0;attempt<2;++attempt) {
            const auto output=QUrl::fromLocalFile(folder.filePath(QStringLiteral("native-context-%1.mp4").arg(attempt)));
            adapter->startRecording(output,VideoPlatform::Region,{{QStringLiteral("rect"),rect}},false);
            CHECK_ADAPTER(QTest::qWaitFor([&]{return data->streams==attempt+1&&bool(caster->source);},2000),"native retry creates fresh source");
            Q_EMIT caster->source->created(42);if(attempt==1)adapter->cancelPendingRecording();
            CHECK_ADAPTER(QTest::qWaitFor([&]{return adapter->readyForRecording()&&data->retired==attempt+1;},4000),"native failed producer reaches Idle and retires source");
            CHECK_ADAPTER(!adapter->isRecording()&&saved.isEmpty()&&!QFileInfo::exists(output.toLocalFile()),"native context failure/cancel never records or saves");
        }
        CHECK_ADAPTER(!errors.isEmpty(),"native context error forwarded");QElapsedTimer destruction;destruction.start();adapter.reset();CHECK_ADAPTER(destruction.elapsed()<2000,"native adapter destruction bounded");
        passed.append(QStringLiteral("real-adapter + native patched KPipeWire failed-context/Cancel/Idle/retry/destruction"));
    }
    result.insert(QStringLiteral("passed"),true);result.insert(QStringLiteral("checks"),passed);result.insert(QStringLiteral("nativeAcceptance"),false);result.insert(QStringLiteral("watchdog30sExecuted"),false);result.insert(QStringLiteral("watchdogInjected40msExecuted"),true);if(report)*report=result;return true;
#undef CHECK_ADAPTER
}
