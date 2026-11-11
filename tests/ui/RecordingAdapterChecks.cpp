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
#include <unistd.h>
namespace {
using Recorder = NkShot::RecordingRecorder;
using Prep = NkShot::RecordingPreparation;
struct Observations {
    int outputs=0,outputTargets=0,streams=0,starts=0,stops=0,retired=0,pauses=0,resumes=0;
    bool synchronousError=false,autoCreated=false,writeOnStart=false,writeOk=false,pauseSupported=true;
    QByteArray bytes=QByteArrayLiteral("synthetic controlled QFile partial bytes 077");
    QByteArray destructorTail;
    bool destructorTailWritten=false,destructorFilePresent=false;
    QString output,lastNonemptyOutput;
    bool audio=false,microphone=false;
    Recorder::Encoder encoder=Recorder::Encoder::NoEncoder;
    Screencasting::CursorMode cursor=Screencasting::Hidden;
};
class FakeRecorder final : public Recorder {
public:
    explicit FakeRecorder(std::shared_ptr<Observations> observations):data(std::move(observations)){}
    ~FakeRecorder() override {if(!data->destructorTail.isEmpty()&&writer.isOpen()){data->destructorFilePresent=QFileInfo::exists(writer.fileName());data->destructorTailWritten=append(data->destructorTail);}writer.close();}
    State state() const override {return current;}
    QList<Encoder> suggestedEncoders() const override {return {Encoder::H264Main,Encoder::VP9,Encoder::Gif,Encoder::WebP};}
    QString output() const override {return data->output;}
    bool recordSystemAudio() const override {return data->audio;}
    bool recordMicrophone() const override {return data->microphone;}
    void setOutput(const QString &value) override {++data->outputs;if(!value.isEmpty()){++data->outputTargets;data->lastNonemptyOutput=value;}data->output=value;}
    void setEncoder(Encoder value) override {data->encoder=value;}
    void setRecordSystemAudio(bool value) override {data->audio=value;}
    void setRecordMicrophone(bool value) override {data->microphone=value;}
    void setNodeId(uint value) override {node=value;}
    void setMaxPendingFrames(int) override {}
    void start() override {++data->starts;if(data->writeOnStart){writer.setFileName(data->output);data->writeOk=writer.open(QIODevice::WriteOnly|QIODevice::Truncate)&&writer.write(data->bytes)==data->bytes.size()&&writer.flush();}if(data->synchronousError){Q_EMIT errorFound(QStringLiteral("synthetic start failure"));report(PipeWireBaseEncodedStream::Idle);}}
    void stop() override {++data->stops;}
    bool supportsPause() const override {return data->pauseSupported;}
    void pause() override {++data->pauses;}
    void resume() override {++data->resumes;}
    bool append(const QByteArray &tail) {return writer.isOpen()&&writer.write(tail)==tail.size()&&writer.flush();}
    void report(State state) {if(state==PipeWireBaseEncodedStream::Idle)writer.close();current=state;Q_EMIT stateChanged();}
    uint node=0;
private:
    std::shared_ptr<Observations> data;
    QFile writer; // Controlled producer handle lives until explicit Idle/destruction.
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
    bool available=true,returnNull=false;
    QPointer<FakeSource> source;
private:
    NkShot::RecordingSource *make(Screencasting::CursorMode mode){++data->streams;data->cursor=mode;if(returnNull)return nullptr;source=new FakeSource(data,this);if(data->autoCreated)QTimer::singleShot(0,source,[source=source]{if(source)Q_EMIT source->created(42);});return source;}
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
        f.adapter->startRecording(QUrl::fromLocalFile(folder.filePath(QStringLiteral("post-selection/retry.mp4"))),VideoPlatform::Region,{{QStringLiteral("rect"),rect}},false);CHECK_ADAPTER(QTest::qWaitFor([&]{return f.data->streams==2;},2000),"retry after explicit Idle");f.adapter->cancelPendingRecording();
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
            CHECK_ADAPTER(!adapter->isRecording()&&saved.isEmpty()&&QFileInfo::exists(output.toLocalFile())&&QFileInfo(output.toLocalFile()).size()==0,"native context failure/cancel never records or saves; explicit empty reservation retained");
        }
        CHECK_ADAPTER(!errors.isEmpty(),"native context error forwarded");QElapsedTimer destruction;destruction.start();adapter.reset();CHECK_ADAPTER(destruction.elapsed()<2000,"native adapter destruction bounded");
        passed.append(QStringLiteral("real-adapter + native patched KPipeWire failed-context/Cancel/Idle/retry/destruction"));
    }

    // All media bytes below are generated test markers, not playable recordings.
    const auto readBytes=[](const QString &path){QFile file(path);return file.open(QIODevice::ReadOnly)?file.readAll():QByteArray();};
    const auto writeBytes=[](const QString &path,const QByteArray &bytes){QFile file(path);return file.open(QIODevice::WriteOnly|QIODevice::Truncate)&&file.write(bytes)==bytes.size();};
    Settings::setPreferredVideoFormat(VideoPlatform::MP4_H264);
    const auto oldOwnedTemplate=Settings::videoFilenameTemplate();
    const auto restoreOwnedTemplate=qScopeGuard([&]{Settings::setVideoFilenameTemplate(oldOwnedTemplate);});
    Settings::setVideoFilenameTemplate(QStringLiteral("owned077"));
    const auto startOwned=[&](Fixture &f){
        const int previousStreams=f.data->streams;
        f.adapter->startRecording(QUrl(),VideoPlatform::Region,{{QStringLiteral("rect"),rect}},false);
        return QTest::qWaitFor([&]{return f.data->streams==previousStreams+1
            &&f.adapter->preparation()->phase()==Prep::Starting
            &&bool(f.caster->source)&&!f.data->output.isEmpty()&&QFileInfo::exists(f.data->output);},2000);
    };
    {
        Fixture f;CHECK_ADAPTER(f.initialized(),"owned cancel init");f.data->writeOnStart=true;
        QSignalSpy saved(f.adapter.get(),&VideoPlatform::recordingSaved);
        CHECK_ADAPTER(startOwned(f),"owned output prepared");
        const QString first=f.data->output;
        CHECK_ADAPTER(QFileInfo::exists(first)&&QFileInfo(first).size()==0,"owned empty file reserved before producer");
        const QString sibling=QFileInfo(first).dir().filePath(QStringLiteral("unknown-sibling077"));
        CHECK_ADAPTER(writeBytes(sibling,QByteArrayLiteral("unowned sibling")),"unknown sibling created");
        Q_EMIT f.caster->source->created(42);
        CHECK_ADAPTER(f.data->writeOk&&readBytes(first)==f.data->bytes,"controlled QFile writes actual owned partial");
        f.adapter->cancelPendingRecording();
        CHECK_ADAPTER(readBytes(first)==f.data->bytes&&saved.isEmpty(),"cancel keeps producer file until Idle");
        const QByteArray tail(" synthetic draining tail");
        CHECK_ADAPTER(f.recorder->append(tail),"canceled controlled producer can still flush tail before Idle");
        f.data->bytes+=tail;
        CHECK_ADAPTER(readBytes(first)==f.data->bytes,"cancel leaves named partial writable while producer drains");
        f.recorder->report(PipeWireBaseEncodedStream::Recording);
        CHECK_ADAPTER(readBytes(first)==f.data->bytes&&!f.adapter->isRecording(),"late Recording preserves partial until Idle");
        const QPointer<FakeRecorder> old=f.recorder;
        f.recorder->report(PipeWireBaseEncodedStream::Idle);
        CHECK_ADAPTER(QTest::qWaitFor([&]{return f.adapter->readyForRecording();},1000)&&!QFileInfo::exists(first)&&saved.isEmpty(),"Idle removes owned canceled partial without saved");
        CHECK_ADAPTER(readBytes(sibling)==QByteArrayLiteral("unowned sibling"),"unknown sibling untouched");
        CHECK_ADAPTER(startOwned(f),"owned retry prepared");
        const QString second=f.data->output;
        CHECK_ADAPTER(QFileInfo::exists(second)&&QFileInfo(second).size()==0,"retry obtains empty exclusive reservation even if basename is reused");
        if(old){Q_EMIT old->errorFound(QStringLiteral("stale old output error"));old->report(PipeWireBaseEncodedStream::Idle);}
        QTest::qWait(40);
        CHECK_ADAPTER(QFileInfo::exists(second)&&f.adapter->preparation()->phase()==Prep::Starting,"stale old Idle/error cannot remove new reservation");
        const int startsBeforeRetry=f.data->starts;
        Q_EMIT f.caster->source->created(42);
        CHECK_ADAPTER(f.data->starts==startsBeforeRetry+1&&f.data->writeOk&&readBytes(second)==f.data->bytes,"retry starts new producer with actual bytes");
        f.adapter->cancelPendingRecording();
        CHECK_ADAPTER(readBytes(second)==f.data->bytes,"retry partial retained until own Idle");
        f.recorder->report(PipeWireBaseEncodedStream::Idle);
        CHECK_ADAPTER(QTest::qWaitFor([&]{return !QFileInfo::exists(second);},1000),"retry own Idle removes its reservation");
        QFile::remove(sibling);
        passed.append(QStringLiteral("owned-file partial Cancel/late Recording/Idle cleanup, fresh retry, stale callbacks and unknown sibling"));
    }
    {
        Fixture f;CHECK_ADAPTER(f.initialized(),"owned success init");f.data->writeOnStart=true;
        QSignalSpy saved(f.adapter.get(),&VideoPlatform::recordingSaved);
        CHECK_ADAPTER(startOwned(f),"owned success prepared");const QString output=f.data->output;
        Q_EMIT f.caster->source->created(42);CHECK_ADAPTER(f.data->writeOk,"owned success actual bytes");
        f.recorder->report(PipeWireBaseEncodedStream::Recording);f.adapter->finishRecording();
        f.recorder->report(PipeWireBaseEncodedStream::Rendering);
        CHECK_ADAPTER(readBytes(output)==f.data->bytes&&saved.isEmpty(),"owned Rendering bytes retained");
        f.recorder->report(PipeWireBaseEncodedStream::Idle);
        CHECK_ADAPTER(QTest::qWaitFor([&]{return saved.size()==1;},1000)&&readBytes(output)==f.data->bytes,"owned success retains bytes and saves once");
        f.recorder->report(PipeWireBaseEncodedStream::Idle);CHECK_ADAPTER(saved.size()==1,"owned duplicate Idle not saved twice");
        QFile::remove(output);
        for(int destruction=0;destruction<2;++destruction){
            Fixture recording;CHECK_ADAPTER(recording.initialized(),"recording failure preservation init");
            recording.data->writeOnStart=true;
            QSignalSpy failedSaved(recording.adapter.get(),&VideoPlatform::recordingSaved);
            CHECK_ADAPTER(startOwned(recording),"recording failure reservation");const QString retained=recording.data->output;
            Q_EMIT recording.caster->source->created(42);
            CHECK_ADAPTER(recording.data->writeOk,"recording failure controlled bytes");
            recording.recorder->report(PipeWireBaseEncodedStream::Recording);
            if(destruction)recording.adapter.reset();
            else{
                Q_EMIT recording.recorder->errorFound(QStringLiteral("synthetic failure after actual Recording state"));
                CHECK_ADAPTER(readBytes(retained)==recording.data->bytes,"Recording error retains bytes while producer drains");
                recording.recorder->report(PipeWireBaseEncodedStream::Idle);
                CHECK_ADAPTER(QTest::qWaitFor([&]{return recording.adapter->readyForRecording();},1000),"Recording error reaches Idle");
            }
            CHECK_ADAPTER(readBytes(retained)==recording.data->bytes&&failedSaved.isEmpty(),"Recording error or destruction preserves bytes without saved");
            QFile::remove(retained);
        }
        passed.append(QStringLiteral("owned-file positive Recording/Rendering/Idle retains actual bytes and saves once"));
    }
    {
        for(int scenario=0;scenario<2;++scenario){
            Fixture f;CHECK_ADAPTER(f.initialized(),"owned prestart failure init");QSignalSpy saved(f.adapter.get(),&VideoPlatform::recordingSaved);
            CHECK_ADAPTER(startOwned(f),"owned prestart reservation");const QString output=f.data->output;
            if(scenario==0)Q_EMIT f.caster->source->failed(QStringLiteral("synthetic source failure before created"));
            else f.adapter->cancelPendingRecording();
            CHECK_ADAPTER(QTest::qWaitFor([&]{return !QFileInfo::exists(output);},1000)&&f.data->starts==0&&saved.isEmpty(),"source failure/cancel before start removes empty reservation");
        }
        for (int explicitPath = 0; explicitPath < 2; ++explicitPath) {
            Fixture f; CHECK_ADAPTER(f.initialized(),"null createStream fixture initialization");
            f.caster->returnNull=true;
            QSignalSpy errors(f.adapter.get(),&VideoPlatform::recordingFailed);
            QSignalSpy saved(f.adapter.get(),&VideoPlatform::recordingSaved);
            const QUrl target=explicitPath ? QUrl::fromLocalFile(folder.filePath(QStringLiteral("explicit-null-stream.mp4"))) : QUrl();
            f.adapter->startRecording(target,VideoPlatform::Region,{{QStringLiteral("rect"),rect}},false);
            CHECK_ADAPTER(QTest::qWaitFor([&]{return !errors.isEmpty();},1000),"null createStream failure reported");
            const QString reserved=f.data->lastNonemptyOutput;
            CHECK_ADAPTER(!reserved.isEmpty()&&f.data->starts==0&&saved.isEmpty(),"null stream never starts or saves");
            if(explicitPath)CHECK_ADAPTER(QFileInfo::exists(reserved)&&QFileInfo(reserved).size()==0,"null stream keeps explicit empty reservation");
            else CHECK_ADAPTER(!QFileInfo::exists(reserved),"null stream removes private empty reservation");
        }
        {
            Fixture f; CHECK_ADAPTER(f.initialized(),"pending destructor writer initialization");
            f.data->writeOnStart=true;f.data->destructorTail=QByteArrayLiteral(" final destructor drain");
            CHECK_ADAPTER(startOwned(f),"pending destructor reservation");const QString output=f.data->output;
            Q_EMIT f.caster->source->created(42);CHECK_ADAPTER(f.data->writeOk,"pending destructor starts controlled writer");
            f.adapter.reset();
            CHECK_ADAPTER(f.data->destructorTailWritten&&f.data->destructorFilePresent,"pending destructor drains writer while named partial still exists before close");
            CHECK_ADAPTER(!QFileInfo::exists(output),"pending destructor removes private file only after writer destruction");
        }
        passed.append(QStringLiteral("owned-file source failure/cancel/null stream remove private reservation, retain explicit empty output"));
    }
    {
        for(int kind=0;kind<4;++kind){
            Fixture f;CHECK_ADAPTER(f.initialized(),"owned replacement init");f.data->writeOnStart=true;
            QSignalSpy saved(f.adapter.get(),&VideoPlatform::recordingSaved);
            QSignalSpy warnings(f.adapter.get(),&VideoPlatform::temporaryOutputCleanupWarning);
            CHECK_ADAPTER(startOwned(f),"owned replacement reservation");const QString output=f.data->output;
            Q_EMIT f.caster->source->created(42);CHECK_ADAPTER(f.data->writeOk,"owned replacement initial partial");
            const QString other=folder.filePath(QStringLiteral("replacement-%1.mp4").arg(kind));
            const QByteArray replacement("replacement must remain");
            if(kind==3){
                CHECK_ADAPTER(::link(QFile::encodeName(output).constData(),QFile::encodeName(other).constData())==0,"owned multi-hardlink setup");
            }else{
                CHECK_ADAPTER(QFile::remove(output),"remove owned name for replacement");
                if(kind==0)CHECK_ADAPTER(writeBytes(output,replacement),"replace regular inode");
                if(kind==1){CHECK_ADAPTER(writeBytes(other,replacement),"symlink destination");CHECK_ADAPTER(QFile::link(other,output),"replace symlink");}
                if(kind==2){CHECK_ADAPTER(QDir().mkdir(output),"replace directory");CHECK_ADAPTER(writeBytes(output+QStringLiteral("/keep"),replacement),"directory content");}
            }
            f.adapter->cancelPendingRecording();f.recorder->report(PipeWireBaseEncodedStream::Idle);
            CHECK_ADAPTER(QTest::qWaitFor([&]{return f.adapter->readyForRecording();},1000)&&saved.isEmpty(),"replacement cancellation settles without saved");
            CHECK_ADAPTER(QTest::qWaitFor([&]{return !warnings.isEmpty();},1000),"replacement cleanup preservation emits nonfatal warning");
            if (!result.contains(QStringLiteral("cleanupWarning")))
                result.insert(QStringLiteral("cleanupWarning"), warnings.first().first().toString());
            if(kind==0)CHECK_ADAPTER(readBytes(output)==replacement,"replacement regular preserved");
            if(kind==1)CHECK_ADAPTER(QFileInfo(output).isSymLink()&&readBytes(other)==replacement,"replacement symlink and target preserved");
            if(kind==2)CHECK_ADAPTER(QFileInfo(output).isDir()&&readBytes(output+QStringLiteral("/keep"))==replacement,"replacement directory preserved");
            if(kind==3)CHECK_ADAPTER(readBytes(output)==f.data->bytes&&readBytes(other)==f.data->bytes,"multiply-linked original preserved");
            if(kind==2){QFile::remove(output+QStringLiteral("/keep"));QDir().rmdir(output);}else QFile::remove(output);
            QFile::remove(other);
        }
        passed.append(QStringLiteral("owned-file cleanup refuses replaced inode, symlink, directory and multiple hardlinks"));
    }
    {
        Fixture f;CHECK_ADAPTER(f.initialized(),"before-created replacement init");f.data->writeOnStart=true;
        QSignalSpy errors(f.adapter.get(),&VideoPlatform::recordingFailed);
        CHECK_ADAPTER(startOwned(f),"before-created reservation");const QString output=f.data->output;
        CHECK_ADAPTER(QFile::remove(output)&&writeBytes(output,QByteArrayLiteral("replacement before created")),"before-created replacement");
        Q_EMIT f.caster->source->created(42);
        CHECK_ADAPTER(QTest::qWaitFor([&]{return !errors.isEmpty();},1000)&&f.data->starts==0,"identity mismatch before created refuses recorder start");
        CHECK_ADAPTER(readBytes(output)==QByteArrayLiteral("replacement before created"),"before-created replacement not deleted");
        QFile::remove(output);
        passed.append(QStringLiteral("owned-file identity recheck before source-created refuses start without deleting replacement"));
    }
    {
        for(int kind=0;kind<3;++kind){
            const QString output=folder.filePath(QStringLiteral("explicit-existing-%1.mp4").arg(kind));
            const QString other=folder.filePath(QStringLiteral("explicit-target-%1.mp4").arg(kind));
            const QByteArray bytes("existing explicit bytes");
            if(kind==0)CHECK_ADAPTER(writeBytes(output,bytes),"explicit existing file");
            else {CHECK_ADAPTER(writeBytes(other,bytes),"explicit target");
                if(kind==1)CHECK_ADAPTER(QFile::link(other,output),"explicit symlink");
                else CHECK_ADAPTER(::link(QFile::encodeName(other).constData(),QFile::encodeName(output).constData())==0,"explicit hardlink");}
            Fixture f;CHECK_ADAPTER(f.initialized(),"explicit rejection init");f.data->writeOnStart=true;
            QSignalSpy errors(f.adapter.get(),&VideoPlatform::recordingFailed);
            f.adapter->startRecording(QUrl::fromLocalFile(output),VideoPlatform::Region,{{QStringLiteral("rect"),rect}},false);
            CHECK_ADAPTER(QTest::qWaitFor([&]{return !errors.isEmpty();},1000),"existing explicit commit rejected");
            CHECK_ADAPTER(f.data->streams==0&&f.data->starts==0&&readBytes(output)==bytes,"existing explicit no stream and bytes preserved");
            if(kind)CHECK_ADAPTER(readBytes(other)==bytes,"explicit link destination preserved");
        }
        passed.append(QStringLiteral("explicit-existing regular/symlink/hardlink rejected at commit without stream or byte changes"));
    }
    {
        const QString output=folder.filePath(QStringLiteral("explicit-new-partial.mp4"));
        Fixture f;CHECK_ADAPTER(f.initialized(),"new explicit init");f.data->writeOnStart=true;
        QSignalSpy saved(f.adapter.get(),&VideoPlatform::recordingSaved);
        f.adapter->startRecording(QUrl::fromLocalFile(output),VideoPlatform::Region,{{QStringLiteral("rect"),rect}},false);
        CHECK_ADAPTER(QTest::qWaitFor([&]{return bool(f.caster->source);},1000),"new explicit source");
        Q_EMIT f.caster->source->created(42);CHECK_ADAPTER(f.data->writeOk,"new explicit partial written");
        f.adapter->cancelPendingRecording();f.recorder->report(PipeWireBaseEncodedStream::Idle);
        CHECK_ADAPTER(QTest::qWaitFor([&]{return f.adapter->readyForRecording();},1000)&&saved.isEmpty()&&readBytes(output)==f.data->bytes,"new explicit partial never claimed by owned cleanup");
        passed.append(QStringLiteral("new explicit output partial survives canceled producer Idle"));
    }
    {
        Fixture f;CHECK_ADAPTER(f.initialized(),"pause adapter initialized");
        f.data->writeOnStart=true;
        const auto output=QUrl::fromLocalFile(folder.filePath(QStringLiteral("pause-resume-084.mp4")));
        QSignalSpy saved(f.adapter.get(),&VideoPlatform::recordingSaved);
        f.adapter->startRecording(output,VideoPlatform::Region,{{QStringLiteral("rect"),rect}},false);
        CHECK_ADAPTER(QTest::qWaitFor([&]{return bool(f.caster->source);},1000),"pause test source selected");
        f.adapter->pauseRecording();f.adapter->resumeRecording();
        CHECK_ADAPTER(f.data->pauses==0&&f.data->resumes==0&&!f.adapter->pausePending(),"Starting refuses pause/resume");
        Q_EMIT f.caster->source->created(42);f.recorder->report(PipeWireBaseEncodedStream::Recording);
        CHECK_ADAPTER(f.adapter->supportsPause()&&f.adapter->isRecording(),"pause supported only after real recorder facade initialization");
        f.data->pauseSupported=false;f.adapter->pauseRecording();
        CHECK_ADAPTER(f.data->pauses==0&&!f.adapter->pausePending(),"unsupported producer refuses pause");
        f.data->pauseSupported=true;QTest::qWait(120);
        f.adapter->pauseRecording();f.adapter->pauseRecording();f.adapter->resumeRecording();
        CHECK_ADAPTER(f.data->pauses==1&&f.data->resumes==0&&f.adapter->pausePending()&&!f.adapter->isPaused(),"pause request waits for producer ack and rejects duplicates");
        bool publishedPaused=false;
        const auto pauseObserver=QObject::connect(f.adapter.get(),&VideoPlatform::pausePendingChanged,f.adapter.get(),[&]{
            if(!f.adapter->pausePending()) publishedPaused=f.adapter->isPaused();
        });
        f.recorder->report(PipeWireBaseEncodedStream::Paused);
        QObject::disconnect(pauseObserver);
        CHECK_ADAPTER(publishedPaused,"pause pending completion publishes the already-committed state");
        const auto pausedTime=f.adapter->recordedTime();
        CHECK_ADAPTER(f.adapter->isRecording()&&f.adapter->isPaused()&&!f.adapter->pausePending()
            &&f.adapter->recordingMode()==VideoPlatform::Region&&f.adapter->preparation()->phase()==Prep::Recording,"Paused retains active attempt, mode and Stop semantics");
        CHECK_ADAPTER(pausedTime>=100,"pause acknowledges elapsed active segment");
        QTest::qWait(1100);CHECK_ADAPTER(f.adapter->recordedTime()==pausedTime,"Paused clock excludes waiting wall time");
        f.adapter->startRecording(QUrl::fromLocalFile(folder.filePath(QStringLiteral("must-not-start-paused.mp4"))),VideoPlatform::Region,{{QStringLiteral("rect"),rect}},false);
        CHECK_ADAPTER(f.data->streams==1&&f.data->starts==1&&!f.adapter->readyForRecording(),"Paused refuses new recording");
        f.adapter->resumeRecording();f.adapter->resumeRecording();f.adapter->pauseRecording();
        CHECK_ADAPTER(f.data->resumes==1&&f.data->pauses==1&&f.adapter->pausePending()&&f.adapter->isPaused(),"resume request does not fabricate running state");
        f.recorder->report(PipeWireBaseEncodedStream::Recording);
        CHECK_ADAPTER(!f.adapter->isPaused()&&!f.adapter->pausePending()&&f.adapter->isRecording()
            &&f.adapter->recordedTime()==pausedTime&&f.adapter->preparation()->phase()==Prep::Recording,"resume ack preserves preparation and accumulated time");
        QTest::qWait(120);f.adapter->finishRecording();f.recorder->report(PipeWireBaseEncodedStream::Rendering);
        CHECK_ADAPTER(f.adapter->recordedTime()>=pausedTime+100,"resumed active time accumulates instead of resetting");
        f.adapter->pauseRecording();f.adapter->resumeRecording();
        CHECK_ADAPTER(f.data->pauses==1&&f.data->resumes==1&&!f.adapter->pausePending(),"Rendering refuses pause/resume");
        f.recorder->report(PipeWireBaseEncodedStream::Idle);
        CHECK_ADAPTER(saved.size()==1&&saved.first().first().toUrl()==output&&readBytes(output.toLocalFile())==f.data->bytes,"pause/resume successful drain saves existing bytes exactly once");
        f.recorder->report(PipeWireBaseEncodedStream::Idle);CHECK_ADAPTER(saved.size()==1,"pause resume duplicate Idle not saved twice");
        passed.append(QStringLiteral("real-adapter pause/resume acknowledgements, frozen cumulative active clock, no new start, unsupported/Starting/Rendering rejection"));
    }
    {
        Fixture f;CHECK_ADAPTER(f.initialized(),"Paused Stop adapter initialized");f.data->writeOnStart=true;
        QSignalSpy saved(f.adapter.get(),&VideoPlatform::recordingSaved);
        const auto output=QUrl::fromLocalFile(folder.filePath(QStringLiteral("paused-stop-084.mp4")));
        f.adapter->startRecording(output,VideoPlatform::Region,{{QStringLiteral("rect"),rect}},false);
        CHECK_ADAPTER(QTest::qWaitFor([&]{return bool(f.caster->source);},1000),"Paused Stop source prepared");
        Q_EMIT f.caster->source->created(42);f.recorder->report(PipeWireBaseEncodedStream::Recording);
        f.adapter->pauseRecording();f.recorder->report(PipeWireBaseEncodedStream::Paused);
        const auto stoppedTime=f.adapter->recordedTime();
        f.adapter->resumeRecording();CHECK_ADAPTER(f.adapter->pausePending(),"Paused Stop includes pending resume");
        f.adapter->finishRecording();
        CHECK_ADAPTER(f.data->stops>0&&!f.adapter->pausePending(),"Stop remains available during pending resume");
        f.recorder->report(PipeWireBaseEncodedStream::Recording);
        CHECK_ADAPTER(f.adapter->isPaused()&&f.adapter->recordedTime()==stoppedTime,"late resume ack cannot revive stopped recording");
        f.recorder->report(PipeWireBaseEncodedStream::Idle);
        CHECK_ADAPTER(saved.size()==1&&f.adapter->readyForRecording()&&readBytes(output.toLocalFile())==f.data->bytes,"Paused direct Stop Idle retains and saves output once");
        passed.append(QStringLiteral("real-adapter Stop while Paused/pending resume ignores late ack and saves once on Idle"));
    }
    result.insert(QStringLiteral("passed"),true);result.insert(QStringLiteral("checks"),passed);result.insert(QStringLiteral("nativeAcceptance"),false);result.insert(QStringLiteral("watchdog30sExecuted"),false);result.insert(QStringLiteral("watchdogInjected40msExecuted"),true);if(report)*report=result;return true;
#undef CHECK_ADAPTER
}
