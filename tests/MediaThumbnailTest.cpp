// SPDX-License-Identifier: GPL-3.0-only
#include "Imaging/MediaThumbnail.h"
#include <QColorSpace>
#include <QElapsedTimer>
#include <QFile>
#include <QImageWriter>
#include <QTemporaryDir>
#include <QtTest>
#include <sys/stat.h>
using NkShot::Imaging::mediaThumbnail;
class MediaThumbnailTest : public QObject {
    Q_OBJECT
    static QByteArray bytes(const QString &path){QFile f(path);if(!f.open(QIODevice::ReadOnly))return {};return f.readAll();}
    static bool put(const QString &path,const QByteArray &data){QFile f(path);return f.open(QIODevice::WriteOnly)&&f.write(data)==data.size();}
private Q_SLOTS:
    void stillFormats_data(){QTest::addColumn<QByteArray>("format");QTest::newRow("png")<<QByteArray("png");QTest::newRow("jpeg")<<QByteArray("jpeg");QTest::newRow("webp")<<QByteArray("webp");}
    void stillFormats(){
        QFETCH(QByteArray,format);if(!QImageWriter::supportedImageFormats().contains(format))QSKIP("Image plugin unavailable");
        QTemporaryDir dir;const auto path=dir.filePath(QStringLiteral("照片 100%.image"));
        QImage image(400,200,QImage::Format_ARGB32);image.fill(QColor(30,140,210));image.setColorSpace(QColorSpace::DisplayP3);image.setDevicePixelRatio(2);
        QVERIFY(image.save(path,format.constData(),100));const auto original=bytes(path);const auto result=mediaThumbnail(QUrl::fromLocalFile(path));
        QVERIFY(!result.isNull());QCOMPARE(result.size(),QSize(160,80));QCOMPARE(result.devicePixelRatio(),1.0);QCOMPARE(result.colorSpace(),QColorSpace(QColorSpace::SRgb));
        const auto expected=image.convertedToColorSpace(QColorSpace::SRgb).pixelColor(100,100),actual=result.pixelColor(80,40);
        QVERIFY(qAbs(expected.red()-actual.red())<=4);QVERIFY(qAbs(expected.green()-actual.green())<=4);QVERIFY(qAbs(expected.blue()-actual.blue())<=4);
        QCOMPARE(bytes(path),original);
    }
    void transparentSmallAndTall(){
        QTemporaryDir dir;const auto path=dir.filePath(QStringLiteral("small.png"));
        QImage image(10,20,QImage::Format_RGBA8888);image.fill(QColor(120,40,10,128));image.setPixelColor(0,0,Qt::transparent);image.setDevicePixelRatio(2);
        QVERIFY(image.save(path));auto result=mediaThumbnail(QUrl::fromLocalFile(path));QCOMPARE(result.size(),image.size());QCOMPARE(result.pixelColor(0,0).alpha(),0);QCOMPARE(result.pixelColor(5,5),image.pixelColor(5,5));
        QImage tall(1,32000,QImage::Format_RGB32);tall.fill(Qt::blue);QVERIFY(tall.save(path));result=mediaThumbnail(QUrl::fromLocalFile(path),320);QVERIFY(!result.isNull());QCOMPARE(result.width(),1);QCOMPARE(result.height(),320);
    }
    void rejectedUrlsAndFiles(){
        QTemporaryDir dir;const auto path=dir.filePath(QStringLiteral("image.png"));QImage image(20,20,QImage::Format_RGB32);image.fill(Qt::red);QVERIFY(image.save(path));const auto before=bytes(path);auto url=QUrl::fromLocalFile(path);
        for(int edge:{-1,0,321,100000})QVERIFY(mediaThumbnail(url,edge).isNull());
        for(const auto &text:{QStringLiteral("https://example.org/image.png"),QStringLiteral("file://localhost/tmp/image.png"),QStringLiteral("file://user@server/tmp/image.png"),QStringLiteral("relative.png"),QStringLiteral("file:relative.png"),QStringLiteral("file:///tmp/zero%00.png"),QStringLiteral("file:///tmp/bad%GG.png")})QVERIFY(mediaThumbnail(QUrl(text,QUrl::StrictMode)).isNull());
        auto query=url;query.setQuery(QStringLiteral("x"));QVERIFY(mediaThumbnail(query).isNull());auto fragment=url;fragment.setFragment(QStringLiteral("x"));QVERIFY(mediaThumbnail(fragment).isNull());
        QVERIFY(mediaThumbnail(QUrl::fromLocalFile(dir.path())).isNull());QVERIFY(mediaThumbnail(QUrl::fromLocalFile(dir.filePath(QStringLiteral("missing.png")))).isNull());
        const auto link=dir.filePath(QStringLiteral("link.png"));QVERIFY(QFile::link(path,link));QVERIFY(mediaThumbnail(QUrl::fromLocalFile(link)).isNull());
        const auto fifo=dir.filePath(QStringLiteral("pipe.png"));QVERIFY(::mkfifo(QFile::encodeName(fifo).constData(),0600)==0);QElapsedTimer timer;timer.start();QVERIFY(mediaThumbnail(QUrl::fromLocalFile(fifo)).isNull());QVERIFY(timer.elapsed()<1000);
        const auto fake=dir.filePath(QStringLiteral("fake.png"));QVERIFY(put(fake,QByteArray("not an image")));QVERIFY(mediaThumbnail(QUrl::fromLocalFile(fake)).isNull());
        const auto video=dir.filePath(QStringLiteral("movie.mp4"));QVERIFY(put(video,QByteArray::fromHex("000000186674797069736f6d0000020069736f6d69736f32")));QVERIFY(mediaThumbnail(QUrl::fromLocalFile(video)).isNull());
        QCOMPARE(bytes(path),before);
    }
    void animatedAndBounds(){
        QTemporaryDir dir;const auto path=dir.filePath(QStringLiteral("fixture.png"));QImage image(2,2,QImage::Format_RGB32);image.fill(Qt::green);QVERIFY(image.save(path));
        auto data=bytes(path);data.insert(33,QByteArray::fromHex("000000086163544c000000020000000000000000"));QVERIFY(put(path,data));QVERIFY(mediaThumbnail(QUrl::fromLocalFile(path)).isNull());
        const auto webp=dir.filePath(QStringLiteral("animated.webp"));QVERIFY(put(webp,QByteArray::fromHex("524946461400000057454250414e494d06000000000000000000")));QVERIFY(mediaThumbnail(QUrl::fromLocalFile(webp)).isNull());
        QImage tooWide(32769,1,QImage::Format_Mono);tooWide.fill(0);QVERIFY(tooWide.save(path));QVERIFY(mediaThumbnail(QUrl::fromLocalFile(path)).isNull());
        QImage tooMany(8000,5001,QImage::Format_Mono);tooMany.fill(0);QVERIFY(tooMany.save(path));QVERIFY(mediaThumbnail(QUrl::fromLocalFile(path)).isNull());
        QFile large(path);QVERIFY(large.open(QIODevice::WriteOnly));QVERIFY(large.resize(64LL*1024*1024+1));large.close();QVERIFY(mediaThumbnail(QUrl::fromLocalFile(path)).isNull());
    }
};
QTEST_GUILESS_MAIN(MediaThumbnailTest)
#include "MediaThumbnailTest.moc"
