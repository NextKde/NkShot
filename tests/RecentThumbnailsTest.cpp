// SPDX-License-Identifier: GPL-3.0-only
#include "Gui/RecentThumbnails.h"
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QImage>
#include <QColorSpace>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>

#include <QtTest>
class RecentThumbnailsTest : public QObject {
    Q_OBJECT
    QUrl fixture(const QString &path,const QColor &color,QSize size=QSize(320,180)) {
        QImage image(size,QImage::Format_ARGB32_Premultiplied);image.fill(color);
        image.setColorSpace(QColorSpace::SRgb);image.setDevicePixelRatio(2);
        return image.save(path)?QUrl::fromLocalFile(path):QUrl();
    }
    QImage lookup(QQmlEngine &engine,const QUrl &url) {
        auto provider=dynamic_cast<QQuickImageProvider *>(engine.imageProvider(url.host()));
        return provider?provider->requestImage(url.path().mid(1),nullptr,QSize()):QImage();
    }
private Q_SLOTS:
    void explicitSessionAndVolatileImages() {
        QTemporaryDir folder;QVERIFY(folder.isValid());
        const auto url=fixture(folder.filePath(QStringLiteral("card.png")),QColor(QStringLiteral("#2876ba")));
        QVERIFY(!url.isEmpty());const auto beforeFiles=QDir(folder.path()).entryList(QDir::Files);
        QQmlEngine engine;RecentThumbnails cache(&engine);
        QVERIFY(!cache.active());QVERIFY(cache.urlFor(url).isEmpty());
        cache.begin({url});QVERIFY(cache.active());
        QVERIFY(QTest::qWaitFor([&]{return !cache.urlFor(url).isEmpty();},5000));
        const auto imageUrl=cache.urlFor(url);const auto thumbnail=lookup(engine,imageUrl);
        QVERIFY(!thumbnail.isNull());QCOMPARE(thumbnail.size(),QSize(160,90));QCOMPARE(thumbnail.devicePixelRatio(),1.);
        QCOMPARE(thumbnail.colorSpace(),QColorSpace(QColorSpace::SRgb));QCOMPARE(thumbnail.pixelColor(80,45),QColor(QStringLiteral("#2876ba")));
        const auto revision=cache.revision();
        QVERIFY(!fixture(url.toLocalFile(),Qt::green).isEmpty());QTest::qWait(80);
        QCOMPARE(cache.revision(),revision);QCOMPARE(cache.urlFor(url),imageUrl);QCOMPARE(lookup(engine,imageUrl),thumbnail);
        cache.clear();QVERIFY(!cache.active());QVERIFY(cache.urlFor(url).isEmpty());QVERIFY(lookup(engine,imageUrl).isNull());QVERIFY(cache.revision()>revision);
        cache.begin({url});QVERIFY(QTest::qWaitFor([&]{return !cache.urlFor(url).isEmpty();},5000));
        QCOMPARE(lookup(engine,cache.urlFor(url)).pixelColor(80,45),QColor(Qt::green));QVERIFY(cache.urlFor(url)!=imageUrl);
        QCOMPARE(QDir(folder.path()).entryList(QDir::Files),beforeFiles);
        cache.clear();QCOMPARE(QDir(folder.path()).entryList(QDir::Files),beforeFiles);
    }
    void replacementDropsRetiredResults() {
        QTemporaryDir folder;QVERIFY(folder.isValid());
        const auto a=fixture(folder.filePath(QStringLiteral("a.png")),Qt::red,QSize(640,400));
        const auto b=fixture(folder.filePath(QStringLiteral("b.png")),Qt::blue);
        QQmlEngine engine;RecentThumbnails cache(&engine);
        cache.begin({a});cache.begin({b});
        QVERIFY(QTest::qWaitFor([&]{return !cache.urlFor(b).isEmpty();},5000));
        QVERIFY(cache.urlFor(a).isEmpty());QCOMPARE(lookup(engine,cache.urlFor(b)).pixelColor(80,45),QColor(Qt::blue));
        cache.begin({a});cache.clear();
        // A second session on the same serial pool is a completion barrier for
        // the retired request, without assuming the global Qt pool is used.
        RecentThumbnails barrier(&engine);barrier.begin({b});
        QVERIFY(QTest::qWaitFor([&]{return !barrier.urlFor(b).isEmpty();},5000));
        QVERIFY(!cache.active());QVERIFY(cache.urlFor(a).isEmpty());QVERIFY(cache.urlFor(b).isEmpty());
    }
    void duplicatesLimitsAndPlaceholders() {
        QTemporaryDir folder;QVERIFY(folder.isValid());QQmlEngine engine;RecentThumbnails cache(&engine);
        QList<QUrl> images;for(int i=0;i<21;++i){auto url=fixture(folder.filePath(QStringLiteral("image-%1.png").arg(i)),QColor::fromHsv(i*10,100,200),QSize(32,24));QVERIFY(!url.isEmpty());images.append(url);}
        const auto video=QUrl::fromLocalFile(folder.filePath(QStringLiteral("video.mp4")));
        {QFile file(video.toLocalFile());QVERIFY(file.open(QIODevice::WriteOnly));QCOMPARE(file.write("video-placeholder"),qint64(17));}
        QList<QUrl> input{video,images.first(),images.first()};for(int i=1;i<images.size();++i)input.append(images.at(i));
        cache.begin(input);
        QVERIFY(QTest::qWaitFor([&]{return !cache.urlFor(images.at(19)).isEmpty();},10000));
        for(int i=0;i<20;++i)QVERIFY(!cache.urlFor(images.at(i)).isEmpty());
        QVERIFY(cache.urlFor(images.at(20)).isEmpty());QVERIFY(cache.urlFor(video).isEmpty());
        const auto missing=QUrl::fromLocalFile(folder.filePath(QStringLiteral("missing.png")));
        cache.begin({video,missing,images.first()});
        QVERIFY(QTest::qWaitFor([&]{return !cache.urlFor(images.first()).isEmpty();},5000));
        QVERIFY(cache.urlFor(video).isEmpty());QVERIFY(cache.urlFor(missing).isEmpty());
        QVERIFY(!QFile::exists(missing.toLocalFile()));
    }
    void synchronousObserversCanDestroySession() {
        QTemporaryDir folder;QVERIFY(folder.isValid());
        const auto url=fixture(folder.filePath(QStringLiteral("observer.png")),Qt::cyan);
        QQmlEngine engine;
        QPointer<RecentThumbnails> beginning=new RecentThumbnails(&engine);
        QObject::connect(beginning,&RecentThumbnails::changed,&engine,[&]{delete beginning.data();});
        beginning->begin({url});QVERIFY(beginning.isNull());
        QPointer<RecentThumbnails> finishing=new RecentThumbnails(&engine);
        QObject::connect(finishing,&RecentThumbnails::changed,&engine,[&]{
            if(finishing&&!finishing->urlFor(url).isEmpty())delete finishing.data();
        });
        finishing->begin({url});
        QVERIFY(QTest::qWaitFor([&]{return finishing.isNull();},5000));
    }
    void destructionDuringDecode() {
        QTemporaryDir folder;QVERIFY(folder.isValid());const auto url=fixture(folder.filePath(QStringLiteral("input.png")),Qt::white,QSize(640,480));
        auto engine=new QQmlEngine;auto cache=new RecentThumbnails(engine);
        cache->begin({url});delete engine;
        QCoreApplication::processEvents();
        cache->clear();cache->begin({url});QVERIFY(!cache->active());QVERIFY(cache->urlFor(url).isEmpty());delete cache;
        QQmlEngine other;
        auto pending=new RecentThumbnails(&other);pending->begin({url});delete pending;
        QCoreApplication::processEvents();
        RecentThumbnails replacement(&other);replacement.begin({url});QVERIFY(QTest::qWaitFor([&]{return !replacement.urlFor(url).isEmpty();},5000));
    }
};
QTEST_MAIN(RecentThumbnailsTest)
#include "RecentThumbnailsTest.moc"
