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
    QString source,silent,ffmpeg,ffprobe,cropMp4,cropWebm;
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
    struct GifDetails {bool valid=false,infinite=false;int width=0,height=0,frames=0;QList<int> delays;};
    GifDetails gifDetails(const QByteArray &data) {
        GifDetails out;if(data.size()<13||(!data.startsWith("GIF89a")&&!data.startsWith("GIF87a")))return out;
        const auto u16=[&](qsizetype at){return int(uchar(data[at]))+256*int(uchar(data[at+1]));};
        out.width=u16(6);out.height=u16(8);qsizetype pos=13;
        if(uchar(data[10])&128)pos+=3*(1<<((uchar(data[10])&7)+1));
        const auto blocks=[&](QByteArray &joined){while(pos<data.size()){const int n=uchar(data[pos++]);if(n==0)return true;if(pos+n>data.size())return false;joined+=data.mid(pos,n);pos+=n;}return false;};
        QByteArray loopMarker("NETSCAPE2.0");loopMarker.append(QByteArray::fromHex("010000"));
        while(pos<data.size()) {
            const int marker=uchar(data[pos++]);
            if(marker==0x3b){out.valid=out.frames>0&&out.delays.size()==out.frames;return out;}
            if(marker==0x21){if(pos>=data.size())return {};const int kind=uchar(data[pos++]);QByteArray payload;if(!blocks(payload))return {};
                if(kind==0xf9){if(payload.size()!=4)return {};out.delays.append(int(uchar(payload[1]))+256*int(uchar(payload[2])));}
                if(kind==0xff&&payload==loopMarker)out.infinite=true;
            } else if(marker==0x2c){if(pos+9>data.size())return {};const int flags=uchar(data[pos+8]);pos+=9;if(flags&128)pos+=3*(1<<((flags&7)+1));if(pos>=data.size())return {};pos++;QByteArray pixels;if(!blocks(pixels))return {};++out.frames;}
            else return {};
        }
        return {};
    }
    bool ready(VideoClipJob &job,const QString &path) {job.inspect(QUrl::fromLocalFile(path));return QTest::qWaitFor([&]{return !job.busy();},20000)&&job.ready();}
