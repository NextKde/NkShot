// SPDX-License-Identifier: GPL-3.0-only
#include "Imaging/VerticalStitcher.h"
#include <QtTest>
#include <QColorSpace>
using namespace NkShot::Imaging;
class VerticalStitcherTest : public QObject {
    Q_OBJECT
    static QImage pattern(int width, int height, int seed = 0) {
        QImage image(width, height, QImage::Format_RGBA8888);
        for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x)
            image.setPixelColor(x, y, QColor((x*17+y*13+seed)%251, (y*y+x*23+seed)%253, (x*11+y*7+seed)%255));
        return image;
    }
private Q_SLOTS:
    void exactThreeFramesAndOwnership() {
        auto original = pattern(240, 800);
        auto first = original.copy(0,0,240,300);
        const auto result = stitchVertical({first, original.copy(0,220,240,300), original.copy(0,440,240,300)});
        QCOMPARE(result.error, StitchError::None);
        QCOMPARE(result.overlaps, QVector<int>({80,80}));
        QCOMPARE(result.image, original.copy(0,0,240,740));
        first.fill(Qt::red);
        QCOMPARE(result.image, original.copy(0,0,240,740));
    }
    void rejectsNoProgress() {
        auto frame = pattern(120, 200);
        QCOMPARE(stitchVertical({frame, frame}).error, StitchError::NoProgress);
    }
    void rejectsUnrelated() {
        QCOMPARE(stitchVertical({pattern(120,200),pattern(120,200,7)}).error, StitchError::NoOverlap);
    }
    void rejectsAmbiguousWhitespace() {
        QImage first(120,100,QImage::Format_RGBA8888); first.fill(Qt::white);
        QImage second = first;
        for (int y=60;y<100;++y) for(int x=0;x<120;++x) second.setPixelColor(x,y,Qt::black);
        QCOMPARE(stitchVertical({first,second}).error, StitchError::AmbiguousOverlap);
    }
    void rejectsIncompatibleAndLimits() {
        auto first = pattern(120,200); auto other = first;
        other.setDevicePixelRatio(2);
        QCOMPARE(stitchVertical({first,other}).error, StitchError::IncompatibleFrames);
        QCOMPARE(stitchVertical({first,pattern(121,200)}).error, StitchError::IncompatibleFrames);
        QCOMPARE(stitchVertical({first,first},24,100).error, StitchError::TooLarge);
        QCOMPARE(stitchVertical({first}).error, StitchError::InvalidInput);
        QCOMPARE(stitchVertical({QImage(),first}).error, StitchError::InvalidInput);
        auto original=pattern(120,300);
        QCOMPARE(stitchVertical({original.copy(0,0,120,200),original.copy(0,100,120,200)},24,30000).error, StitchError::TooLarge);
    }
    void preservesAlphaAndRejectsChangedOverlap() {
        auto original=pattern(120,300);
        for(int y=0;y<300;++y) for(int x=0;x<120;++x) {
            auto color=original.pixelColor(x,y); color.setAlpha(80+(x%175)); original.setPixelColor(x,y,color);
        }
        auto first=original.copy(0,0,120,200); auto second=original.copy(0,100,120,200);
        auto result=stitchVertical({first,second});
        QCOMPARE(result.error,StitchError::None);
        QCOMPARE(result.image,original);
        second.setPixelColor(20,50,Qt::red);
        QCOMPARE(stitchVertical({first,second}).error,StitchError::NoOverlap);
    }
    void rejectsDifferentColorSpaces() {
        auto first=pattern(120,200); auto second=first;
        first.setColorSpace(QColorSpace::SRgb); second.setColorSpace(QColorSpace::DisplayP3);
        QCOMPARE(stitchVertical({first,second}).error,StitchError::IncompatibleFrames);
    }
    void fractionalDprPreserved() {
        auto original=pattern(120,300); original.setDevicePixelRatio(1.5);
        const auto result=stitchVertical({original.copy(0,0,120,200),original.copy(0,100,120,200)});
        QCOMPARE(result.error,StitchError::None);
        QCOMPARE(result.image.devicePixelRatio(),1.5);
        QCOMPARE(result.image,original);
    }
};
QTEST_GUILESS_MAIN(VerticalStitcherTest)
#include "VerticalStitcherTest.moc"
