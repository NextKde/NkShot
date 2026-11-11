// SPDX-License-Identifier: GPL-3.0-only
#include "Imaging/ImageExport.h"
#include <QColorSpace>
#include <QFile>
#include <QImageReader>
#include <QImageWriter>
#include <QTemporaryDir>
#include <QtTest>
using namespace NkShot::Imaging;
class ImageExportTest : public QObject {
    Q_OBJECT
    static QByteArray bytes(const QString &path) { QFile f(path); if (!f.open(QIODevice::ReadOnly)) return {}; return f.readAll(); }
    static void seed(const QString &path) { QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly)); QCOMPARE(f.write("existing target"), qint64(15)); }
private Q_SLOTS:
    void availability() {
        QCOMPARE(imageExportAvailable(ImageExportFormat::Jpeg), QImageWriter::supportedImageFormats().contains("jpeg"));
        QCOMPARE(imageExportAvailable(ImageExportFormat::WebpLossless), QImageWriter::supportedImageFormats().contains("webp"));
        QVERIFY(!imageExportAvailable(static_cast<ImageExportFormat>(99)));
        QTemporaryDir dir; QImage image(10,10,QImage::Format_ARGB32); image.fill(Qt::red);
        for (auto format : {ImageExportFormat::Jpeg, ImageExportFormat::WebpLossless}) {
            if (imageExportAvailable(format)) continue;
            const auto path=dir.filePath(format==ImageExportFormat::Jpeg ? QStringLiteral("missing.jpg"):QStringLiteral("missing.webp"));
            seed(path); QString error;
            QVERIFY(!saveImageCopy(image,path,format,90,&error)); QVERIFY(error.contains(QStringLiteral("encoder")));
            QCOMPARE(bytes(path),QByteArray("existing target"));
        }
    }
    void jpegWhiteAndQuality() {
        if (!imageExportAvailable(ImageExportFormat::Jpeg)) QSKIP("Qt JPEG encoder unavailable; missing-plugin behavior covered separately");
        QTemporaryDir dir; QImage image(96,64,QImage::Format_ARGB32); image.fill(Qt::transparent);
        for(int y=0;y<64;++y)for(int x=32;x<64;++x)image.setPixelColor(x,y,QColor(255,0,0,128));
        for(int y=0;y<64;++y)for(int x=64;x<96;++x)image.setPixelColor(x,y,Qt::blue);
        image.setDevicePixelRatio(2); image.setColorSpace(QColorSpace::SRgb); image.setText(QStringLiteral("fixture"),QStringLiteral("unchanged"));
        const auto before=image.copy(); QString error=QStringLiteral("stale"); const auto path=dir.filePath(QStringLiteral("copy.JPEG"));
        QVERIFY2(saveImageCopy(image,path,ImageExportFormat::Jpeg,100,&error),qPrintable(error)); QVERIFY(error.isEmpty());
        QVERIFY(bytes(path).startsWith(QByteArray::fromHex("ffd8ff")));
        QImage result(path); QCOMPARE(result.size(),image.size());
        QVERIFY(result.pixelColor(10,30).red()>250); QVERIFY(result.pixelColor(10,30).green()>250);
        auto pink=result.pixelColor(48,30); QVERIFY(pink.red()>248); QVERIFY(qAbs(pink.green()-127)<5); QVERIFY(qAbs(pink.blue()-127)<5);
        QVERIFY(result.pixelColor(80,30).blue()>248); QCOMPARE(image,before); QCOMPARE(image.devicePixelRatio(),2.0);
        QCOMPARE(image.text(QStringLiteral("fixture")),QStringLiteral("unchanged")); QCOMPARE(image.colorSpace(),QColorSpace(QColorSpace::SRgb));
        QImage noise(96,64,QImage::Format_RGB32);
        for(int y=0;y<64;++y)for(int x=0;x<96;++x)noise.setPixelColor(x,y,QColor((x*43+y*71)%256,(x*97+y*13)%256,(x*19+y*103)%256));
        const auto low=dir.filePath(QStringLiteral("low.jpg")),high=dir.filePath(QStringLiteral("high.jpg"));
        QVERIFY(saveImageCopy(noise,low,ImageExportFormat::Jpeg,1)); QVERIFY(saveImageCopy(noise,high,ImageExportFormat::Jpeg,100));
        QVERIFY(bytes(low).size()<bytes(high).size()); QVERIFY(!QImage(low).isNull());
    }
    void webpAlphaColorAndSize() {
        if (!imageExportAvailable(ImageExportFormat::WebpLossless)) QSKIP("Qt WebP encoder unavailable; missing-plugin behavior covered separately");
        QTemporaryDir dir; QImage image(80,60,QImage::Format_ARGB32); image.fill(Qt::transparent);
        for(int y=10;y<50;++y)for(int x=10;x<70;++x)image.setPixelColor(x,y,QColor(80,140,200,(x%2)?128:255));
        image.setColorSpace(QColorSpace::DisplayP3); image.setDevicePixelRatio(2); image.setText(QStringLiteral("fixture"),QStringLiteral("source"));
        const auto before=image.copy(); auto expected=image.convertedToColorSpace(QColorSpace::SRgb);
        const auto path=dir.filePath(QStringLiteral("copy.WEBP")); QString error;
        QVERIFY2(saveImageCopy(image,path,ImageExportFormat::WebpLossless,50,&error),qPrintable(error));
        auto data=bytes(path); QVERIFY(data.startsWith("RIFF")); QCOMPARE(data.mid(8,4),QByteArray("WEBP"));
        QVERIFY(data.contains("VP8L")); // Lossless WebP bitstream, not merely quality-100 lossy.
        QImage result(path); QCOMPARE(result.size(),image.size());
        for(int y=0;y<60;++y)for(int x=0;x<80;++x) {
            QCOMPARE(result.pixelColor(x,y).alpha(),expected.pixelColor(x,y).alpha());
            if(expected.pixelColor(x,y).alpha()) QCOMPARE(result.pixelColor(x,y),expected.pixelColor(x,y));
        }
        QCOMPARE(image,before); QCOMPARE(image.colorSpace(),QColorSpace(QColorSpace::DisplayP3));
        QCOMPARE(image.devicePixelRatio(),2.0); QCOMPARE(image.text(QStringLiteral("fixture")),QStringLiteral("source"));
    }
    void unwritableDestination() {
        if (!imageExportAvailable(ImageExportFormat::Jpeg)) QSKIP("Qt JPEG encoder unavailable");
        QTemporaryDir dir; const auto folder=dir.filePath(QStringLiteral("locked")); QVERIFY(QDir().mkdir(folder));
        const auto path=folder+QStringLiteral("/saved.jpg"); seed(path);
        QVERIFY(QFile::setPermissions(folder,QFileDevice::ReadOwner|QFileDevice::ExeOwner));
        // Privileged test runners may bypass directory permissions. Probe rather than
        // claiming an unwritable-path check passed in that environment.
        QFile probe(folder+QStringLiteral("/probe"));
        if(probe.open(QIODevice::WriteOnly)) {
            probe.close(); QFile::setPermissions(folder,QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner);
            QSKIP("Runner bypasses directory permissions");
        }
        QImage image(10,10,QImage::Format_RGB32); image.fill(Qt::blue); QString error;
        const bool saved=saveImageCopy(image,path,ImageExportFormat::Jpeg,90,&error);
        const auto content=bytes(path);
        QFile::setPermissions(folder,QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner);
        QVERIFY(!saved); QVERIFY(!error.isEmpty()); QCOMPARE(content,QByteArray("existing target"));
    }
    void rejectedInputsPreserveDestination() {
        QTemporaryDir dir; QImage image(20,20,QImage::Format_RGB32); image.fill(Qt::red);
        const auto path=dir.filePath(QStringLiteral("saved.jpg")); seed(path); QString error;
        for(int quality : {-1,0,101}) { QVERIFY(!saveImageCopy(image,path,ImageExportFormat::Jpeg,quality,&error)); QVERIFY(!error.isEmpty()); QCOMPARE(bytes(path),QByteArray("existing target")); }
        QVERIFY(!saveImageCopy({},path,ImageExportFormat::Jpeg,90,&error));
        QVERIFY(!saveImageCopy(image,path,static_cast<ImageExportFormat>(99),90,&error));
        QVERIFY(!saveImageCopy(image,path,ImageExportFormat::WebpLossless,100,&error));
        QVERIFY(!saveImageCopy(image,QStringLiteral("relative.jpg"),ImageExportFormat::Jpeg,90,&error));
        QVERIFY(!saveImageCopy(image,dir.filePath(QStringLiteral("bad.png")),ImageExportFormat::Jpeg,90,&error));
        QVERIFY(!saveImageCopy(image,dir.filePath(QStringLiteral("missing/copy.jpg")),ImageExportFormat::Jpeg,90,&error));
        QVERIFY(QDir().mkdir(dir.filePath(QStringLiteral("directory.jpg"))));
        QVERIFY(!saveImageCopy(image,dir.filePath(QStringLiteral("directory.jpg")),ImageExportFormat::Jpeg,90,&error));
        const auto link=dir.filePath(QStringLiteral("link.jpg")); QVERIFY(QFile::link(path,link));
        QVERIFY(!saveImageCopy(image,link,ImageExportFormat::Jpeg,90,&error)); QCOMPARE(bytes(path),QByteArray("existing target"));
        for(const auto size : {QSize(8193,1),QSize(1,32001),QSize(8192,4883)}) {
            QImage oversized(size,QImage::Format_Mono); QVERIFY(!oversized.isNull());
            QVERIFY(!saveImageCopy(oversized,path,ImageExportFormat::Jpeg,90,&error)); QVERIFY(error.contains(QStringLiteral("40 megapixels")));
        }
        QCOMPARE(bytes(path),QByteArray("existing target"));
    }
};
QTEST_GUILESS_MAIN(ImageExportTest)
#include "ImageExportTest.moc"
