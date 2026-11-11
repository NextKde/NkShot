// SPDX-License-Identifier: GPL-3.0-only
#include "LocalMediaLoader.h"
#include <QtTest>
#include <KLocalizedString>
#include <QTemporaryDir>
#include <QProcess>
#include <QImageWriter>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QScopeGuard>
#include <QCryptographicHash>
#include <sys/stat.h>
#include <signal.h>
using NkShot::LocalMediaLoader;
class LocalMediaLoaderTest : public QObject {
    Q_OBJECT
    QTemporaryDir directory;
    QString video;
    QString ffmpeg;
    bool run(const QString &program,const QStringList &args) {
        QProcess process; process.start(program,args);
        const bool result=process.waitForFinished(30000) && process.exitCode()==0;
        if(!result) qWarning()<<process.readAllStandardError();
        return result;
    }
    bool wait(LocalMediaLoader &loader,int timeout=20000) {
        return QTest::qWaitFor([&]{return !loader.busy();},timeout);
    }
    QByteArray hash(const QString &path) {
        QFile file(path); if(!file.open(QIODevice::ReadOnly)) return {};
        return QCryptographicHash::hash(file.readAll(),QCryptographicHash::Sha256);
    }
    QString image(const QString &name,const char *format) {
        const auto path=directory.filePath(name); QImage pixels(80,60,QImage::Format_ARGB32);
        pixels.fill(QColor(20,100,200)); return pixels.save(path,format) ? path : QString();
    }
    void fakeProbe(const QString &dir,const QByteArray &body) {
        QVERIFY(QDir().mkpath(dir)); QFile file(dir+QStringLiteral("/ffprobe")); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("#!/bin/sh\n"); file.write(body); file.close();
        QVERIFY(file.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
    }
private Q_SLOTS:
    void initTestCase() {
        KLocalizedString::setApplicationDomain("nkshot");
        QVERIFY(directory.isValid()); ffmpeg=QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
        QVERIFY(!ffmpeg.isEmpty()); QVERIFY(!QStandardPaths::findExecutable(QStringLiteral("ffprobe")).isEmpty());
        video=directory.filePath(QStringLiteral("recording ; ' 中文.mp4"));
        QVERIFY(run(ffmpeg,{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-f"),QStringLiteral("lavfi"),QStringLiteral("-i"),QStringLiteral("testsrc2=s=160x120:r=20:d=1"),
            QStringLiteral("-f"),QStringLiteral("lavfi"),QStringLiteral("-i"),QStringLiteral("sine=frequency=440:duration=1"),QStringLiteral("-c:v"),QStringLiteral("libx264"),QStringLiteral("-threads"),QStringLiteral("2"),QStringLiteral("-pix_fmt"),QStringLiteral("yuv420p"),QStringLiteral("-c:a"),QStringLiteral("aac"),video}));
    }
    void stillImages_data() {
        QTest::addColumn<QByteArray>("format");
        QTest::newRow("PNG")<<QByteArray("PNG"); QTest::newRow("JPEG")<<QByteArray("JPEG"); QTest::newRow("static-WebP")<<QByteArray("WEBP");
    }
    void stillImages() {
        QFETCH(QByteArray,format);
        const auto path=image(QStringLiteral("still-")+QString::fromLatin1(format)+QStringLiteral(" ; ' 中文"),format.constData()); QVERIFY(!path.isEmpty());
        const auto before=hash(path);
        LocalMediaLoader loader; QSignalSpy result(&loader,&LocalMediaLoader::imageReady);
        loader.load(QUrl::fromLocalFile(path)); QVERIFY(wait(loader)); QVERIFY2(loader.error().isEmpty(),qPrintable(loader.error()));
        QCOMPARE(result.size(),1); QCOMPARE(qvariant_cast<QImage>(result[0][0]).size(),QSize(80,60));
        QCOMPARE(hash(path),before);
    }
    void realMp4() {
        const auto before=hash(video);
        LocalMediaLoader loader; QSignalSpy result(&loader,&LocalMediaLoader::videoReady);
        loader.load(QUrl::fromLocalFile(video)); QVERIFY(wait(loader)); QVERIFY2(loader.error().isEmpty(),qPrintable(loader.error()));
        QCOMPARE(result.size(),1); QCOMPARE(result[0][1].toLongLong(),1000); QCOMPARE(result[0][2].toSize(),QSize(160,120));
        QVERIFY(result[0][3].toBool()); QCOMPARE(hash(video),before);
    }
    void longImageAndValidatedIdentity() {
        const auto path=directory.filePath(QStringLiteral("long-scroll.png"));
        { QImage pixels(1250,32000,QImage::Format_RGB32); pixels.fill(QColor(20,100,200));
          QVERIFY(pixels.save(path,"PNG")); }
        LocalMediaLoader loader; QSignalSpy result(&loader,&LocalMediaLoader::imageReady);
        QVERIFY(!loader.validatedSourceUnchanged());
        loader.load(QUrl::fromLocalFile(path)); QVERIFY(wait(loader));
        QVERIFY2(loader.error().isEmpty(),qPrintable(loader.error())); QCOMPARE(result.size(),1);
        QCOMPARE(qvariant_cast<QImage>(result[0][0]).size(),QSize(1250,32000));
        QVERIFY(loader.validatedSourceUnchanged());
        QFile modified(path); QVERIFY(modified.open(QIODevice::Append)); modified.write("changed"); modified.close();
        QVERIFY(!loader.validatedSourceUnchanged());
        loader.cancel(); QVERIFY(!loader.validatedSourceUnchanged());
    }
    void invalidSources() {
        const auto pipe=directory.filePath(QStringLiteral("special.png")); QCOMPARE(::mkfifo(QFile::encodeName(pipe).constData(),0600),0);
        for(const QUrl &url: {QUrl(QStringLiteral("https://example.com/image.png")),QUrl(QStringLiteral("qrc:/image.png")),QUrl(QStringLiteral("file://server/image.png")),
            QUrl::fromLocalFile(directory.path()),QUrl::fromLocalFile(pipe),QUrl::fromLocalFile(directory.filePath(QStringLiteral("missing.png")))}) {
            LocalMediaLoader loader; QSignalSpy result(&loader,&LocalMediaLoader::imageReady);
            loader.load(url); QVERIFY(wait(loader)); QVERIFY(!loader.error().isEmpty()); QCOMPARE(result.size(),0);
        }
        QFile fake(directory.filePath(QStringLiteral("fake.mp4"))); QVERIFY(fake.open(QIODevice::WriteOnly)); fake.write("not a recording");fake.close();
        LocalMediaLoader loader; QSignalSpy result(&loader,&LocalMediaLoader::videoReady);
        loader.load(QUrl::fromLocalFile(fake.fileName())); QVERIFY(wait(loader)); QVERIFY(!loader.error().isEmpty()); QVERIFY(result.isEmpty());
    }
    void animatedAndOversizedImages() {
        const auto animated=directory.filePath(QStringLiteral("animated.webp"));
        QVERIFY(run(ffmpeg,{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-f"),QStringLiteral("lavfi"),QStringLiteral("-i"),QStringLiteral("testsrc2=s=32x32:r=2:d=1"),QStringLiteral("-loop"),QStringLiteral("0"),QStringLiteral("-c:v"),QStringLiteral("libwebp_anim"),animated}));
        LocalMediaLoader loader; QSignalSpy result(&loader,&LocalMediaLoader::imageReady);
        loader.load(QUrl::fromLocalFile(animated)); QVERIFY(wait(loader)); QVERIFY(!loader.error().isEmpty()); QVERIFY(result.isEmpty());
        const auto large=directory.filePath(QStringLiteral("large.png")); QImage huge(6400,6400,QImage::Format_RGB32); huge.fill(Qt::white); QVERIFY(huge.save(large));
        loader.load(QUrl::fromLocalFile(large)); QVERIFY(wait(loader)); QVERIFY(!loader.error().isEmpty()); QVERIFY(result.isEmpty());
        QFile oversized(directory.filePath(QStringLiteral("oversized.mp4"))); QVERIFY(oversized.open(QIODevice::WriteOnly)); QVERIFY(oversized.resize(256LL*1024*1024+1)); oversized.close();
        loader.load(QUrl::fromLocalFile(oversized.fileName())); QVERIFY(wait(loader)); QVERIFY(!loader.error().isEmpty());
    }
    void corruptAndUnsupportedVideo() {
        const auto unsupported=directory.filePath(QStringLiteral("mpeg4.mp4"));
        QVERIFY(run(ffmpeg,{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-f"),QStringLiteral("lavfi"),QStringLiteral("-i"),QStringLiteral("testsrc2=s=32x32:r=10:d=1"),QStringLiteral("-c:v"),QStringLiteral("mpeg4"),unsupported}));
        LocalMediaLoader loader; QSignalSpy result(&loader,&LocalMediaLoader::videoReady);
        loader.load(QUrl::fromLocalFile(unsupported)); QVERIFY(wait(loader)); QVERIFY(!loader.error().isEmpty()); QVERIFY(result.isEmpty());
        const auto corrupt=directory.filePath(QStringLiteral("truncated.mp4")); QVERIFY(QFile::copy(video,corrupt));
        QFile truncated(corrupt); QVERIFY(truncated.open(QIODevice::ReadWrite)); QVERIFY(truncated.resize(truncated.size()/2)); truncated.close();
        loader.load(QUrl::fromLocalFile(corrupt)); QVERIFY(wait(loader)); QVERIFY(!loader.error().isEmpty()); QVERIFY(result.isEmpty());
    }
    void cancelSwitchAndDestruct() {
        const auto folder=directory.filePath(QStringLiteral("slow-tools")); fakeProbe(folder,"exec /bin/sleep 99\n");
        const auto old=qgetenv("PATH"); QByteArray path=folder.toUtf8(); path+=':';path+=old;
        qputenv("PATH",path); auto restore=qScopeGuard([&]{qputenv("PATH",old);});
        LocalMediaLoader loader; QSignalSpy videos(&loader,&LocalMediaLoader::videoReady); QSignalSpy images(&loader,&LocalMediaLoader::imageReady);
        loader.load(QUrl::fromLocalFile(video)); QVERIFY(loader.busy()); QTest::qWait(50); loader.cancel(); QVERIFY(!loader.busy());
        const auto still=image(QStringLiteral("switch.png"),"PNG"); QVERIFY(!still.isEmpty());
        loader.load(QUrl::fromLocalFile(still)); QVERIFY(wait(loader)); QCOMPARE(images.size(),1); QVERIFY(videos.isEmpty());
        auto dying=new LocalMediaLoader; dying->load(QUrl::fromLocalFile(video)); QTest::qWait(50); delete dying;
        QTest::qWait(100); QVERIFY(videos.isEmpty());
    }
    void reentrantCompletionAndDestruction() {
        const auto path=image(QStringLiteral("reentrant.png"),"PNG"); QVERIFY(!path.isEmpty());
        LocalMediaLoader loader; QSignalSpy result(&loader,&LocalMediaLoader::imageReady);
        bool cancelled=false;
        connect(&loader,&LocalMediaLoader::changed,&loader,[&] {if(!loader.busy() && !cancelled) { cancelled=true; loader.cancel(); }});
        // Explicit cancellation from the busy notification must start no worker.
        LocalMediaLoader cancelOnStart;
        connect(&cancelOnStart,&LocalMediaLoader::changed,&cancelOnStart,[&]{if(cancelOnStart.busy()) cancelOnStart.cancel();});
        cancelOnStart.load(QUrl::fromLocalFile(path)); QVERIFY(!cancelOnStart.busy());
        auto dying=new LocalMediaLoader; QPointer<LocalMediaLoader> guard(dying);
        connect(dying,&LocalMediaLoader::changed,dying,[dying]{delete dying;},Qt::SingleShotConnection);
        dying->load(QUrl::fromLocalFile(path)); QVERIFY(!guard);
        loader.load(QUrl::fromLocalFile(path)); QVERIFY(wait(loader)); QVERIFY(result.isEmpty());
    }
    void realProbeTimeout() {
        const auto folder=directory.filePath(QStringLiteral("timeout-tools")); fakeProbe(folder,"exec /bin/sleep 99\n");
        const auto old=qgetenv("PATH"); QByteArray path=folder.toUtf8();path+=':';path+=old;
        qputenv("PATH",path);auto restore=qScopeGuard([&]{qputenv("PATH",old);});
        LocalMediaLoader loader; QSignalSpy result(&loader,&LocalMediaLoader::videoReady);
        loader.load(QUrl::fromLocalFile(video)); QVERIFY(wait(loader,18000)); QVERIFY(!loader.error().isEmpty()); QVERIFY(result.isEmpty());
    }
};
QTEST_GUILESS_MAIN(LocalMediaLoaderTest)
#include "LocalMediaLoaderTest.moc"
