// SPDX-License-Identifier: GPL-3.0-only
#include "Media/FrameInspectionSession.h"
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QFile>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QtTest>
#include <sys/stat.h>
class FrameInspectionSessionTest:public QObject {
    Q_OBJECT
    QTemporaryDir dir;
    QString ffmpeg,ffprobe;
    QByteArray execute(const QString &program,const QStringList &args){QProcess p;p.start(program,args);if(!p.waitForFinished(30000)||p.exitCode()!=0){qWarning()<<p.readAllStandardError();return {};}return p.readAllStandardOutput();}
    QByteArray hash(const QString &path){QFile f(path);if(!f.open(QIODevice::ReadOnly))return {};return QCryptographicHash::hash(f.readAll(),QCryptographicHash::Sha256);}
    bool wait(FrameInspectionSession &s){return QTest::qWaitFor([&]{return !s.busy();},15000)&&s.ready();}
    QString fixture(const QString &name,bool webm,bool offset=false){
        const auto path=dir.filePath(name);QStringList args{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-y"),QStringLiteral("-f"),QStringLiteral("lavfi"),QStringLiteral("-i"),QStringLiteral("testsrc2=size=160x90:rate=30000/1001:duration=2"),QStringLiteral("-an"),QStringLiteral("-threads"),QStringLiteral("2")};
        if(webm)args<<QStringLiteral("-vf")<<QStringLiteral("select='eq(mod(n,7),0)+eq(n,1)+eq(n,2)'")<<QStringLiteral("-fps_mode")<<QStringLiteral("vfr")<<QStringLiteral("-c:v")<<QStringLiteral("libvpx-vp9")<<QStringLiteral("-lossless")<<QStringLiteral("1")<<QStringLiteral("-deadline")<<QStringLiteral("realtime")<<QStringLiteral("-cpu-used")<<QStringLiteral("8");
        else args<<QStringLiteral("-c:v")<<QStringLiteral("libx264")<<QStringLiteral("-bf")<<QStringLiteral("3")<<QStringLiteral("-g")<<QStringLiteral("30");
        if(offset)args<<QStringLiteral("-output_ts_offset")<<QStringLiteral("5");
        args<<path;execute(ffmpeg,args);return path;
    }
private Q_SLOTS:
    void initTestCase(){QVERIFY(dir.isValid());ffmpeg=QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));ffprobe=QStandardPaths::findExecutable(QStringLiteral("ffprobe"));QVERIFY(!ffmpeg.isEmpty()&&!ffprobe.isEmpty());}
    void actualFrames_data(){QTest::addColumn<bool>("webm");QTest::addColumn<bool>("offset");QTest::newRow("bframe-2997")<<false<<false;QTest::newRow("offset")<<false<<true;QTest::newRow("vfr")<<true<<false;}
    void actualFrames(){
        QFETCH(bool,webm);QFETCH(bool,offset);const auto path=fixture(QString::fromLatin1(QTest::currentDataTag())+(webm?QStringLiteral(".webm"):QStringLiteral(".mp4")),webm,offset);const auto original=hash(path);QVERIFY(!original.isEmpty());
        const auto index=QJsonDocument::fromJson(execute(ffprobe,{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-select_streams"),QStringLiteral("v:0"),QStringLiteral("-show_frames"),QStringLiteral("-show_streams"),QStringLiteral("-show_entries"),QStringLiteral("frame=best_effort_timestamp:stream=time_base"),QStringLiteral("-of"),QStringLiteral("json"),path})).object();
        const auto frames=index.value(QStringLiteral("frames")).toArray();const auto tb=index.value(QStringLiteral("streams")).toArray().first().toObject().value(QStringLiteral("time_base")).toString();
        const auto rational=tb.split(u'/');const auto num=rational[0].toLongLong(),den=rational[1].toLongLong();
        QQmlEngine engine;FrameInspectionSession s(&engine);QVERIFY(s.begin(QUrl::fromLocalFile(path),0));QVERIFY2(wait(s),qPrintable(s.error()));QCOMPARE(s.frameCount(),frames.size());QCOMPARE(s.currentIndex(),0);QCOMPARE(s.positionMs(),0);QCOMPARE(s.sourceSize(),QSize(160,90));
        const auto timestamp=[](const QJsonValue &v){const auto x=v.toObject().value(QStringLiteral("best_effort_timestamp"));return x.isString()?x.toString().toLongLong():qint64(x.toDouble());};
        for(const int n:{1,int(frames.size()/2),int(frames.size()-1),0}) {
            s.requestFrame(n);if(n!=s.currentIndex()){QVERIFY(s.snapshot().isNull());QVERIFY(s.previewUrl().isEmpty());}QVERIFY2(wait(s),qPrintable(s.error()));QCOMPARE(s.currentIndex(),n);
            const auto pts=timestamp(frames[n]);const auto reference=execute(ffmpeg,{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-copyts"),QStringLiteral("-i"),path,QStringLiteral("-an"),QStringLiteral("-vf"),QStringLiteral("settb=expr=%1,select=eq(pts\\,%2)").arg(tb).arg(pts),QStringLiteral("-frames:v"),QStringLiteral("1"),QStringLiteral("-fps_mode"),QStringLiteral("passthrough"),QStringLiteral("-threads"),QStringLiteral("2"),QStringLiteral("-pix_fmt"),QStringLiteral("rgb24"),QStringLiteral("-f"),QStringLiteral("rawvideo"),QStringLiteral("pipe:1")});
            QCOMPARE(reference.size(),160*90*3);const auto image=s.snapshot().convertToFormat(QImage::Format_RGB888);for(int y=0;y<90;++y)QCOMPARE(QByteArray(reinterpret_cast<const char*>(image.constScanLine(y)),160*3),reference.mid(y*160*3,160*3));
            QCOMPARE(s.positionMs(),qRound64(double(pts-timestamp(frames[0]))*num*1000/den));QCOMPARE(hash(path),original);
        }
        s.requestFrame(1);s.requestFrame(frames.size()-1);QVERIFY2(wait(s),qPrintable(s.error()));QCOMPARE(s.currentIndex(),frames.size()-1);
        const auto old=s.previewUrl();s.clear();QVERIFY(!s.ready());QVERIFY(s.snapshot().isNull());QVERIFY(s.previewUrl().isEmpty());QCOMPARE(s.frameCount(),0);QVERIFY(!old.isEmpty());
        QVERIFY(s.begin(QUrl::fromLocalFile(path),0));s.clear();QTest::qWait(200);QVERIFY(!s.ready());QVERIFY(!s.busy());
        QVERIFY(s.begin(QUrl::fromLocalFile(path),500));QVERIFY2(wait(s),qPrintable(s.error()));QVERIFY(s.positionMs()<=500);
        const int unchangedIndex=s.currentIndex();QVERIFY(unchangedIndex>=0);
        QFile changed(path);QVERIFY(changed.open(QIODevice::Append));changed.write("changed");changed.close();QVERIFY(s.snapshot().isNull());s.requestFrame(unchangedIndex);QVERIFY(!s.ready());QVERIFY(s.previewUrl().isEmpty());QVERIFY(s.error().contains(QStringLiteral("recording changed")));
    }
    void duplicateTimelineAndObserverLifetime(){
        const auto path=dir.filePath(QStringLiteral("duplicate.webm"));
        execute(ffmpeg,{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-y"),QStringLiteral("-f"),QStringLiteral("lavfi"),QStringLiteral("-i"),QStringLiteral("testsrc2=size=160x90:rate=10:duration=1"),QStringLiteral("-vf"),QStringLiteral("setpts=floor(N/2)/(10*TB)"),QStringLiteral("-fps_mode"),QStringLiteral("passthrough"),QStringLiteral("-c:v"),QStringLiteral("libvpx-vp9"),QStringLiteral("-threads"),QStringLiteral("2"),QStringLiteral("-deadline"),QStringLiteral("realtime"),path});
        QQmlEngine engine;FrameInspectionSession s(&engine);QVERIFY(s.begin(QUrl::fromLocalFile(path)));QVERIFY(QTest::qWaitFor([&]{return !s.busy();},15000));QVERIFY(!s.ready());QVERIFY(s.error().contains(QStringLiteral("timestamps")));
        QPointer<FrameInspectionSession> killed=new FrameInspectionSession(&engine);
        connect(killed,&FrameInspectionSession::changed,this,[&]{delete killed.data();});QVERIFY(!killed->begin(QUrl::fromLocalFile(path)));QVERIFY(!killed);
        killed=new FrameInspectionSession(&engine);bool armed=false;
        connect(killed,&FrameInspectionSession::changed,this,[&]{if(armed&&!killed->busy())delete killed.data();});
        QVERIFY(killed->begin(QUrl::fromLocalFile(path)));armed=true;QVERIFY(QTest::qWaitFor([&]{return killed.isNull();},15000));
        const auto valid=fixture(QStringLiteral("replacement.mp4"),false);
        QVERIFY(s.begin(QUrl::fromLocalFile(path)));QVERIFY(s.begin(QUrl::fromLocalFile(valid)));QVERIFY2(wait(s),qPrintable(s.error()));QVERIFY(s.frameCount()>20);
    }
    void outputLimitObserverStartsNewGeneration(){
        const auto valid=fixture(QStringLiteral("output-limit-replacement.mp4"),false);
        QTemporaryDir tools;QVERIFY(tools.isValid());
        QFile stub(tools.filePath(QStringLiteral("ffprobe")));QVERIFY(stub.open(QIODevice::WriteOnly));
        QVERIFY(stub.write("#!/bin/sh\nhead -c 1048577 /dev/zero\n")>0);stub.close();
        QVERIFY(stub.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner));
        struct RestorePath {QByteArray value=qgetenv("PATH");~RestorePath(){qputenv("PATH",value);}} restore;
        const QByteArray testPath=QFile::encodeName(tools.path())+QByteArrayLiteral(":")+restore.value;
        qputenv("PATH",testPath);
        QQmlEngine engine;FrameInspectionSession session(&engine);
        bool restarted=false,accepted=false;
        connect(&session,&FrameInspectionSession::changed,this,[&]{
            if(!restarted&&session.error().contains(QStringLiteral("safe output limit"))){
                restarted=true;qputenv("PATH",restore.value);accepted=session.begin(QUrl::fromLocalFile(valid));
            }
        });
        QVERIFY(session.begin(QUrl::fromLocalFile(valid)));
        auto *process=session.findChild<QProcess*>();QVERIFY(process);
        // Keep stdout buffered until finished(): exercise its synchronous failure observer.
        QObject::disconnect(process,&QProcess::readyReadStandardOutput,&session,nullptr);
        QObject::disconnect(process,&QProcess::readyReadStandardError,&session,nullptr);
        QVERIFY(QTest::qWaitFor([&]{return restarted&&!session.busy();},15000));
        QVERIFY(accepted);QVERIFY2(session.ready(),qPrintable(session.error()));
        QCOMPARE(session.currentIndex(),0);QVERIFY(!session.snapshot().isNull());
    }
    void invalidSources(){
        QQmlEngine engine;FrameInspectionSession s(&engine);
        for(const auto &url:{QUrl(QStringLiteral("https://example.org/test.mp4")),QUrl(QStringLiteral("file://server/x.mp4")),QUrl(QStringLiteral("file:relative.mp4")),QUrl(QStringLiteral("file:///tmp/x%00.mp4"))}){QVERIFY(!s.begin(url));QVERIFY(s.snapshot().isNull());}
        const auto path=fixture(QStringLiteral("invalid-fixture.mp4"),false);auto url=QUrl::fromLocalFile(path);url.setQuery(QStringLiteral("x"));QVERIFY(!s.begin(url));url=QUrl::fromLocalFile(path);url.setFragment(QStringLiteral("x"));QVERIFY(!s.begin(url));
        const auto link=dir.filePath(QStringLiteral("link.mp4"));QVERIFY(QFile::link(path,link));QVERIFY(!s.begin(QUrl::fromLocalFile(link)));
        const auto fifo=dir.filePath(QStringLiteral("pipe.mp4"));QVERIFY(::mkfifo(QFile::encodeName(fifo).constData(),0600)==0);QVERIFY(!s.begin(QUrl::fromLocalFile(fifo)));
        const auto bad=dir.filePath(QStringLiteral("bad.mp4"));QFile file(bad);QVERIFY(file.open(QIODevice::WriteOnly));file.write("not video");file.close();QVERIFY(s.begin(QUrl::fromLocalFile(bad)));QVERIFY(QTest::qWaitFor([&]{return !s.busy();},10000));QVERIFY(!s.ready());QVERIFY(!s.error().isEmpty());
        QVERIFY(s.begin(QUrl::fromLocalFile(path)));QVERIFY2(wait(s),qPrintable(s.error()));s.requestFrame(-1);QVERIFY(!s.ready());QVERIFY(!s.error().isEmpty());
    }
};
QTEST_GUILESS_MAIN(FrameInspectionSessionTest)
#include "FrameInspectionSessionTest.moc"
