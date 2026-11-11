// SPDX-License-Identifier: GPL-3.0-only
#include "Imaging/ScrollFrameAligner.h"
#include <QtTest>
#include <QColorSpace>
#include <QPainter>
#include <QFont>
#include <cstring>
using namespace NkShot::Imaging;
class ScrollFrameAlignerTest : public QObject {
    Q_OBJECT
    static QImage pattern(int width,int height,int seed=0) {
        QImage image(width,height,QImage::Format_RGBA8888);
        for(int y=0;y<height;++y)for(int x=0;x<width;++x){quint32 n=quint32(x+1)*2654435761u^quint32(y+seed+1)*2246822519u;n^=n>>13;n*=3266489917u;image.setPixelColor(x,y,QColor(30+int(n%190),30+int((n>>8)%190),30+int((n>>16)%190)));}return image;
    }
private Q_SLOTS:
    void variablePositiveScroll_data(){QTest::addColumn<int>("distance");for(int n:{1,17,73,150,216})QTest::newRow(qPrintable(QString::number(n)))<<n;}
    void variablePositiveScroll(){QFETCH(int,distance);const auto page=pattern(160,600);const auto previous=page.copy(0,0,160,240),next=page.copy(0,distance,160,240);const auto result=alignScrollFrames(previous,next);QCOMPARE(result.error,StitchError::None);QCOMPARE(result.overlap,240-distance);QVERIFY(result.confidence>=.94);QVERIFY(result.runnerUpGap>=.08);}
    void smallNoiseAndLocalIsland(){const auto page=pattern(192,500);const auto previous=page.copy(0,0,192,240);auto next=page.copy(0,60,192,240);for(int y=0;y<next.height();++y)for(int x=0;x<next.width();++x){auto c=next.pixelColor(x,y);const int n=((x*7+y*3)%5)-2;c.setRgb(c.red()+n,c.green()-n,c.blue()+n);next.setPixelColor(x,y,c);}for(int y=55;y<67;++y)for(int x=65;x<82;++x)next.setPixelColor(x,y,Qt::red);const auto result=alignScrollFrames(previous,next);QCOMPARE(result.error,StitchError::None);QCOMPARE(result.overlap,180);QCOMPARE(previous,page.copy(0,0,192,240));}
    void blankRepeatedReverseAndNoop(){QImage blank(160,240,QImage::Format_RGBA8888);blank.fill(Qt::white);QVERIFY(alignScrollFrames(blank,blank).error!=StitchError::None);auto repeated=pattern(160,16);QImage a(160,240,QImage::Format_RGBA8888),b=a;for(int y=0;y<240;++y){memcpy(a.scanLine(y),repeated.constScanLine(y%16),640);memcpy(b.scanLine(y),repeated.constScanLine((y+5)%16),640);}QCOMPARE(alignScrollFrames(a,b).error,StitchError::AmbiguousOverlap);const auto page=pattern(160,500);a=page.copy(0,0,160,240);b=page.copy(0,70,160,240);QCOMPARE(alignScrollFrames(a,a).error,StitchError::NoProgress);QVERIFY(alignScrollFrames(b,a).error!=StitchError::None);}
    void largeDynamicAndUnrelated(){const auto page=pattern(192,500);auto a=page.copy(0,0,192,240),b=page.copy(0,60,192,240);for(int y=20;y<170;++y)for(int x=20;x<160;++x)b.setPixelColor(x,y,Qt::green);QVERIFY(alignScrollFrames(a,b).error!=StitchError::None);QVERIFY(alignScrollFrames(a,pattern(192,240,938)).error!=StitchError::None);}
    void sparseTextureRejected(){QImage page(192,500,QImage::Format_RGBA8888);page.fill(Qt::white);const auto island=pattern(20,20);for(int y=0;y<20;++y)for(int x=0;x<20;++x)page.setPixelColor(x+70,y+100,island.pixelColor(x,y));QVERIFY(alignScrollFrames(page.copy(0,0,192,240),page.copy(0,50,192,240)).error!=StitchError::None);}
    void existingSmallGradient() {
        QImage page(40,240,QImage::Format_RGBA8888);for(int y=0;y<240;++y)for(int x=0;x<40;++x)page.setPixelColor(x,y,QColor((x*7+y)%256,(y*13+x)%256,(x+y*3)%256));
        const auto result=alignScrollFrames(page.copy(0,0,40,100),page.copy(0,40,40,100));QCOMPARE(result.error,StitchError::None);QCOMPARE(result.overlap,60);
    }
    void whiteTextFixedHeaderAndRepeatedRows() {
        QImage page(240,600,QImage::Format_RGBA8888);page.fill(Qt::white);
        {QPainter painter(&page);painter.setPen(Qt::NoPen);painter.setBrush(Qt::black);
            for(int line=0;line<40;++line)for(int glyph=0;glyph<26;++glyph) {
                const int bits=(line*37+glyph*19+line*glyph*13)%31;
                for(int bit=0;bit<5;++bit)if(bits&(1<<bit))painter.drawRect(5+glyph*9, line*15+bit*2, 4, 1);
            }
        }
        auto a=page.copy(0,0,240,240),b=page.copy(0,63,240,240);
        const auto ordinary=alignScrollFrames(a,b);QVERIFY2(ordinary.error==StitchError::None,qPrintable(QStringLiteral("glyph registration error=%1 score=%2 gap=%3").arg(int(ordinary.error)).arg(ordinary.confidence).arg(ordinary.runnerUpGap)));QCOMPARE(ordinary.overlap,177);
        const auto header=pattern(240,8);{QPainter p(&a);p.drawImage(0,0,header);}{QPainter p(&b);p.drawImage(0,0,header);}
        QVERIFY(alignScrollFrames(a,b).error!=StitchError::None);
        QImage rows(240,240,QImage::Format_RGBA8888);for(int y=0;y<240;++y)memcpy(rows.scanLine(y),page.constScanLine(y%15),960);
        QImage shifted=rows;for(int y=0;y<240;++y)memcpy(shifted.scanLine(y),rows.constScanLine((y+5)%240),960);
        QVERIFY(alignScrollFrames(rows,shifted).error!=StitchError::None);
    }
    void realTextWithMargins_data(){QTest::addColumn<int>("widthPercent");QTest::addColumn<bool>("centered");QTest::newRow("left20")<<20<<false;QTest::newRow("left35")<<35<<false;QTest::newRow("center50")<<50<<true;}
    void realTextWithMargins(){
        QFETCH(int,widthPercent);QFETCH(bool,centered);
        QImage page(480,900,QImage::Format_RGBA8888);page.fill(Qt::white);const int width=480*widthPercent/100,x=centered?(480-width)/2:8;
        {QPainter p(&page);QFont font(QStringLiteral("DejaVu Sans"));font.setPixelSize(12);p.setFont(font);p.setPen(Qt::black);
            for(int row=0;row<50;++row)p.drawText(QRect(x,row*18,width,18),Qt::AlignLeft|Qt::AlignVCenter,QStringLiteral("%1 %2").arg(row,2,10,QLatin1Char('0')).arg(quint32(row+17)*2654435761u,0,16));}
        auto a=page.copy(0,0,480,360),b=page.copy(0,73,480,360);const auto aligned=alignScrollFrames(a,b);QCOMPARE(aligned.error,StitchError::None);QCOMPARE(aligned.overlap,287);
        {QPainter p(&a);p.fillRect(QRect(0,0,480,18),Qt::white);p.drawImage(QRect(x,0,width,18),page,QRect(x,0,width,18));}
        {QPainter p(&b);p.fillRect(QRect(0,0,480,18),Qt::white);p.drawImage(QRect(x,0,width,18),page,QRect(x,0,width,18));}
        QVERIFY(alignScrollFrames(a,b).error!=StitchError::None);
        QImage repeat(480,360,QImage::Format_RGBA8888);for(int y=0;y<360;++y)memcpy(repeat.scanLine(y),page.constScanLine(y%18),1920);
        QImage shift=repeat;for(int y=0;y<360;++y)memcpy(shift.scanLine(y),repeat.constScanLine((y+5)%360),1920);
        QVERIFY(alignScrollFrames(repeat,shift).error!=StitchError::None);
    }
    void strictGeometryAndBounds(){auto a=pattern(80,100),b=a;QCOMPARE(alignScrollFrames({},a).error,StitchError::InvalidInput);QCOMPARE(alignScrollFrames(a,b,1).error,StitchError::InvalidInput);QCOMPARE(alignScrollFrames(a,pattern(81,100)).error,StitchError::IncompatibleFrames);b.setDevicePixelRatio(2);QCOMPARE(alignScrollFrames(a,b).error,StitchError::IncompatibleFrames);b=a;b.setColorSpace(QColorSpace::SRgb);QCOMPARE(alignScrollFrames(a,b).error,StitchError::IncompatibleFrames);QImage tall(8,8193,QImage::Format_RGBA8888);QCOMPARE(alignScrollFrames(tall,tall).error,StitchError::TooLarge);}
};
QTEST_MAIN(ScrollFrameAlignerTest)
#include "ScrollFrameAlignerTest.moc"
