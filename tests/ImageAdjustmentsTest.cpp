// SPDX-License-Identifier: GPL-3.0-only
#include "Imaging/ImageAdjustments.h"
#include <QColorSpace>
#include <QtTest>
#include <limits>
using namespace NkShot::Imaging;
class ImageAdjustmentsTest : public QObject {
    Q_OBJECT
    static QImage pixel(QColor color) { QImage image(1,1,QImage::Format_RGBA8888); image.fill(color); return image; }
private Q_SLOTS:
    void identityIsIndependentAndExact() {
        auto source=pixel(QColor(82,153,29,117)); source.setColorSpace(QColorSpace::DisplayP3);
        source.setDevicePixelRatio(2); source.setText(QStringLiteral("test"),QStringLiteral("untouched"));
        const auto snapshot=source.copy(); auto result=adjustedCopy(source);
        QCOMPARE(result,source); QCOMPARE(result.format(),source.format()); QCOMPARE(result.colorSpace(),source.colorSpace());
        QCOMPARE(result.devicePixelRatio(),2.0); QCOMPARE(result.text(QStringLiteral("test")),QStringLiteral("untouched"));
        QVERIFY(result.constBits()!=source.constBits()); result.fill(Qt::black); QCOMPARE(source,snapshot);
    }
    void extremesAndGrey() {
        const auto red=pixel(QColor(255,0,0,100));
        QCOMPARE(adjustedCopy(red,{0,0,-100}).pixelColor(0,0),QColor(54,54,54,100));
        QCOMPARE(adjustedCopy(pixel(Qt::green),{0,0,-100}).pixelColor(0,0),QColor(182,182,182));
        QCOMPARE(adjustedCopy(pixel(Qt::blue),{0,0,-100}).pixelColor(0,0),QColor(18,18,18));
        QCOMPARE(adjustedCopy(red,{100,0,0}).pixelColor(0,0),QColor(255,255,255,100));
        QCOMPARE(adjustedCopy(red,{-100,0,0}).pixelColor(0,0),QColor(0,0,0,100));
        QCOMPARE(adjustedCopy(red,{0,-100,0}).pixelColor(0,0),QColor(128,128,128,100));
        QCOMPARE(adjustedCopy(pixel(QColor(64,128,192)),{0,100,0}).pixelColor(0,0),QColor(1,129,255));
        QCOMPARE(adjustedCopy(pixel(QColor(128,128,128)),{0,0,100}).pixelColor(0,0),QColor(128,128,128));
        QCOMPARE(adjustedCopy(pixel(QColor(128,64,32)),{10,0,0}).pixelColor(0,0),QColor(154,90,58));
    }
    void profileAlphaDimensionsAndSource() {
        QImage source(19,7,QImage::Format_RGBA8888); source.fill(QColor(80,140,200,128));
        source.setPixelColor(2,3,QColor(20,30,40,0)); source.setColorSpace(QColorSpace::DisplayP3); source.setDevicePixelRatio(1.5);
        const auto before=source.copy(); const auto result=adjustedCopy(source,{0,0,-100});
        QCOMPARE(result.size(),source.size()); QCOMPARE(result.devicePixelRatio(),1.5);
        QCOMPARE(result.colorSpace(),QColorSpace(QColorSpace::SRgb)); QCOMPARE(source,before); QCOMPARE(source.colorSpace(),before.colorSpace());
        const auto normalized=source.convertedToColorSpace(QColorSpace::SRgb).pixelColor(0,0);
        const int grey=qRound(.2126*normalized.red()+.7152*normalized.green()+.0722*normalized.blue());
        QCOMPARE(result.pixelColor(0,0),QColor(grey,grey,grey,128)); QCOMPARE(result.pixelColor(2,3),QColor(0,0,0,0));
        for(int y=0;y<source.height();++y)for(int x=0;x<source.width();++x) QCOMPARE(result.pixelColor(x,y).alpha(),source.pixelColor(x,y).alpha());
        auto premult=source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        const auto adjusted=adjustedCopy(premult,{20,30,40}); QVERIFY(!adjusted.isNull());
        QCOMPARE(adjusted.pixelColor(0,0).alpha(),128); QCOMPARE(adjusted.size(),premult.size());
    }
    void invalidAndLimits() {
        QVERIFY(adjustedCopy({}).isNull()); const auto source=pixel(Qt::red);
        for(int value : {-101,101,std::numeric_limits<int>::min(),std::numeric_limits<int>::max()}) {
            QVERIFY(adjustedCopy(source,{value,0,0}).isNull()); QVERIFY(adjustedCopy(source,{0,value,0}).isNull()); QVERIFY(adjustedCopy(source,{0,0,value}).isNull());
        }
        for(const auto size : {QSize(8193,1),QSize(1,32001),QSize(8192,4883)}) {
            QImage image(size,QImage::Format_Mono); QVERIFY(!image.isNull()); QVERIFY(adjustedCopy(image).isNull());
        }
        for(const auto size : {QSize(8192,1),QSize(1,32000)}) {
            QImage image(size,QImage::Format_RGB32); image.fill(Qt::white); QCOMPARE(adjustedCopy(image,{-100,0,0}).size(),size);
        }
    }
};
QTEST_GUILESS_MAIN(ImageAdjustmentsTest)
#include "ImageAdjustmentsTest.moc"