private Q_SLOTS:
    void initTestCase() {
        QVERIFY(folder.isValid());ffmpeg=QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));ffprobe=QStandardPaths::findExecutable(QStringLiteral("ffprobe"));
        QVERIFY2(!ffmpeg.isEmpty()&&!ffprobe.isEmpty(),"Install ffmpeg/ffprobe to run real clip export tests");
        source=folder.filePath(QStringLiteral("original ; ' 中文.mp4"));silent=folder.filePath(QStringLiteral("silent.mp4"));
        QVERIFY(generate(source,true));QVERIFY(generate(silent,false));
        cropMp4=folder.filePath(QStringLiteral("crop-spatial.mp4"));cropWebm=folder.filePath(QStringLiteral("crop-spatial.webm"));
        for(const bool webm:{false,true}) {
            QStringList args{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-y"),QStringLiteral("-f"),QStringLiteral("lavfi"),QStringLiteral("-i"),
                QStringLiteral("nullsrc=s=320x240:r=30:d=2,geq=lum='16+X/2+Y/4':cb=128:cr=128"),
                QStringLiteral("-f"),QStringLiteral("lavfi"),QStringLiteral("-i"),QStringLiteral("sine=frequency=440:sample_rate=48000:duration=2"),
                QStringLiteral("-c:v"),webm?QStringLiteral("libvpx-vp9"):QStringLiteral("libx264"),QStringLiteral("-pix_fmt"),QStringLiteral("yuv420p"),QStringLiteral("-threads"),QStringLiteral("2")};
            if(webm)args<<QStringLiteral("-lossless")<<QStringLiteral("1")<<QStringLiteral("-deadline")<<QStringLiteral("realtime")<<QStringLiteral("-cpu-used")<<QStringLiteral("8");
            else args<<QStringLiteral("-qp")<<QStringLiteral("0");
            args<<QStringLiteral("-c:a")<<(webm?QStringLiteral("libopus"):QStringLiteral("aac"))<<(webm?cropWebm:cropMp4);
            QProcess process;process.start(ffmpeg,args);QVERIFY(process.waitForFinished(30000));QVERIFY2(process.exitCode()==0,process.readAllStandardError().constData());
        }
    }
    void centerCrop_data() {
        QTest::addColumn<bool>("webm");QTest::addColumn<QString>("aspect");QTest::addColumn<QRect>("expected");
        const QList<QPair<QString,QRect>> modes{{QStringLiteral("original"),QRect(0,0,320,240)},
            {QStringLiteral("square"),QRect(40,0,240,240)},{QStringLiteral("portrait"),QRect(92,0,134,240)},
            {QStringLiteral("wide"),QRect(0,30,320,180)}};
        for(const bool webm:{false,true})for(const auto &mode:modes)
            QTest::newRow(qPrintable((webm?QStringLiteral("webm-"):QStringLiteral("mp4-"))+mode.first))<<webm<<mode.first<<mode.second;
    }
    void centerCrop() {
        QFETCH(bool,webm);QFETCH(QString,aspect);QFETCH(QRect,expected);
        const auto input=webm?cropWebm:cropMp4;const auto original=hash(input);VideoClipJob job;
        QVERIFY2(ready(job,input),qPrintable(job.error()));QCOMPARE(job.sourceSize(),QSize(320,240));QVERIFY(job.cropAvailable());
        QCOMPARE(job.cropAspect(),QStringLiteral("original"));QCOMPARE(job.cropRect(),QRect(0,0,320,240));
        job.setCropAspect(aspect);QCOMPARE(job.cropRect(),expected);QVERIFY(job.ready());
        job.setCropAspect(QStringLiteral("arbitrary"));QCOMPARE(job.cropAspect(),aspect);
        const auto sourceFrame=execute(ffmpeg,{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-i"),input,
            QStringLiteral("-frames:v"),QStringLiteral("1"),QStringLiteral("-an"),QStringLiteral("-pix_fmt"),QStringLiteral("gray"),QStringLiteral("-f"),QStringLiteral("rawvideo"),QStringLiteral("pipe:1")});
        QCOMPARE(sourceFrame.size(),320*240);
        for(const auto &format:{QStringLiteral("mp4"),QStringLiteral("gif")}) {
            job.setOutputFormat(format);QVERIFY(QTest::qWaitFor([&]{return !job.busy();},10000));QVERIFY2(job.ready(),qPrintable(job.error()));
            QCOMPARE(job.cropAspect(),aspect);QCOMPARE(job.cropRect(),expected);
            const auto target=folder.filePath(QString::fromLatin1(QTest::currentDataTag())+u'.'+format);
            job.exportClip(QUrl::fromLocalFile(target),200,1200);QVERIFY(job.busy());
            job.setCropAspect(aspect==QStringLiteral("square")?QStringLiteral("original"):QStringLiteral("square"));
            QCOMPARE(job.cropAspect(),aspect);QCOMPARE(job.cropRect(),expected);
            QVERIFY(QTest::qWaitFor([&]{return !job.busy();},30000));QVERIFY2(job.error().isEmpty(),qPrintable(job.error()));
            QCOMPARE(job.outputUrl(),QUrl::fromLocalFile(target));QCOMPARE(hash(input),original);QCOMPARE(job.sourceSize(),QSize(320,240));
            QStringList probe{QStringLiteral("-v"),QStringLiteral("error")};
            if(format==QStringLiteral("gif"))probe<<QStringLiteral("-ignore_loop")<<QStringLiteral("1");
            probe<<QStringLiteral("-show_streams")<<QStringLiteral("-of")<<QStringLiteral("json")<<target;
            const auto streams=QJsonDocument::fromJson(execute(ffprobe,probe)).object()[QStringLiteral("streams")].toArray();
            QCOMPARE(streams.size(),format==QStringLiteral("mp4")?2:1);
            const auto video=streams.first().toObject();QCOMPARE(video[QStringLiteral("width")].toInt(),expected.width());QCOMPARE(video[QStringLiteral("height")].toInt(),expected.height());
            QStringList decode{QStringLiteral("-v"),QStringLiteral("error")};
            if(format==QStringLiteral("gif"))decode<<QStringLiteral("-ignore_loop")<<QStringLiteral("1");
            decode<<QStringLiteral("-i")<<target<<QStringLiteral("-frames:v")<<QStringLiteral("1")<<QStringLiteral("-an")<<QStringLiteral("-pix_fmt")<<QStringLiteral("gray")<<QStringLiteral("-f")<<QStringLiteral("rawvideo")<<QStringLiteral("pipe:1");
            const auto output=execute(ffmpeg,decode);QCOMPARE(output.size(),expected.width()*expected.height());
            // Spatial gradients distinguish a real centre crop from squeezing the
            // complete source into a smaller output of the same dimensions.
            for(const QPoint point:{QPoint(8,8),QPoint(expected.width()/2,expected.height()/2),QPoint(expected.width()-9,expected.height()-9)}) {
                const int actual=uchar(output.at(point.y()*expected.width()+point.x()));
                const int wanted=uchar(sourceFrame.at((point.y()+expected.y())*320+point.x()+expected.x()));
                QVERIFY2(qAbs(actual-wanted)<=5,qPrintable(QStringLiteral("Crop pixel %1,%2 was %3, expected %4").arg(point.x()).arg(point.y()).arg(actual).arg(wanted)));
            }
        }
        job.setCropAspect(aspect==QStringLiteral("square")?QStringLiteral("original"):QStringLiteral("square"));
        QVERIFY(job.outputUrl().isEmpty());QCOMPARE(job.progress(),qreal(0));QVERIFY(job.error().isEmpty());
        job.inspect(QUrl::fromLocalFile(input));QCOMPARE(job.cropAspect(),QStringLiteral("original"));QVERIFY(job.cropRect().isEmpty());QCOMPARE(job.sourceSize(),QSize(0,0));
        QVERIFY(QTest::qWaitFor([&]{return !job.busy();},10000));QVERIFY(job.ready());QCOMPARE(job.cropRect(),QRect(0,0,320,240));
    }
    void movableCrop_data() {
        QTest::addColumn<bool>("webm");QTest::addColumn<QString>("aspect");QTest::addColumn<QRect>("expected");
        const QList<QPair<QString,QRect>> cases{{QStringLiteral("square"),QRect(0,0,240,240)},
            {QStringLiteral("square"),QRect(80,0,240,240)},{QStringLiteral("wide"),QRect(0,0,320,180)},
            {QStringLiteral("wide"),QRect(0,60,320,180)}};
        int index=0;
        for(const bool webm:{false,true})for(const auto &test:cases)
            QTest::newRow(qPrintable(QStringLiteral("move-%1-%2").arg(webm?QStringLiteral("webm"):QStringLiteral("mp4")).arg(index++)))<<webm<<test.first<<test.second;
    }
    void movableCrop() {
        QFETCH(bool,webm);QFETCH(QString,aspect);QFETCH(QRect,expected);
        const auto input=webm?cropWebm:cropMp4;const auto original=hash(input);VideoClipJob job;
        QVERIFY2(ready(job,input),qPrintable(job.error()));job.setCropAspect(aspect);QVERIFY(job.cropCentered());
        job.setCropPosition(expected.x(),expected.y());QCOMPARE(job.cropRect(),expected);QVERIFY(!job.cropCentered());
        const auto sourceFrame=execute(ffmpeg,{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-i"),input,
            QStringLiteral("-frames:v"),QStringLiteral("1"),QStringLiteral("-an"),QStringLiteral("-pix_fmt"),QStringLiteral("gray"),QStringLiteral("-f"),QStringLiteral("rawvideo"),QStringLiteral("pipe:1")});
        QCOMPARE(sourceFrame.size(),320*240);
        for(const auto &format:{QStringLiteral("mp4"),QStringLiteral("gif")}) {
            job.setOutputFormat(format);QVERIFY(QTest::qWaitFor([&]{return !job.busy();},10000));QVERIFY2(job.ready(),qPrintable(job.error()));
            QCOMPARE(job.cropRect(),expected);QVERIFY(!job.cropCentered());
            const auto target=folder.filePath(QString::fromLatin1(QTest::currentDataTag())+u'.'+format);
            job.exportClip(QUrl::fromLocalFile(target),200,1200);QVERIFY(job.busy());
            job.setCropPosition(17,19);job.resetCropPosition();QCOMPARE(job.cropRect(),expected);
            QVERIFY(QTest::qWaitFor([&]{return !job.busy();},30000));QVERIFY2(job.error().isEmpty(),qPrintable(job.error()));
            QCOMPARE(job.outputUrl(),QUrl::fromLocalFile(target));QCOMPARE(hash(input),original);
            QStringList decode{QStringLiteral("-v"),QStringLiteral("error")};
            if(format==QStringLiteral("gif"))decode<<QStringLiteral("-ignore_loop")<<QStringLiteral("1");
            decode<<QStringLiteral("-i")<<target<<QStringLiteral("-frames:v")<<QStringLiteral("1")<<QStringLiteral("-an")<<QStringLiteral("-pix_fmt")<<QStringLiteral("gray")<<QStringLiteral("-f")<<QStringLiteral("rawvideo")<<QStringLiteral("pipe:1");
            const auto output=execute(ffmpeg,decode);QCOMPARE(output.size(),expected.width()*expected.height());
            for(const QPoint point:{QPoint(8,8),QPoint(expected.width()/2,expected.height()/2),QPoint(expected.width()-9,expected.height()-9)}) {
                const int actual=uchar(output.at(point.y()*expected.width()+point.x()));
                const int wanted=uchar(sourceFrame.at((point.y()+expected.y())*320+point.x()+expected.x()));
                QVERIFY2(qAbs(actual-wanted)<=5,qPrintable(QStringLiteral("Moved crop pixel %1,%2 was %3, expected source %4").arg(point.x()).arg(point.y()).arg(actual).arg(wanted)));
            }
            // Repeated geometry (including odd coordinate snapping) must keep a
            // completed output visible and must not create a change notification.
            QSignalSpy unchanged(&job,&VideoClipJob::changed);
            job.setCropPosition(expected.x()+1,expected.y()+1);
            QCOMPARE(job.cropRect(),expected);QCOMPARE(unchanged.size(),0);
            QCOMPARE(job.outputUrl(),QUrl::fromLocalFile(target));QCOMPARE(job.progress(),qreal(1));
        }
        job.setCropPosition(17,19);QVERIFY(job.outputUrl().isEmpty());QCOMPARE(job.progress(),qreal(0));
        job.resetCropPosition();QVERIFY(job.cropCentered());QVERIFY(job.outputUrl().isEmpty());QCOMPARE(job.progress(),qreal(0));
        QSignalSpy resetAgain(&job,&VideoClipJob::changed);job.resetCropPosition();QCOMPARE(resetAgain.size(),0);
    }
    void cropPositionBoundsAndLifecycle() {
        VideoClipJob job;
        QSignalSpy changes(&job,&VideoClipJob::changed);
        job.setCropPosition(std::numeric_limits<int>::max(),std::numeric_limits<int>::min());job.resetCropPosition();QCOMPARE(changes.size(),0);
        QVERIFY(ready(job,cropMp4));QVERIFY(job.cropCentered());
        changes.clear();job.setCropPosition(25,25);job.resetCropPosition();QCOMPARE(changes.size(),0); // Original is not movable.
        job.setCropAspect(QStringLiteral("portrait"));QCOMPARE(job.cropRect(),QRect(92,0,134,240));
        job.setCropPosition(std::numeric_limits<int>::max(),std::numeric_limits<int>::min());QCOMPARE(job.cropRect(),QRect(186,0,134,240));QVERIFY(!job.cropCentered());
        job.setCropPosition(std::numeric_limits<int>::min(),std::numeric_limits<int>::max());QCOMPARE(job.cropRect(),QRect(0,0,134,240));
        job.setCropPosition(17,19);QCOMPARE(job.cropRect(),QRect(16,0,134,240));
        changes.clear();job.setCropPosition(17,1);QCOMPARE(changes.size(),0);QCOMPARE(job.cropRect(),QRect(16,0,134,240));
        job.setCropAspect(QStringLiteral("invalid"));QCOMPARE(job.cropRect(),QRect(16,0,134,240));QCOMPARE(changes.size(),0);
        job.setCropPosition(93,0);QVERIFY(job.cropCentered());QCOMPARE(job.cropRect(),QRect(92,0,134,240));
        changes.clear();job.resetCropPosition();QCOMPARE(changes.size(),0);
        job.setCropPosition(0,0);job.setCropAspect(QStringLiteral("wide"));QVERIFY(job.cropCentered());QCOMPARE(job.cropRect(),QRect(0,30,320,180));
        job.setCropPosition(std::numeric_limits<int>::max(),std::numeric_limits<int>::max());QCOMPARE(job.cropRect(),QRect(0,60,320,180));
        job.setCropPosition(-1,-1);QCOMPARE(job.cropRect(),QRect(0,0,320,180)); // Public negatives clamp; they are not the private sentinel.
        job.setCropPosition(0,31);QVERIFY(job.cropCentered());QCOMPARE(job.cropRect(),QRect(0,30,320,180));
        job.exportClip(QUrl::fromLocalFile(folder.filePath(QStringLiteral("position-invalid-range.mp4"))),0,1);QVERIFY(!job.error().isEmpty());
        job.setCropPosition(0,0);QVERIFY(job.error().isEmpty());
        job.exportClip(QUrl::fromLocalFile(folder.filePath(QStringLiteral("position-invalid-range.mp4"))),0,1);const auto previousError=job.error();QVERIFY(!previousError.isEmpty());
        changes.clear();job.setCropPosition(1,1);QCOMPARE(job.error(),previousError);QCOMPARE(changes.size(),0);
        job.resetCropPosition();QVERIFY(job.error().isEmpty());QVERIFY(job.cropCentered());
        job.setCropPosition(0,0);job.inspect(QUrl::fromLocalFile(cropWebm));QVERIFY(job.cropCentered());QCOMPARE(job.cropAspect(),QStringLiteral("original"));
        job.setCropPosition(0,60);job.resetCropPosition();QVERIFY(QTest::qWaitFor([&]{return !job.busy();},10000));QVERIFY(job.ready());
        job.setCropAspect(QStringLiteral("wide"));QVERIFY(job.cropCentered());QCOMPARE(job.cropRect(),QRect(0,30,320,180));
    }
    void cropNonSquareSarKeepsOriginal() {
        const auto input=folder.filePath(QStringLiteral("non-square-sar.mp4"));
        execute(ffmpeg,{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-y"),QStringLiteral("-i"),cropMp4,
            QStringLiteral("-vf"),QStringLiteral("setsar=4/3"),QStringLiteral("-c:v"),QStringLiteral("libx264"),QStringLiteral("-threads"),QStringLiteral("2"),QStringLiteral("-c:a"),QStringLiteral("copy"),input});
        VideoClipJob job;QVERIFY2(ready(job,input),qPrintable(job.error()));QVERIFY(!job.cropAvailable());job.setCropAspect(QStringLiteral("square"));
        QSignalSpy unmoved(&job,&VideoClipJob::changed);job.setCropPosition(20,30);job.resetCropPosition();QCOMPARE(unmoved.size(),0);
        QCOMPARE(job.cropAspect(),QStringLiteral("original"));QCOMPARE(job.cropRect(),QRect(0,0,320,240));
        const auto target=folder.filePath(QStringLiteral("non-square-sar-copy.mp4"));job.exportClip(QUrl::fromLocalFile(target),200,1200);
        QVERIFY(QTest::qWaitFor([&]{return !job.busy();},30000));QVERIFY2(job.error().isEmpty(),qPrintable(job.error()));
        const auto sar=execute(ffprobe,{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-select_streams"),QStringLiteral("v:0"),QStringLiteral("-show_entries"),QStringLiteral("stream=sample_aspect_ratio"),QStringLiteral("-of"),QStringLiteral("csv=p=0"),target});
        QCOMPARE(sar.trimmed(),QByteArray("4:3"));
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
    void webmSparseHeldFramesAndOpus_data() {
        QTest::addColumn<double>("audioOffset");
        QTest::newRow("normal")<<0.;
        QTest::newRow("negative-opus")<<-.04;
    }
    void webmSparseHeldFramesAndOpus() {
        QFETCH(double,audioOffset);
        const QString label=QString::fromLatin1(QTest::currentDataTag());
        const auto webm=folder.filePath(label+QStringLiteral(" sparse ' 中文.webm"));
        QProcess encode;
        encode.start(ffmpeg,{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-y"),QStringLiteral("-i"),source,
            QStringLiteral("-vf"),QStringLiteral("select='eq(n,0)+eq(n,60)+eq(n,120)'"),QStringLiteral("-fps_mode"),QStringLiteral("vfr"),
            QStringLiteral("-c:v"),QStringLiteral("libvpx-vp9"),QStringLiteral("-deadline"),QStringLiteral("realtime"),QStringLiteral("-cpu-used"),QStringLiteral("8"),
            QStringLiteral("-g"),QStringLiteral("300"),QStringLiteral("-threads"),QStringLiteral("2"),QStringLiteral("-c:a"),QStringLiteral("libopus"),QStringLiteral("-af"),QStringLiteral("asetpts=PTS+%1/TB").arg(audioOffset),QStringLiteral("-avoid_negative_ts"),QStringLiteral("disabled"),webm});
        QVERIFY(encode.waitForFinished(30000));QVERIFY2(encode.exitCode()==0,encode.readAllStandardError().constData());
        const auto original=hash(webm);VideoClipJob job;QVERIFY2(ready(job,webm),qPrintable(job.error()));QVERIFY(job.hasAudio());
        const auto target=folder.filePath(label+QStringLiteral("-sparse-clip.mp4"));job.exportClip(QUrl::fromLocalFile(target),1350,3150);
        QVERIFY(QTest::qWaitFor([&]{return !job.busy();},30000));QVERIFY2(job.error().isEmpty(),qPrintable(job.error()));
        const auto frames=execute(ffmpeg,{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-xerror"),QStringLiteral("-i"),target,QStringLiteral("-an"),QStringLiteral("-pix_fmt"),QStringLiteral("yuv420p"),QStringLiteral("-f"),QStringLiteral("rawvideo"),QStringLiteral("pipe:1")});
        const int bytes=320*180*3/2,count=frames.size()/bytes;QCOMPARE(count,54);
        QVERIFY(qAbs(int(uchar(frames.at(1000)))-16)<=4); // hold the frame before the requested start
        QVERIFY(qAbs(int(uchar(frames.at((count-1)*bytes+1000)))-196)<=4);
        const auto pcm=execute(ffmpeg,{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-i"),target,QStringLiteral("-vn"),QStringLiteral("-ac"),QStringLiteral("1"),QStringLiteral("-ar"),QStringLiteral("48000"),QStringLiteral("-f"),QStringLiteral("f32le"),QStringLiteral("pipe:1")});
        const auto rms=[&](double from,double to){double sum=0;int n=0;for(int i=int(from*48000);i<int(to*48000)&&4*i+4<=pcm.size();++i){float v;std::memcpy(&v,pcm.constData()+4*i,4);sum+=double(v)*v;++n;}return n?std::sqrt(sum/n):0.;};
        int first=-1;for(int i=0;i*4+4<=pcm.size();++i){float v;std::memcpy(&v,pcm.constData()+i*4,4);if(std::abs(v)>.08){first=i;break;}}
        QVERIFY(first>=0);QVERIFY(qAbs(first/48000.-(.65+audioOffset))<.025);
        QVERIFY(rms(.68,.78)>.15);QVERIFY(rms(.1,.5)<.02);QVERIFY(rms(1.,1.4)<.02);
        job.setOutputFormat(QStringLiteral("gif"));QVERIFY(QTest::qWaitFor([&]{return !job.busy();},10000));QVERIFY(job.ready());
        const auto gif=folder.filePath(label+QStringLiteral("-sparse-clip.gif"));job.exportClip(QUrl::fromLocalFile(gif),1350,3150);
        QVERIFY(QTest::qWaitFor([&]{return !job.busy();},30000));QVERIFY2(job.error().isEmpty(),qPrintable(job.error()));
        QFile f(gif);QVERIFY(f.open(QIODevice::ReadOnly));const auto detail=gifDetails(f.readAll());QVERIFY(detail.valid&&detail.infinite);QCOMPARE(detail.frames,18);
        const auto gifFrames=execute(ffmpeg,{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-ignore_loop"),QStringLiteral("1"),QStringLiteral("-i"),gif,QStringLiteral("-pix_fmt"),QStringLiteral("gray"),QStringLiteral("-f"),QStringLiteral("rawvideo"),QStringLiteral("pipe:1")});
        QVERIFY(gifFrames.size()>=320*180*18);QVERIFY(uchar(gifFrames.at(1000))<10);QVERIFY(uchar(gifFrames.at(17*320*180+1000))>190);
        QCOMPARE(hash(webm),original);
    }
    void removeAudio_data() {QTest::addColumn<bool>("webm");QTest::newRow("mp4")<<false;QTest::newRow("webm")<<true;}
    void removeAudio() {
        QFETCH(bool,webm);
        QString input=source;
        if(webm) {
            input=folder.filePath(QStringLiteral("mute-source.webm"));
            execute(ffmpeg,{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-y"),QStringLiteral("-i"),source,
                QStringLiteral("-c:v"),QStringLiteral("libvpx-vp9"),QStringLiteral("-deadline"),QStringLiteral("realtime"),
                QStringLiteral("-cpu-used"),QStringLiteral("8"),QStringLiteral("-threads"),QStringLiteral("2"),
                QStringLiteral("-c:a"),QStringLiteral("libopus"),input});
        }
        const auto before=hash(input);QVERIFY(!before.isEmpty());VideoClipJob job;
        QVERIFY(ready(job,input));QVERIFY(job.hasAudio());QVERIFY(job.includeAudio());
        job.setIncludeAudio(false);QVERIFY(job.ready());QVERIFY(!job.includeAudio());
        const auto target=folder.filePath(webm?QStringLiteral("silent-webm.mp4"):QStringLiteral("silent-mp4.mp4"));
        job.exportClip(QUrl::fromLocalFile(target),1350,3150);
        QVERIFY(job.busy());job.setIncludeAudio(true);QVERIFY(!job.includeAudio());
        QVERIFY(QTest::qWaitFor([&]{return !job.busy();},30000));QVERIFY2(job.error().isEmpty(),qPrintable(job.error()));
        QCOMPARE(job.outputUrl(),QUrl::fromLocalFile(target));
        const auto tracks=execute(ffprobe,{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-show_streams"),QStringLiteral("-of"),QStringLiteral("json"),target});
        const auto streams=QJsonDocument::fromJson(tracks).object()[QStringLiteral("streams")].toArray();
        QCOMPARE(streams.size(),1);QCOMPARE(streams.first().toObject()[QStringLiteral("codec_type")].toString(),QStringLiteral("video"));
        job.setIncludeAudio(true);QVERIFY(job.ready());QVERIFY(job.outputUrl().isEmpty());
        const auto withAudio=target+QStringLiteral("-audio.mp4");job.exportClip(QUrl::fromLocalFile(withAudio),1350,3150);
        QVERIFY(QTest::qWaitFor([&]{return !job.busy();},30000));QVERIFY2(job.error().isEmpty(),qPrintable(job.error()));
        const auto audio=execute(ffprobe,{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-select_streams"),QStringLiteral("a"),QStringLiteral("-show_entries"),QStringLiteral("stream=codec_type"),QStringLiteral("-of"),QStringLiteral("csv=p=0"),withAudio});
        QCOMPARE(audio.trimmed(),QByteArray("audio"));QCOMPARE(hash(input),before);
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
    void actualGifFramesTimingAndFormatSwitch() {
        const auto original=hash(source);VideoClipJob job;job.setOutputFormat(QStringLiteral("gif"));QCOMPARE(job.outputFormat(),QStringLiteral("gif"));QVERIFY(!job.busy());
        QVERIFY2(ready(job,source),qPrintable(job.error()));QVERIFY(job.hasAudio());const auto duration=job.durationMs();
        const auto target=folder.filePath(QStringLiteral("animated ; 中文.gif"));job.exportClip(QUrl::fromLocalFile(target),1350,3150);
        QVERIFY(QTest::qWaitFor([&]{return !job.busy();},30000));QVERIFY2(job.error().isEmpty(),qPrintable(job.error()));QCOMPARE(hash(source),original);
        QFile file(target);QVERIFY(file.open(QIODevice::ReadOnly));const auto details=gifDetails(file.readAll());QVERIFY(details.valid);QVERIFY(details.infinite);QCOMPARE(details.width,320);QCOMPARE(details.height,180);QCOMPARE(details.frames,18);for(const auto delay:details.delays)QCOMPARE(delay,10);
        const auto metadata=QJsonDocument::fromJson(execute(ffprobe,{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-f"),QStringLiteral("gif"),QStringLiteral("-ignore_loop"),QStringLiteral("1"),QStringLiteral("-show_streams"),QStringLiteral("-show_format"),QStringLiteral("-of"),QStringLiteral("json"),target})).object();
        QCOMPARE(metadata.value(QStringLiteral("streams")).toArray().size(),1);QCOMPARE(metadata.value(QStringLiteral("streams")).toArray()[0].toObject().value(QStringLiteral("codec_type")).toString(),QStringLiteral("video"));QVERIFY(qAbs(metadata.value(QStringLiteral("format")).toObject().value(QStringLiteral("duration")).toString().toDouble()-1.8)<.01);
        const auto frames=execute(ffmpeg,{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-ignore_loop"),QStringLiteral("1"),QStringLiteral("-i"),target,QStringLiteral("-pix_fmt"),QStringLiteral("rgba"),QStringLiteral("-f"),QStringLiteral("rawvideo"),QStringLiteral("pipe:1")});
        const int frameBytes=320*180*4;QCOMPARE(frames.size(),18*frameBytes);QVERIFY(frames.left(frameBytes)!=frames.mid(frameBytes,frameBytes));
        // 30→10 fps picks source frames 42 and 93 for this non-keyframe range.
        QVERIFY(qAbs(int(uchar(frames[0]))-146)<=2);QVERIFY(qAbs(int(uchar(frames[17*frameBytes]))-91)<=2);
        job.setOutputFormat(QStringLiteral("mp4"));QVERIFY(job.outputUrl().isEmpty());QVERIFY(QTest::qWaitFor([&]{return !job.busy();},20000));QVERIFY2(job.ready(),qPrintable(job.error()));QCOMPARE(job.durationMs(),duration);QCOMPARE(job.sourceUrl(),QUrl::fromLocalFile(source));
        const auto mp4=folder.filePath(QStringLiteral("after-gif.mp4"));job.exportClip(QUrl::fromLocalFile(mp4),1350,3150);QVERIFY(QTest::qWaitFor([&]{return !job.busy();},30000));QVERIFY2(job.error().isEmpty(),qPrintable(job.error()));QVERIFY(QFile::exists(mp4));
    }
    void gifAccurateSeekNearTwoHourEnd() {
        const auto longInput=folder.filePath(QStringLiteral("two-hours.mp4"));
        QProcess remux;remux.start(ffmpeg,{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-y"),QStringLiteral("-stream_loop"),QStringLiteral("1199"),QStringLiteral("-i"),silent,QStringLiteral("-t"),QStringLiteral("7200"),QStringLiteral("-c"),QStringLiteral("copy"),QStringLiteral("-movflags"),QStringLiteral("+faststart"),longInput});QVERIFY(remux.waitForFinished(30000));QCOMPARE(remux.exitCode(),0);
        VideoClipJob job;job.setOutputFormat(QStringLiteral("gif"));QVERIFY2(ready(job,longInput),qPrintable(job.error()));QCOMPARE(job.durationMs(),qint64(7200000));
        const auto target=folder.filePath(QStringLiteral("near-end.gif"));job.exportClip(QUrl::fromLocalFile(target),7195350,7197150);
        bool seekBeforeInput=false;for(auto process:job.findChildren<QProcess *>()) {const auto args=process->arguments();const int seek=args.indexOf(QStringLiteral("-ss")),input=args.indexOf(QStringLiteral("-i"));if(seek>=0&&seek<input&&args.value(seek+1)==QStringLiteral("7195.350"))seekBeforeInput=true;}QVERIFY(seekBeforeInput);
        QVERIFY(QTest::qWaitFor([&]{return !job.busy();},30000));QVERIFY2(job.error().isEmpty(),qPrintable(job.error()));
        QFile file(target);QVERIFY(file.open(QIODevice::ReadOnly));const auto details=gifDetails(file.readAll());QVERIFY(details.valid);QCOMPARE(details.frames,18);for(const auto delay:details.delays)QCOMPARE(delay,10);
        const auto frames=execute(ffmpeg,{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-ignore_loop"),QStringLiteral("1"),QStringLiteral("-i"),target,QStringLiteral("-pix_fmt"),QStringLiteral("rgba"),QStringLiteral("-f"),QStringLiteral("rawvideo"),QStringLiteral("pipe:1")});
        const int frameBytes=320*180*4;QCOMPARE(frames.size(),18*frameBytes);QVERIFY(qAbs(int(uchar(frames[0]))-146)<=2);QVERIFY(qAbs(int(uchar(frames[17*frameBytes]))-91)<=2);
    }
    void gifDimensionsLimitsAndNoClobber() {
        const auto large=folder.filePath(QStringLiteral("large.mp4"));execute(ffmpeg,{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-y"),QStringLiteral("-i"),silent,QStringLiteral("-vf"),QStringLiteral("scale=1280:720"),QStringLiteral("-an"),QStringLiteral("-c:v"),QStringLiteral("libx264"),QStringLiteral("-threads"),QStringLiteral("2"),large});
        VideoClipJob job;job.setOutputFormat(QStringLiteral("gif"));QVERIFY2(ready(job,large),qPrintable(job.error()));const auto target=folder.filePath(QStringLiteral("scaled.gif"));job.exportClip(QUrl::fromLocalFile(target),0,1000);QVERIFY(QTest::qWaitFor([&]{return !job.busy();},30000));QVERIFY2(job.error().isEmpty(),qPrintable(job.error()));
        QFile file(target);QVERIFY(file.open(QIODevice::ReadOnly));const auto gif=gifDetails(file.readAll());QVERIFY(gif.valid);QCOMPARE(gif.width,640);QCOMPARE(gif.height,360);QCOMPARE(gif.frames,10);
        const auto preserved=hash(target);job.exportClip(QUrl::fromLocalFile(target),0,1000);QVERIFY(!job.busy());QVERIFY(!job.error().isEmpty());QCOMPARE(hash(target),preserved);
        job.exportClip(QUrl::fromLocalFile(folder.filePath(QStringLiteral("too-short.gif"))),0,199);QVERIFY(!job.busy());QVERIFY(!job.error().isEmpty());
        const auto shortest=folder.filePath(QStringLiteral("minimum-duration.gif"));
        job.exportClip(QUrl::fromLocalFile(shortest),0,200);QVERIFY(QTest::qWaitFor([&]{return !job.busy();},30000));QVERIFY2(job.error().isEmpty(),qPrintable(job.error()));
        QFile shortestFile(shortest);QVERIFY(shortestFile.open(QIODevice::ReadOnly));const auto shortestGif=gifDetails(shortestFile.readAll());
        QVERIFY(shortestGif.valid);QVERIFY(shortestGif.infinite);QCOMPARE(shortestGif.frames,2);QCOMPARE(shortestGif.delays.size(),2);
        int shortestCentiseconds=0;for(int delay:shortestGif.delays){QCOMPARE(delay,10);shortestCentiseconds+=delay;}QCOMPARE(shortestCentiseconds,20);
        const auto longInput=folder.filePath(QStringLiteral("long.mp4"));execute(ffmpeg,{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-y"),QStringLiteral("-stream_loop"),QStringLiteral("3"),QStringLiteral("-i"),silent,QStringLiteral("-t"),QStringLiteral("18"),QStringLiteral("-c"),QStringLiteral("copy"),longInput});
        QVERIFY2(ready(job,longInput),qPrintable(job.error()));job.exportClip(QUrl::fromLocalFile(folder.filePath(QStringLiteral("too-long.gif"))),0,16000);QVERIFY(!job.busy());QVERIFY(job.error().contains(QStringLiteral("15 seconds")));
        const auto longest=folder.filePath(QStringLiteral("maximum-duration.gif"));
        job.exportClip(QUrl::fromLocalFile(longest),0,15000);QVERIFY(QTest::qWaitFor([&]{return !job.busy();},30000));QVERIFY2(job.error().isEmpty(),qPrintable(job.error()));
        QFile longestFile(longest);QVERIFY(longestFile.open(QIODevice::ReadOnly));const auto longestGif=gifDetails(longestFile.readAll());
        QVERIFY(longestGif.valid);QVERIFY(longestGif.infinite);QCOMPARE(longestGif.frames,150);QCOMPARE(longestGif.delays.size(),150);
        int longestCentiseconds=0;for(int delay:longestGif.delays){QCOMPARE(delay,10);longestCentiseconds+=delay;}QCOMPARE(longestCentiseconds,1500);
        const auto race=folder.filePath(QStringLiteral("gif-race.gif"));job.exportClip(QUrl::fromLocalFile(race),0,1000);QFile racing(race);QVERIFY(racing.open(QIODevice::WriteOnly));racing.write("keep newer");racing.close();const auto raceHash=hash(race);
        QVERIFY(QTest::qWaitFor([&]{return !job.busy();},30000));QVERIFY(!job.error().isEmpty());QCOMPARE(hash(race),raceHash);
    }
    void gifPaletteCancellationAndSwitch() {
        VideoClipJob job;job.setOutputFormat(QStringLiteral("gif"));QVERIFY(ready(job,source));QSignalSpy done(&job,&VideoClipJob::exported);
        const auto target=folder.filePath(QStringLiteral("cancel-palette.gif"));job.exportClip(QUrl::fromLocalFile(target),0,6000);QVERIFY(job.busy());
        QVERIFY(!QDir(folder.path()).entryList({QStringLiteral(".nkshot-clip-palette-*")},QDir::Files|QDir::Hidden).isEmpty());
        const auto children=job.findChildren<QProcess *>();QVERIFY(!children.isEmpty());bool paletteCommand=false;for(auto process:children)for(const auto &arg:process->arguments())if(arg.contains(QStringLiteral("palettegen=")))paletteCommand=true;QVERIFY(paletteCommand);
        job.cancel();QVERIFY(!job.busy());QVERIFY(QTest::qWaitFor([&]{return QDir(folder.path()).entryList({QStringLiteral(".nkshot-clip-*")},QDir::Files|QDir::Hidden).isEmpty();},3000));QVERIFY(!QFile::exists(target));QCOMPARE(done.size(),0);
        job.exportClip(QUrl::fromLocalFile(target),0,6000);QVERIFY(job.busy());job.setOutputFormat(QStringLiteral("mp4"));QVERIFY(QTest::qWaitFor([&]{return !job.busy();},20000));QVERIFY(job.ready());QVERIFY(!QFile::exists(target));QCOMPARE(done.size(),0);QVERIFY(job.outputUrl().isEmpty());
    }
    void gifSecondPassLifecycle() {
        for(int operation=0;operation<3;++operation) {
            auto job=std::make_unique<VideoClipJob>();job->setOutputFormat(QStringLiteral("gif"));QVERIFY(ready(*job,source));
            const auto target=folder.filePath(QStringLiteral("second-pass-%1.gif").arg(operation));bool acted=false,confirmedPaletteUse=false;
            auto connection=connect(job.get(),&VideoClipJob::changed,this,[&]{
                if(acted||!job||!job->busy()||job->progress()<=.35)return;
                for(auto process:job->findChildren<QProcess *>())for(const auto &arg:process->arguments())if(arg.contains(QStringLiteral("paletteuse=")))confirmedPaletteUse=true;
                if(!confirmedPaletteUse)return;
                acted=true;
                if(operation==0)job->cancel();
                else if(operation==1)job->inspect(QUrl::fromLocalFile(silent));
                else QTimer::singleShot(0,this,[&]{job.reset();});
            });
            job->exportClip(QUrl::fromLocalFile(target),0,6000);
            QVERIFY(QTest::qWaitFor([&]{return acted&&(!job||!job->busy());},30000));QVERIFY(confirmedPaletteUse);QVERIFY(!QFile::exists(target));
            if(operation==1){QVERIFY(job->ready());QCOMPARE(job->sourceUrl(),QUrl::fromLocalFile(silent));}
            if(operation==2)QVERIFY(!job);else {QVERIFY(job->outputUrl().isEmpty());disconnect(connection);}
            QVERIFY(QTest::qWaitFor([&]{return QDir(folder.path()).entryList({QStringLiteral(".nkshot-clip-*")},QDir::Files|QDir::Hidden).isEmpty();},3000));
        }
    }
    void gifCapabilitiesIndependentOfMp4Encoders() {
        const auto oldPath=qgetenv("PATH");const bool hadPath=qEnvironmentVariableIsSet("PATH");
        const auto oldReal=qgetenv("NKSHOT_TEST_REAL_FFMPEG");const bool hadReal=qEnvironmentVariableIsSet("NKSHOT_TEST_REAL_FFMPEG");
        const auto restore=qScopeGuard([&]{if(hadPath)qputenv("PATH",oldPath);else qunsetenv("PATH");if(hadReal)qputenv("NKSHOT_TEST_REAL_FFMPEG",oldReal);else qunsetenv("NKSHOT_TEST_REAL_FFMPEG");});
        const auto bin=folder.filePath(QStringLiteral("gif-tools"));QVERIFY(QDir().mkpath(bin));QFile script(bin+QStringLiteral("/ffmpeg"));QVERIFY(script.open(QIODevice::WriteOnly));
        script.write("#!/bin/sh\ncase \"$*\" in *-encoders*) printf ' V..... gif encoder\\n V..... png encoder\\n'; exit 0;; *-filters*) printf ' ... palettegen V->V\\n ... paletteuse VV->V\\n'; exit 0;; esac\nexec \"$NKSHOT_TEST_REAL_FFMPEG\" \"$@\"\n");script.close();QVERIFY(script.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
        QByteArray path=bin.toUtf8();path.append(':');path.append(oldPath);qputenv("PATH",path);qputenv("NKSHOT_TEST_REAL_FFMPEG",ffmpeg.toUtf8());
        VideoClipJob job;job.setOutputFormat(QStringLiteral("gif"));QVERIFY2(ready(job,source),qPrintable(job.error()));
        const auto target=folder.filePath(QStringLiteral("gif-only-tools.gif"));job.exportClip(QUrl::fromLocalFile(target),1350,3150);QVERIFY(QTest::qWaitFor([&]{return !job.busy();},30000));QVERIFY2(job.error().isEmpty(),qPrintable(job.error()));QVERIFY(QFile::exists(target));
        job.setOutputFormat(QStringLiteral("mp4"));QVERIFY(QTest::qWaitFor([&]{return !job.busy();},20000));QVERIFY(!job.ready());QVERIFY(job.error().contains(QStringLiteral("H.264")));
        const auto encoderError=job.error();job.setCropAspect(QStringLiteral("square"));
        QCOMPARE(job.cropAspect(),QStringLiteral("original"));QCOMPARE(job.error(),encoderError);
        job.setCropPosition(20,30);job.resetCropPosition();QCOMPARE(job.error(),encoderError);
        job.setOutputFormat(QStringLiteral("gif"));QVERIFY(QTest::qWaitFor([&]{return !job.busy();},20000));QVERIFY2(job.ready(),qPrintable(job.error()));QVERIFY(job.error().isEmpty());
    }
    void gifByteBudget() {
        const auto noisy=folder.filePath(QStringLiteral("noise.mp4"));
        QProcess generate;generate.start(ffmpeg,{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-y"),QStringLiteral("-f"),QStringLiteral("lavfi"),QStringLiteral("-i"),QStringLiteral("nullsrc=s=640x640:r=10:d=15,geq=lum='random(1)*255':cb=128:cr=128"),QStringLiteral("-c:v"),QStringLiteral("libx264"),QStringLiteral("-pix_fmt"),QStringLiteral("yuv420p"),QStringLiteral("-qp"),QStringLiteral("0"),QStringLiteral("-preset"),QStringLiteral("ultrafast"),QStringLiteral("-threads"),QStringLiteral("2"),noisy});QVERIFY(generate.waitForFinished(30000));QCOMPARE(generate.exitCode(),0);
        VideoClipJob job;job.setOutputFormat(QStringLiteral("gif"));QVERIFY2(ready(job,noisy),qPrintable(job.error()));const auto target=folder.filePath(QStringLiteral("over-budget.gif"));job.exportClip(QUrl::fromLocalFile(target),0,15000);QVERIFY(QTest::qWaitFor([&]{return !job.busy();},60000));QVERIFY2(!job.error().isEmpty(),"High-entropy 640px GIF must exceed the 20 MiB budget");QVERIFY(!QFile::exists(target));QVERIFY(job.outputUrl().isEmpty());
        QVERIFY(QTest::qWaitFor([&]{return QDir(folder.path()).entryList({QStringLiteral(".nkshot-clip-*")},QDir::Files|QDir::Hidden).isEmpty();},3000));
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
