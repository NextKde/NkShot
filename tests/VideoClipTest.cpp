// SPDX-License-Identifier: GPL-3.0-only
#include "Media/VideoClipJob.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cmath>
#include <memory>
#include <cstring>
#include <unistd.h>
#include <limits>
#include <QScopeGuard>
#include <signal.h>
#include <cerrno>

class VideoClipTest:public QObject {
    Q_OBJECT
    QTemporaryDir folder;
    QString source,silent,ffmpeg,ffprobe;
    QByteArray execute(const QString &program,const QStringList &args) {
        QProcess p;p.start(program,args);if(!p.waitForFinished(30000)||p.exitCode()!=0){qWarning()<<p.readAllStandardError();return {};}
        return p.readAllStandardOutput();
    }
    bool generate(const QString &path,bool audio) {
        QStringList a{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-y"),QStringLiteral("-f"),QStringLiteral("lavfi"),QStringLiteral("-i"),QStringLiteral("nullsrc=s=320x180:r=30:d=6,geq=lum='16+mod(N*3,200)':cb=128:cr=128")};
        if(audio)a<<QStringLiteral("-f")<<QStringLiteral("lavfi")<<QStringLiteral("-i")<<QStringLiteral("aevalsrc=if(between(t\\,2\\,2.2)\\,0.5*sin(2*PI*440*t)\\,0):s=48000:d=6");
        a<<QStringLiteral("-c:v")<<QStringLiteral("libx264")<<QStringLiteral("-pix_fmt")<<QStringLiteral("yuv420p")<<QStringLiteral("-g")<<QStringLiteral("300")<<QStringLiteral("-threads")<<QStringLiteral("2");
        if(audio)a<<QStringLiteral("-c:a")<<QStringLiteral("aac");
        a<<path;QProcess p;p.start(ffmpeg,a);const bool ok=p.waitForFinished(30000)&&p.exitCode()==0;if(!ok)qWarning()<<p.readAllStandardError();return ok;
    }
    QByteArray hash(const QString &p) { QFile f(p);if(!f.open(QIODevice::ReadOnly))return {};return QCryptographicHash::hash(f.readAll(),QCryptographicHash::Sha256); }
    bool ready(VideoClipJob &job,const QString &path) {job.inspect(QUrl::fromLocalFile(path));return QTest::qWaitFor([&]{return !job.busy();},20000)&&job.ready();}
private Q_SLOTS:
    void initTestCase() {
        QVERIFY(folder.isValid());ffmpeg=QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));ffprobe=QStandardPaths::findExecutable(QStringLiteral("ffprobe"));
        QVERIFY2(!ffmpeg.isEmpty()&&!ffprobe.isEmpty(),"Install ffmpeg/ffprobe to run real clip export tests");
        source=folder.filePath(QStringLiteral("original ; ' 中文.mp4"));silent=folder.filePath(QStringLiteral("silent.mp4"));
        QVERIFY(generate(source,true));QVERIFY(generate(silent,false));
    }
    void realAccurateClipWithAudio() {
        const auto original=hash(source);VideoClipJob job;QVERIFY2(ready(job,source),qPrintable(job.error()));QVERIFY(job.hasAudio());
        const auto target=folder.filePath(QStringLiteral("clip ; ' 中文.mp4"));QSignalSpy done(&job,&VideoClipJob::exported);
        job.exportClip(QUrl::fromLocalFile(target),1350,3150);QVERIFY(QTest::qWaitFor([&]{return !job.busy();},30000));
        QVERIFY2(job.error().isEmpty(),qPrintable(job.error()));QCOMPARE(done.size(),1);QCOMPARE(job.outputUrl(),QUrl::fromLocalFile(target));QCOMPARE(hash(source),original);
        auto metadata=QJsonDocument::fromJson(execute(ffprobe,{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-show_streams"),QStringLiteral("-show_format"),QStringLiteral("-of"),QStringLiteral("json"),target})).object();
        QCOMPARE(metadata.value(QStringLiteral("streams")).toArray().size(),2);
        QVERIFY(qAbs(metadata.value(QStringLiteral("format")).toObject().value(QStringLiteral("duration")).toString().toDouble()-1.8)<.06);
        // The generated picture is spatially uniform with frame-index encoded luma.
        const auto frames=execute(ffmpeg,{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-i"),target,QStringLiteral("-an"),QStringLiteral("-pix_fmt"),QStringLiteral("yuv420p"),QStringLiteral("-f"),QStringLiteral("rawvideo"),QStringLiteral("pipe:1")});
        const int frameBytes=320*180*3/2;const int count=frames.size()/frameBytes;QVERIFY(count>=53&&count<=55);
        QVERIFY(qAbs(int(uchar(frames.at(1000)))-(16+(41*3)%200))<=4);
        QVERIFY(qAbs(int(uchar(frames.at((count-1)*frameBytes+1000)))-(16+((41+count-1)*3)%200))<=4);
        // Input audio pulse at 2.0–2.2s must move to 0.65–0.85s, not start or disappear.
        const auto pcm=execute(ffmpeg,{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-i"),target,QStringLiteral("-vn"),QStringLiteral("-ac"),QStringLiteral("1"),QStringLiteral("-ar"),QStringLiteral("48000"),QStringLiteral("-f"),QStringLiteral("f32le"),QStringLiteral("pipe:1")});
        const auto rms=[&](double from,double to){double sum=0;int n=0;for(int i=int(from*48000);i<int(to*48000)&&4*i+4<=pcm.size();++i){float v;std::memcpy(&v,pcm.constData()+4*i,4);sum+=double(v)*v;++n;}return n?std::sqrt(sum/n):0.;};
        QVERIFY(rms(.68,.82)>.15);QVERIFY(rms(.1,.5)<.02);QVERIFY(rms(1.,1.4)<.02);
    }
    void silentShortClip() {
        VideoClipJob job;QVERIFY2(ready(job,silent),qPrintable(job.error()));QVERIFY(!job.hasAudio());
        const auto target=folder.filePath(QStringLiteral("short.mp4"));job.exportClip(QUrl::fromLocalFile(target),1000,1200);
        QVERIFY(QTest::qWaitFor([&]{return !job.busy();},30000));QVERIFY2(job.error().isEmpty(),qPrintable(job.error()));
        auto streams=QJsonDocument::fromJson(execute(ffprobe,{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-show_streams"),QStringLiteral("-of"),QStringLiteral("json"),target})).object().value(QStringLiteral("streams")).toArray();QCOMPARE(streams.size(),1);
    }
    void lowSampleRateAudioResampled() {
        for(const int rate:{16000,22050}) {
            const auto input=folder.filePath(QStringLiteral("low-%1.mp4").arg(rate));
            execute(ffmpeg,{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-y"),QStringLiteral("-i"),source,QStringLiteral("-c:v"),QStringLiteral("copy"),QStringLiteral("-c:a"),QStringLiteral("aac"),QStringLiteral("-ar"),QString::number(rate),input});
            const auto sourceHash=hash(input);QVERIFY(!sourceHash.isEmpty());VideoClipJob job;QVERIFY2(ready(job,input),qPrintable(job.error()));
            const auto target=folder.filePath(QStringLiteral("low-%1-clip.mp4").arg(rate));job.exportClip(QUrl::fromLocalFile(target),1350,3150);
            QVERIFY(QTest::qWaitFor([&]{return !job.busy();},30000));QVERIFY2(job.error().isEmpty(),qPrintable(job.error()));QCOMPARE(hash(input),sourceHash);
            const auto metadata=QJsonDocument::fromJson(execute(ffprobe,{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-show_streams"),QStringLiteral("-show_format"),QStringLiteral("-of"),QStringLiteral("json"),target})).object();
            bool checkedAudio=false;for(const auto &value:metadata.value(QStringLiteral("streams")).toArray()) {
                const auto stream=value.toObject();if(stream.value(QStringLiteral("codec_type")).toString()!=QStringLiteral("audio"))continue;
                QCOMPARE(stream.value(QStringLiteral("sample_rate")).toString(),QStringLiteral("48000"));QCOMPARE(stream.value(QStringLiteral("codec_name")).toString(),QStringLiteral("aac"));checkedAudio=true;
            }
            QVERIFY(checkedAudio);QVERIFY(qAbs(metadata.value(QStringLiteral("format")).toObject().value(QStringLiteral("duration")).toString().toDouble()-1.8)<.04);
            const auto pcm=execute(ffmpeg,{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-i"),target,QStringLiteral("-vn"),QStringLiteral("-ac"),QStringLiteral("1"),QStringLiteral("-f"),QStringLiteral("f32le"),QStringLiteral("pipe:1")});
            double sum=0;int n=0;for(int i=int(.68*48000);i<int(.82*48000)&&4*i+4<=pcm.size();++i){float v;std::memcpy(&v,pcm.constData()+4*i,4);sum+=double(v)*v;++n;}
            QVERIFY(n>0);QVERIFY(std::sqrt(sum/n)>.1);
        }
    }
    void invalidRangesAndSourceAliases() {
        const auto original=hash(source);VideoClipJob job;QVERIFY(ready(job,source));
        for(auto range:QList<QPair<qint64,qint64>>{{-1,200},{0,50},{200,100},{0,9000},{std::numeric_limits<qint64>::max(),std::numeric_limits<qint64>::min()}}){job.exportClip(QUrl::fromLocalFile(folder.filePath(QStringLiteral("bad.mp4"))),range.first,range.second);QVERIFY(!job.busy());QVERIFY(!job.error().isEmpty());}
        job.exportClip(QUrl::fromLocalFile(source),0,1000);QVERIFY(!job.error().isEmpty());
        const auto alias=folder.filePath(QStringLiteral("hardlink.mp4"));QCOMPARE(::link(QFile::encodeName(source).constData(),QFile::encodeName(alias).constData()),0);
        job.exportClip(QUrl::fromLocalFile(alias),0,1000);QVERIFY(!job.error().isEmpty());QCOMPARE(hash(source),original);
        job.exportClip(QUrl(QStringLiteral("https://example.invalid/out.mp4")),0,1000);QVERIFY(!job.error().isEmpty());
        job.inspect(QUrl(QStringLiteral("https://example.invalid/input.mp4")));QVERIFY(!job.ready());QVERIFY(!job.busy());
    }
    void cancelAndReopen() {
        VideoClipJob job;QVERIFY(ready(job,source));const auto target=folder.filePath(QStringLiteral("cancel.mp4"));QSignalSpy done(&job,&VideoClipJob::exported);
        job.exportClip(QUrl::fromLocalFile(target),0,6000);QVERIFY(job.busy());job.cancel();QTest::qWait(300);QVERIFY(!job.busy());QVERIFY(!QFile::exists(target));QCOMPARE(done.size(),0);
        QVERIFY(QDir(folder.path()).entryList({QStringLiteral(".nkshot-clip-*")},QDir::Files|QDir::Hidden).isEmpty());
        QVERIFY(ready(job,silent));job.exportClip(QUrl::fromLocalFile(target),0,500);QVERIFY(QTest::qWaitFor([&]{return !job.busy();},30000));QVERIFY2(job.error().isEmpty(),qPrintable(job.error()));QCOMPARE(done.size(),1);
    }
    void existingDestinationAndRace() {
        VideoClipJob job;QVERIFY(ready(job,source));const auto target=folder.filePath(QStringLiteral("existing.mp4"));QFile file(target);QVERIFY(file.open(QIODevice::WriteOnly));file.write("keep");file.close();const auto original=hash(target);
        job.exportClip(QUrl::fromLocalFile(target),0,1000);QVERIFY(!job.error().isEmpty());QCOMPARE(hash(target),original);
        job.exportClip(QUrl::fromLocalFile(target),0,1000);QVERIFY(!job.busy());QVERIFY(!job.error().isEmpty());QCOMPARE(hash(target),original);
        const auto race=folder.filePath(QStringLiteral("racing.mp4"));job.exportClip(QUrl::fromLocalFile(race),0,6000);QFile racing(race);QVERIFY(racing.open(QIODevice::WriteOnly));racing.write("new owner");racing.close();const auto racedHash=hash(race);
        QVERIFY(QTest::qWaitFor([&]{return !job.busy();},30000));QVERIFY(!job.error().isEmpty());QCOMPARE(hash(race),racedHash);
        job.exportClip(QUrl::fromLocalFile(folder.filePath(QStringLiteral("missing/clip.mp4"))),0,1000);QVERIFY(!job.busy());QVERIFY(!job.error().isEmpty());
    }
    void reentrantCancellation() {
        VideoClipJob job;const auto first=folder.filePath(QStringLiteral("reentrant.mp4"));
        auto connection=connect(&job,&VideoClipJob::changed,&job,[&]{if(job.busy())job.cancel();});
        job.inspect(QUrl::fromLocalFile(source));QVERIFY(!job.busy());QVERIFY(!job.ready());disconnect(connection);
        QVERIFY(ready(job,source));
        connection=connect(&job,&VideoClipJob::changed,&job,[&]{if(job.busy())job.cancel();});
        job.exportClip(QUrl::fromLocalFile(first),0,1000);QVERIFY(!job.busy());QTest::qWait(200);QVERIFY(!QFile::exists(first));disconnect(connection);
        bool progressCanceled=false;
        connection=connect(&job,&VideoClipJob::changed,&job,[&]{if(job.busy()&&job.progress()>0){progressCanceled=true;job.cancel();}});
        job.exportClip(QUrl::fromLocalFile(first),0,6000);QVERIFY(QTest::qWaitFor([&]{return !job.busy();},30000));QVERIFY(progressCanceled);QVERIFY(!QFile::exists(first));QVERIFY(job.outputUrl().isEmpty());disconnect(connection);
        QSignalSpy exported(&job,&VideoClipJob::exported);bool completionCanceled=false;
        connection=connect(&job,&VideoClipJob::changed,&job,[&]{if(!job.outputUrl().isEmpty()){completionCanceled=true;job.cancel();}});
        job.exportClip(QUrl::fromLocalFile(first),0,1000);QVERIFY(QTest::qWaitFor([&]{return !job.busy();},30000));
        QVERIFY(completionCanceled);QVERIFY(QFile::exists(first));QVERIFY(job.outputUrl().isEmpty());QCOMPARE(exported.size(),0);disconnect(connection);
    }
    void corruptEncodedDataRejected() {
        const auto broken=folder.filePath(QStringLiteral("corrupt-mdat.mp4"));QFile original(source);QVERIFY(original.open(QIODevice::ReadOnly));auto data=original.readAll();
        const auto at=data.indexOf("mdat");QVERIFY(at>0);for(qsizetype i=at+4;i<qMin(data.size(),at+2004);++i)data[i]='\0';
        QFile altered(broken);QVERIFY(altered.open(QIODevice::WriteOnly));QCOMPARE(altered.write(data),data.size());altered.close();
        VideoClipJob job;job.inspect(QUrl::fromLocalFile(broken));QVERIFY(QTest::qWaitFor([&]{return !job.busy();},20000));
        const auto target=folder.filePath(QStringLiteral("corrupt-result.mp4"));
        if(job.ready()){job.exportClip(QUrl::fromLocalFile(target),0,2000);QVERIFY(QTest::qWaitFor([&]{return !job.busy();},30000));}
        QVERIFY(!job.error().isEmpty());QVERIFY(!QFile::exists(target));
    }
    void missingToolsAndEncoderFailure() {
        const auto oldPath=qgetenv("PATH");const bool hadPath=qEnvironmentVariableIsSet("PATH");
        const auto restore=qScopeGuard([&]{if(hadPath)qputenv("PATH",oldPath);else qunsetenv("PATH");});
        qputenv("PATH",QByteArray());VideoClipJob missing;missing.inspect(QUrl::fromLocalFile(source));QVERIFY(!missing.busy());QVERIFY(!missing.ready());QVERIFY(missing.error().contains(QStringLiteral("FFmpeg")));
        const auto bin=folder.filePath(QStringLiteral("test-bin"));QVERIFY(QDir().mkpath(bin));
        QFile script(bin+QStringLiteral("/ffmpeg"));QVERIFY(script.open(QIODevice::WriteOnly));
        script.write("#!/bin/sh\ncase \"$*\" in *-encoders*) printf ' V..... libx264 encoder\\n A..... aac encoder\\n'; exit 0;; esac\nexit 23\n");script.close();QVERIFY(script.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
        QByteArray testPath=bin.toUtf8();testPath.append(':');testPath.append(oldPath);qputenv("PATH",testPath);VideoClipJob failed;QVERIFY2(ready(failed,source),qPrintable(failed.error()));
        const auto target=folder.filePath(QStringLiteral("failed.mp4"));failed.exportClip(QUrl::fromLocalFile(target),0,1000);
        QVERIFY(QTest::qWaitFor([&]{return !failed.busy();},20000));QVERIFY(!failed.error().isEmpty());QVERIFY(!QFile::exists(target));
        QVERIFY(QDir(folder.path()).entryList({QStringLiteral(".nkshot-clip-*")},QDir::Files|QDir::Hidden).isEmpty());
    }
    void realProbeTimeoutTerminatesProcess() {
        const auto oldPath=qgetenv("PATH");const bool hadPath=qEnvironmentVariableIsSet("PATH");
        const auto oldPidFile=qgetenv("NKSHOT_TEST_CLIP_PIDFILE");const bool hadPidFile=qEnvironmentVariableIsSet("NKSHOT_TEST_CLIP_PIDFILE");
        const auto restore=qScopeGuard([&]{
            if(hadPath)qputenv("PATH",oldPath);else qunsetenv("PATH");
            if(hadPidFile)qputenv("NKSHOT_TEST_CLIP_PIDFILE",oldPidFile);else qunsetenv("NKSHOT_TEST_CLIP_PIDFILE");
        });
        const auto bin=folder.filePath(QStringLiteral("timeout-bin"));QVERIFY(QDir().mkpath(bin));
        const auto pidPath=folder.filePath(QStringLiteral("timeout.pid"));qputenv("NKSHOT_TEST_CLIP_PIDFILE",pidPath.toUtf8());
        QFile script(bin+QStringLiteral("/ffprobe"));QVERIFY(script.open(QIODevice::WriteOnly));
        script.write("#!/bin/sh\nprintf '%s' \"$$\" > \"$NKSHOT_TEST_CLIP_PIDFILE\"\nexec /bin/sleep 99\n");script.close();QVERIFY(script.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
        QByteArray path=bin.toUtf8();path.append(':');path.append(oldPath);qputenv("PATH",path);
        VideoClipJob job;QElapsedTimer elapsed;elapsed.start();job.inspect(QUrl::fromLocalFile(source));QVERIFY(job.busy());
        QVERIFY(QTest::qWaitFor([&]{return QFileInfo(pidPath).size()>0;},3000));QFile pidFile(pidPath);QVERIFY(pidFile.open(QIODevice::ReadOnly));bool validPid=false;const auto pid=pidFile.readAll().toLongLong(&validPid);QVERIFY(validPid&&pid>1);
        QVERIFY(QTest::qWaitFor([&]{return !job.busy();},20000));QVERIFY(elapsed.elapsed()>=14000);QVERIFY(job.error().contains(QStringLiteral("timed out")));QVERIFY(!job.ready());QVERIFY(job.outputUrl().isEmpty());
        QVERIFY(QTest::qWaitFor([&]{errno=0;return ::kill(pid_t(pid),0)==-1&&errno==ESRCH;},3000));
        QVERIFY(QDir(folder.path()).entryList({QStringLiteral(".nkshot-clip-*")},QDir::Files|QDir::Hidden).isEmpty());
    }
    void destructionDuringExportCleansTemporaryFile() {
        const auto original=hash(source);const auto target=folder.filePath(QStringLiteral("destroy-active.mp4"));
        auto job=std::make_unique<VideoClipJob>();QVERIFY(ready(*job,source));job->exportClip(QUrl::fromLocalFile(target),0,6000);QVERIFY(job->busy());
        QVERIFY(!QDir(folder.path()).entryList({QStringLiteral(".nkshot-clip-*")},QDir::Files|QDir::Hidden).isEmpty());
        job.reset();QTest::qWait(300);QVERIFY(!QFile::exists(target));QCOMPARE(hash(source),original);
        QVERIFY(QTest::qWaitFor([&]{return QDir(folder.path()).entryList({QStringLiteral(".nkshot-clip-*")},QDir::Files|QDir::Hidden).isEmpty();},3000));
    }
    void invalidMediaAndChangedSource() {
        const auto broken=folder.filePath(QStringLiteral("broken.mp4"));QFile f(broken);QVERIFY(f.open(QIODevice::WriteOnly));f.write("not media");f.close();VideoClipJob job;job.inspect(QUrl::fromLocalFile(broken));QVERIFY(QTest::qWaitFor([&]{return !job.busy();},20000));QVERIFY(!job.ready());QVERIFY(!job.error().isEmpty());
        const auto copy=folder.filePath(QStringLiteral("changed.mp4"));QVERIFY(QFile::copy(silent,copy));QVERIFY(ready(job,copy));QFile append(copy);QVERIFY(append.open(QIODevice::Append));append.write("changed");append.close();job.exportClip(QUrl::fromLocalFile(folder.filePath(QStringLiteral("changed-output.mp4"))),0,1000);QVERIFY(!job.busy());QVERIFY(!job.ready());
    }
};
QTEST_GUILESS_MAIN(VideoClipTest)
#include "VideoClipTest.moc"
