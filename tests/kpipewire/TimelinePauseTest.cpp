// SPDX-License-Identifier: GPL-2.0-or-later
// Synthetic media exercises the production producer, encoders and MP4 muxer.
// No desktop/PipeWire server is fabricated, and no native capture is claimed.
#include <QtTest>
#include <QProcess>
#include <QTemporaryDir>
#include <QScopeGuard>
#include <cmath>
#include "pipewirerecord_p.h"
#include "pipewireaudiosourcestream_p.h"
#include "encoder_p.h"
#include "audioencoder_p.h"
#include "pwhelpers.h"
extern "C" {
#include <libavcodec/avcodec.h>
}
using namespace std::chrono_literals;
class FixtureProducer : public PipeWireRecordProduce {
public:
    mutable Clock::time_point now = Clock::time_point(100s);
    std::chrono::nanoseconds readStep = 0ns;
    bool manualPump = true;
    explicit FixtureProducer(const QString &path, AudioSources sources = AudioSource::SystemAudio,
                             PipeWireBaseEncodedStream::Encoder encoder = PipeWireBaseEncodedStream::H264Main)
        : PipeWireRecordProduce(encoder, 0, quint64(-1), 0, {10,1}, path, sources) {}
    Clock::time_point monotonicNow() const override { const auto result=now; now+=readStep; return result; }
    void at(std::chrono::nanoseconds t) { now = Clock::time_point(100s + t); }
    bool open() {
        setMaxFramerate({10,1});
        if (!initializeEncoder({64,48})) return false;
        m_audioInputStates.resize(m_audioSources.testFlag(AudioSource::SystemAudio) + m_audioSources.testFlag(AudioSource::Microphone));
        recordEpoch();
        return true;
    }
    void pump() {
        for (int i=0; i<4; ++i) {
            auto [filtered,queued] = m_encoder->encodeFrame(1000);
            m_pendingFilterFrames -= filtered;
            m_pendingEncodeFrames += queued;
            m_pendingEncodeFrames -= m_encoder->receivePacket();
            if (m_audioEncoder) {
                m_audioEncoder->encodeFrame(100000);
                m_audioEncoder->receivePacket();
            }
        }
    }
    void video(std::chrono::nanoseconds t, QColor color) {
        QImage image(64,48,QImage::Format_RGBA8888);
        image.fill(color);
        PipeWireFrame frame{};
        frame.format=SPA_VIDEO_FORMAT_RGBA;
        frame.presentationTimestamp=100s+t;
        auto owned=std::make_shared<QImage>(image);
        frame.dataFrame=std::make_shared<PipeWireFrameData>(frame.format,owned->bits(),owned->size(),owned->bytesPerLine(),
            new PipeWireFrameCleanupFunction([owned] {}));
        processFrame(frame);
        if (manualPump) pump();
    }
    void audio(std::chrono::nanoseconds t,int count,double frequency,int input=0) {
        std::vector<float> samples(count*2);
        for(int i=0;i<count;++i) {
            const auto phase=std::chrono::duration<double>(t).count()+double(i)/48000;
            samples[i*2]=samples[i*2+1]=float(.3*std::sin(phase*frequency*2*3.141592653589793));
        }
        PipeWireAudioFrame frame{samples.data(),quint32(count),2,48000,100s+t};
        processAudioFrame(input,frame);
        if (manualPump) pump();
    }
    void close() {
        m_stopTimepoint=now;
        drainPendingAudio();
        m_pendingAudio.clear();
        m_encoder->finish();
        for(int i=0;i<100;++i) {
            auto [filtered,queued] = m_encoder->encodeFrame(10000);
            int received=m_encoder->receivePacket();
            if (!filtered && !queued && !received) break;
        }
        finishEncoding();
    }
};
struct ThreadDrainGate {
    QSemaphore packetEntered;
    QSemaphore packetRelease;
    std::atomic_int packetCount = 0;
    std::atomic_bool gateTimedOut = false;
    std::atomic_bool initialized = false;
    std::atomic_bool pendingAtStop = false;
};
class ThreadDrainProducer : public FixtureProducer {
public:
    std::shared_ptr<ThreadDrainGate> gate;
    ThreadDrainProducer(const QString &path, std::shared_ptr<ThreadDrainGate> value)
        : FixtureProducer(path), gate(std::move(value)) {
        manualPump=false;
        setEncodingPreference(PipeWireBaseEncodedStream::Speed);
    }
    void processPacket(AVPacket *packet) override {
        // Hold only the final expected packet. Earlier packets are consumed
        // normally, so the test does not manufacture codec backpressure or
        // EAGAIN by blocking the output consumer for an entire input burst.
        if (++gate->packetCount == 10) {
            gate->packetEntered.release();
            // Bounded even if an assertion fails in the coordinating test.
            if (!gate->packetRelease.tryAcquire(1,5000)) gate->gateTimedOut=true;
        }
        PipeWireRecordProduce::processPacket(packet);
    }
};
// This separate control-only seam tests the production Base queued state
// protocol; it makes no encoder or successful native capture claim.
class ControlProducer : public PipeWireProduce {
public:
    ControlProducer() : PipeWireProduce(PipeWireBaseEncodedStream::H264Main,1,quint64(-1),0,{10,1}) {}
    void initialize() override {
        m_stream.reset(new PipeWireSourceStream);
        Q_EMIT started();
    }
    void processPacket(AVPacket *) override {}
};
class ControlStream : public PipeWireBaseEncodedStream {
public:
    ControlProducer *producer = nullptr;
    ControlStream() { setNodeId(1); setEncoder(H264Main); }
    std::unique_ptr<PipeWireProduce> makeProduce() override {
        auto next=std::make_unique<ControlProducer>(); producer=next.get(); return next;
    }
};
class TimelinePauseTest : public QObject {
    Q_OBJECT
    QByteArray decode(const QString &file, bool video) {
        QProcess p;
        QStringList args{"-v","error","-i",file};
        args << (video ? QStringList{"-an","-fps_mode","passthrough","-f","rawvideo","-pix_fmt","rgb24","pipe:1"}
                       : QStringList{"-vn","-f","f32le","-ac","1","-ar","48000","pipe:1"});
        p.start("ffmpeg",args);
        if(!p.waitForFinished(30000)||p.exitCode()!=0) return {};
        return p.readAllStandardOutput();
    }
    double tone(const QByteArray &bytes,double hz) {
        double x=0,y=0; const int count=bytes.size()/4;
        for(int i=0;i<count;++i) {
            float v; memcpy(&v,bytes.constData()+i*4,4);
            x+=v*std::cos(2*3.141592653589793*hz*i/48000);
            y+=v*std::sin(2*3.141592653589793*hz*i/48000);
        }
        return std::hypot(x,y)/std::max(1,count);
    }
private Q_SLOTS:
    void initTestCase() {
        QVERIFY2(!QStandardPaths::findExecutable("ffmpeg").isEmpty(),"Real decode CLI required; no fake pass");
        QVERIFY(avcodec_find_encoder_by_name("libx264"));
        QVERIFY(avcodec_find_encoder_by_name("aac"));
        qputenv("KPIPEWIRE_FORCE_ENCODER","libx264");
    }
    void nativeSparseWebmArtifact() {
        QVERIFY(avcodec_find_encoder_by_name("libvpx-vp9"));
        QVERIFY(avcodec_find_encoder_by_name("libopus"));
        const auto previous=qgetenv("KPIPEWIRE_FORCE_ENCODER");
        qputenv("KPIPEWIRE_FORCE_ENCODER","libvpx-vp9");
        const auto restore=qScopeGuard([&]{qputenv("KPIPEWIRE_FORCE_ENCODER",previous);});
        const auto path=qEnvironmentVariable("NKSHOT_WEBM_ARTIFACT",QStringLiteral("/logs/086-native-encoded.webm"));
        QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
        FixtureProducer producer(path,AudioSource::SystemAudio,PipeWireBaseEncodedStream::VP9);
        producer.setEncodingPreference(PipeWireBaseEncodedStream::Speed);
        QVERIFY(producer.open());
        for(int tick=0;tick<60;++tick) {
            const auto time=std::chrono::milliseconds(tick*100);
            producer.at(time+100ms);
            if(tick==0 || tick==20 || tick==40)
                producer.video(time,tick==0 ? Qt::red : tick==20 ? Qt::green : Qt::blue);
            const bool pulse=(tick>=20 && tick<22) || (tick>=40 && tick<42);
            producer.audio(time,4800,pulse ? 440 : 0);
        }
        producer.close();
        // These are the actual muxer's three sparse frames, not a CLI reencode.
        const auto pixels=decode(path,true);
        QCOMPARE(pixels.size(),3*64*48*3);
        const auto audio=decode(path,false);
        QVERIFY(std::abs(double(audio.size())/(4*48000)-6.)<.05);
        const auto rms=[&](double begin,double end) {
            const int first=int(begin*48000),last=int(end*48000);
            if(audio.size()<last*4) return 0.;
            double energy=0;
            for(int sample=first;sample<last;++sample) {
                float value; memcpy(&value,audio.constData()+sample*4,4);
                energy+=double(value)*value;
            }
            return std::sqrt(energy/(last-first));
        };
        QVERIFY(rms(2.03,2.15)>.1); QVERIFY(rms(4.03,4.15)>.1);
        QVERIFY(rms(1.,1.8)<.005); QVERIFY(rms(3.,3.8)<.005);
        QProcess probe;
        probe.start(QStringLiteral("ffprobe"),{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-select_streams"),
            QStringLiteral("v:0"),QStringLiteral("-show_frames"),QStringLiteral("-show_entries"),
            QStringLiteral("frame=best_effort_timestamp_time"),QStringLiteral("-of"),QStringLiteral("json"),path});
        QVERIFY(probe.waitForFinished(30000)); QCOMPARE(probe.exitCode(),0);
        const auto frames=QJsonDocument::fromJson(probe.readAllStandardOutput()).object()[QStringLiteral("frames")].toArray();
        QCOMPARE(frames.size(),3);
        for(int index=0;index<3;++index) {
            const double pts=frames[index].toObject()[QStringLiteral("best_effort_timestamp_time")].toString().toDouble();
            QVERIFY(std::abs(pts-index*2.)<.01);
        }
        qInfo()<<"Production VP9/Opus sparse fixture retained:"<<path;
    }
    void actualBaseAcknowledgementAndStop() {
        ControlStream stream;
        QCOMPARE(stream.timelinePauseVersion(),1);
        stream.start(); QTRY_COMPARE(stream.state(),PipeWireBaseEncodedStream::Recording);
        QSemaphore entered,release;
        QMetaObject::invokeMethod(stream.producer,[&] { entered.release(); release.acquire(); },Qt::QueuedConnection);
        QVERIFY(entered.tryAcquire(1,2000));
        stream.pause(); QCOMPARE(stream.state(),PipeWireBaseEncodedStream::Recording);
        stream.pause(); // No second command while the first is awaiting ack.
        release.release(); QTRY_COMPARE(stream.state(),PipeWireBaseEncodedStream::Paused);
        stream.resume(); QCOMPARE(stream.state(),PipeWireBaseEncodedStream::Paused);
        QTRY_COMPARE(stream.state(),PipeWireBaseEncodedStream::Recording);
        QMetaObject::invokeMethod(stream.producer,[&] { entered.release(); release.acquire(); },Qt::QueuedConnection);
        QVERIFY(entered.tryAcquire(1,2000));
        stream.pause(); stream.stop();
        QCOMPARE(stream.state(),PipeWireBaseEncodedStream::Rendering);
        QSignalSpy state(&stream,&PipeWireBaseEncodedStream::stateChanged);
        release.release(); QTRY_COMPARE(stream.state(),PipeWireBaseEncodedStream::Idle);
        // The only subsequent state is Idle, never a resurrected Paused.
        QCOMPARE(state.size(),1);
    }
    void mappingAndFutureTails() {
        QTemporaryDir dir;
        FixtureProducer p(dir.filePath("boundaries.mp4")); QVERIFY(p.open());
        p.at(100ms); p.audio(0ms,4800,440);
        QCOMPARE(p.m_audioInputStates[0].sampleCount,4800);
        p.requestTimelinePause(true,1);
        p.at(200ms); p.requestTimelinePause(false,2);
        p.at(300ms);
        // A single real audio buffer crosses both boundaries. Only active
        // samples survive; the old prefix is a duplicate, not appended again.
        p.audio(50ms,12000,880);
        QCOMPARE(p.m_audioInputStates[0].sampleCount,9600);
        p.audio(250ms,4800,880); // Future 50 ms remains owned.
        QVERIFY(!p.m_pendingAudio.empty());
        p.requestTimelinePause(true,3);
        p.at(400ms); p.drainPendingAudio();
        QCOMPARE(p.m_audioInputStates[0].sampleCount,9600);
        QCOMPARE(p.activeTime(p.now),200ms);
        QVERIFY(!p.acceptsVideoTimestamp(100s+350ms));
        p.requestTimelinePause(false,4);
        QVERIFY(!p.acceptsVideoTimestamp(100s+50ms));
        QVERIFY(!p.acceptsVideoTimestamp(std::nullopt));
        QVERIFY(p.acceptsVideoTimestamp(100s+400ms));
        p.at(400ms+std::chrono::nanoseconds(20834));
        p.audio(400ms,1,440);
        QCOMPARE(p.m_audioInputStates[0].sampleCount,9601);
        p.close();
    }
    void pauseBoundaryUsesOneClockSnapshot() {
        QTemporaryDir dir;
        FixtureProducer p(dir.filePath("clock-boundary.mp4")); QVERIFY(p.open());
        p.at(100ms); p.audio(0ms,9600,440);
        QCOMPARE(p.m_audioInputStates[0].sampleCount,4800);
        p.readStep=10ms;
        p.requestTimelinePause(true,1);
        // The command boundary is exactly 100 ms. Any second clock read during
        // its drain would leak at least 480 paused samples into the encoder.
        QCOMPARE(p.m_audioInputStates[0].sampleCount,4800);
        p.readStep=0ns; p.at(300ms); p.drainPendingAudio();
        QCOMPARE(p.m_audioInputStates[0].sampleCount,4800);
        p.close();
    }
    void pausedStopDrainsActualWorkerQueues() {
        QTemporaryDir dir;
        const auto output=qEnvironmentVariable("NKSHOT_TIMELINE_ARTIFACT_DIR",dir.path());
        QDir().mkpath(output);
        const auto path=output+"/threaded-paused-stop.mp4";
        auto gate=std::make_shared<ThreadDrainGate>();
        QThread worker;
        auto producer=new ThreadDrainProducer(path,gate);
        producer->moveToThread(&worker);
        QPointer<ThreadDrainProducer> guarded=producer;
        QSignalSpy finished(producer,&PipeWireProduce::finished);
        QObject::connect(&worker,&QThread::finished,producer,&QObject::deleteLater);
        worker.start();
        QMetaObject::invokeMethod(producer,[producer,gate] {
            gate->initialized=producer->open();
            // Encoder size/framerate came from open(), not this unconnected
            // source. No daemon, target or PipeWire capture is simulated.
            producer->m_stream.reset(new PipeWireSourceStream);
            if (!gate->initialized) { producer->deactivate(); return; }
            producer->startThreads();
            auto feed=new QTimer(producer);
            feed->setInterval(100);
            QObject::connect(feed,&QTimer::timeout,producer,[producer,feed,tick=0]() mutable {
                const auto t=std::chrono::milliseconds(tick*100);
                producer->at(t+100ms);
                producer->video(t,Qt::red);
                producer->audio(t,4800,440);
                if (++tick == 10) feed->stop();
            });
            feed->start();
            // Fail-safe for a worker that never emits its first packet. This
            // still uses the production stop/drain path, not direct finish().
            QTimer::singleShot(6000,producer,[producer] {
                if (!producer->m_deactivated) producer->deactivate();
            });
        },Qt::QueuedConnection);
        const bool entered=gate->packetEntered.tryAcquire(1,4000);
        if (guarded) QMetaObject::invokeMethod(producer,[producer,gate] {
            producer->at(1s);
            producer->requestTimelinePause(true,42);
            producer->at(2s);
            gate->pendingAtStop=producer->m_pendingFilterFrames>0 || producer->m_pendingEncodeFrames>0;
            producer->deactivate();
            // Release only after production stop has observed pending queues.
            // No finished handler waits on this gate, avoiding join deadlocks.
            gate->packetRelease.release();
        },Qt::QueuedConnection);
        const bool exited=worker.wait(15000);
        gate->packetRelease.release();
        if (!exited) {
            // A stuck production drain is a real failure; never destroy a live
            // QThread or silently accept a file written by a partial finalizer.
            if (guarded) QMetaObject::invokeMethod(producer,&PipeWireProduce::deactivate,Qt::QueuedConnection);
            if (!worker.wait(5000)) qFatal("Production paused drain failed to quiesce within watchdog");
        }
        QCoreApplication::processEvents();
        QVERIFY(entered); QVERIFY(exited); QVERIFY(gate->initialized);
        QCOMPARE(gate->packetCount.load(),10);
        QVERIFY(gate->pendingAtStop); QVERIFY(!gate->gateTimedOut);
        QCOMPARE(finished.size(),1);
        const auto pixels=decode(path,true);
        QCOMPARE(pixels.size(),10*64*48*3);
        const auto audio=decode(path,false); QVERIFY(!audio.isEmpty());
        QVERIFY(std::abs(double(audio.size())/(4*48000)-1.)<.05);
        QVERIFY(tone(audio,440)>.03);
        QProcess probe;
        probe.start("ffprobe", {"-v","error","-select_streams","v:0","-show_frames",
            "-show_entries","frame=best_effort_timestamp_time","-of","json",path});
        QVERIFY(probe.waitForFinished(30000)); QCOMPARE(probe.exitCode(),0);
        const auto frames=QJsonDocument::fromJson(probe.readAllStandardOutput()).object()["frames"].toArray();
        QCOMPARE(frames.size(),10);
        for(int i=0;i<frames.size();++i) {
            QVERIFY(std::abs(frames[i].toObject()["best_effort_timestamp_time"].toString().toDouble()-i*.1)<.001);
        }
    }
    void realMuxedPause_data() {
        QTest::addColumn<bool>("twoInputs"); QTest::addColumn<bool>("stopPaused");
        QTest::newRow("one-resume")<<false<<false;
        QTest::newRow("two-resume")<<true<<false;
        QTest::newRow("two-stop-paused-ended-input")<<true<<true;
    }
    void realMuxedPause() {
        QFETCH(bool,twoInputs); QFETCH(bool,stopPaused);
        QTemporaryDir dir;
        const auto output=qEnvironmentVariable("NKSHOT_TIMELINE_ARTIFACT_DIR",dir.path());
        QDir().mkpath(output);
        const auto path=output+"/"+QString::fromLatin1(QTest::currentDataTag())+".mp4";
        FixtureProducer p(path,twoInputs ? AudioSource::SystemAudio|AudioSource::Microphone : AudioSources(AudioSource::SystemAudio));
        QVERIFY(p.open());
        QSignalSpy ack(&p,&PipeWireProduce::timelinePauseAcknowledged);
        for(int tick=0;tick<30;++tick) {
            const auto t=std::chrono::milliseconds(tick*100);
            if(tick==10) { p.at(t); p.requestTimelinePause(true,7); }
            if(tick==20 && !stopPaused) { p.at(t); p.requestTimelinePause(false,8); }
            p.at(t+100ms);
            const bool excluded=tick>=10 && (tick<20 || stopPaused);
            p.video(t,excluded ? Qt::green : tick<10 ? Qt::red : Qt::blue);
            p.audio(t,4800,excluded ? 2200 : tick<10 ? 440 : 880);
            if(twoInputs) p.audio(t,4800,excluded ? 3300 : tick<10 ? 660 : 1320,1);
            if(tick==15 && stopPaused && twoInputs) p.handleAudioStreamStopped(1);
        }
        QCOMPARE(ack.size(),stopPaused ? 1 : 2);
        if(!stopPaused) { // old paused samples delivered after resume must not return
            p.audio(1300ms,4800,2200);
            p.video(1300ms,Qt::green);
        }
        p.close();
        const auto pixels=decode(path,true); QVERIFY(!pixels.isEmpty());
        const auto audio=decode(path,false); QVERIFY(!audio.isEmpty());
        const int frameSize=64*48*3;
        QCOMPARE(pixels.size()/frameSize,stopPaused ? 10 : 20);
        QProcess probe;
        probe.start("ffprobe", {"-v","error","-select_streams","v:0","-show_frames",
            "-show_entries","frame=best_effort_timestamp_time","-of","json",path});
        QVERIFY(probe.waitForFinished(30000)); QCOMPARE(probe.exitCode(),0);
        const auto frames=QJsonDocument::fromJson(probe.readAllStandardOutput()).object()["frames"].toArray();
        QCOMPARE(frames.size(),stopPaused ? 10 : 20);
        for(int i=0;i<frames.size();++i) {
            const double pts=frames[i].toObject()["best_effort_timestamp_time"].toString().toDouble();
            QVERIFY2(std::abs(pts-i*.1)<.001,qPrintable(QString("Frame %1 PTS %2").arg(i).arg(pts)));
        }
        for(int i=0;i<pixels.size();i+=3) {
            const auto r=uchar(pixels[i]), g=uchar(pixels[i+1]), b=uchar(pixels[i+2]);
            QVERIFY2(!(g>r+30 && g>b+30),"Paused-only green frame entered production output");
        }
        const double seconds=double(audio.size())/(4*48000);
        QVERIFY2(std::abs(seconds-(stopPaused?1.:2.))<.05,qPrintable(QString::number(seconds)));
        QVERIFY(tone(audio,440)>.015);
        if(!stopPaused) QVERIFY(tone(audio,880)>.015);
        if(twoInputs) QVERIFY(tone(audio,660)>.015);
        if(twoInputs && !stopPaused) QVERIFY(tone(audio,1320)>.015);
        QVERIFY(tone(audio,2200)<.003);
        QVERIFY(tone(audio,3300)<.003);
    }
};
QTEST_MAIN(TimelinePauseTest)
#include "TimelinePauseTest.moc"
