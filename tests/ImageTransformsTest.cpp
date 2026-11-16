// SPDX-License-Identifier: GPL-3.0-only
#include "Imaging/ImageTransforms.h"
#include <QtTest>
#include <QColorSpace>
using namespace NkShot::Imaging;
class ImageTransformsTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void orientationPixelsAndMetadata() {
        QImage source(3,2,QImage::Format_RGBA8888);
        for(int y=0;y<2;++y)for(int x=0;x<3;++x)source.setPixelColor(x,y,QColor(20+x*60,30+y*90,70,80+x*20+y*10));
        source.setDevicePixelRatio(2);source.setColorSpace(QColorSpace::DisplayP3);
        const auto original=source.copy();
        for(int turns=0;turns<4;++turns)for(bool mirror:{false,true}) {
            const int w=turns%2?2:3,h=turns%2?3:2;
            const auto result=orientedCopy(source,turns,mirror,w);
            QCOMPARE(result.size(),QSize(w,h));QCOMPARE(result.devicePixelRatio(),2.);
            QCOMPARE(result.colorSpace(),source.colorSpace());
            for(int y=0;y<2;++y)for(int x=0;x<3;++x) {
                int ox=x,oy=y;
                if(turns==1){ox=1-y;oy=x;}if(turns==2){ox=2-x;oy=1-y;}if(turns==3){ox=y;oy=2-x;}
                if(mirror)ox=w-1-ox;
                QCOMPARE(result.pixelColor(ox,oy),source.pixelColor(x,y));
            }
            QCOMPARE(source,original);
        }
        QVERIFY(orientedCopy(source,-1,false,3).isNull());
        QVERIFY(orientedCopy(source,4,false,3).isNull());
        QVERIFY(orientedCopy(source,0,false,3,5).isNull());
        QVERIFY(orientedCopy(source,0,false,0).isNull());
        QVERIFY(orientedCopy(source,0,false,8193).isNull());
        QCOMPARE(orientedCopy(source,1,true,4).size(),QSize(4,6));
    }
    void sharingCanvasAndWatermark() {
        QImage source(100, 60, QImage::Format_RGBA8888); source.fill(QColor("#2868b0"));
        source.setDevicePixelRatio(1.5); source.setColorSpace(QColorSpace::SRgb);
        const auto original = source;
        PresentationOptions options{25, PresentationStyle::Aurora, QStringLiteral("Demo"), false};
        const auto output = composePresentation(source, options);
        QCOMPARE(output.size(), QSize(150, 146));
        QCOMPARE(output.devicePixelRatio(), 1.5);
        QCOMPARE(output.colorSpace(), QColorSpace(QColorSpace::SRgb));
        QCOMPARE(output.pixelColor(25,25), source.pixelColor(0,0));
        QCOMPARE(output.pixelColor(124,84), source.pixelColor(99,59));
        QCOMPARE(source, original);
        for (int y=0; y<output.height(); ++y) for (int x=0; x<output.width(); ++x) QCOMPARE(output.pixelColor(x,y).alpha(), 255);
        options.watermark.clear();
        QCOMPARE(composePresentation(source, options).size(), QSize(150,110));
        options.rounded = true;
        const auto rounded = composePresentation(source, options);
        QVERIFY(rounded.pixelColor(25,25) != source.pixelColor(0,0));
        QCOMPARE(rounded.pixelColor(75,55), source.pixelColor(50,30));
        options.style = PresentationStyle::Dark;
        QVERIFY(composePresentation(source, options).pixelColor(0,0).lightness() < 80);
        options.style = PresentationStyle::Light;
        QVERIFY(composePresentation(source, options).pixelColor(0,0).lightness() > 200);
    }
    void framesPreserveContentAndIncludeChrome()
    {
        QImage source(800, 160, QImage::Format_RGBA8888);
        for (int y = 0; y < source.height(); ++y)
            for (int x = 0; x < source.width(); ++x)
                source.setPixelColor(x, y, QColor(x % 256, y % 256, (x + y) % 256));
        source.setDevicePixelRatio(2);
        source.setColorSpace(QColorSpace::SRgb);
        const auto original = source.copy();
        for (auto frame : {PresentationFrame::MacOS, PresentationFrame::Browser}) {
            PresentationOptions options{24, PresentationStyle::Light, {}, false, frame, QStringLiteral("Example 中文")};
            const int header = frame == PresentationFrame::MacOS ? 42 : 52;
            const auto output = composePresentation(source, options);
            QCOMPARE(output.size(), QSize(848, 208 + header));
            QCOMPARE(presentationSize(source.size(), options), output.size());
            QCOMPARE(output.devicePixelRatio(), 2.);
            for (int y = 0; y < source.height(); ++y)
                for (int x = 0; x < source.width(); ++x)
                    QCOMPARE(output.pixelColor(x + 24, y + 24 + header), source.pixelColor(x, y));
            QVERIFY(output.pixelColor(44, 24 + header / 2).red() > 200);
            QVERIFY(output.pixelColor(44, 24 + header / 2).green() < 140);
            options.watermark = QStringLiteral("Shot by NkShot");
            QCOMPARE(composePresentation(source, options).height(), output.height() + 36);
            options.watermark.clear();
            options.frameTitle.clear();
            QVERIFY(composePresentation(source, options) != output);
            options.padding = 0;
            options.rounded = true;
            QVERIFY(!composePresentation(source, options).isNull());
            QImage tiny(1, 1, QImage::Format_RGB32);
            tiny.fill(Qt::blue);
            QVERIFY(!composePresentation(tiny, options).isNull());
        }
        QCOMPARE(source, original);
        PresentationOptions options{0, PresentationStyle::Dark, {}, false, PresentationFrame::MacOS, {}};
        QImage tall(1, 32000, QImage::Format_RGB32);
        tall.fill(Qt::white);
        QVERIFY(composePresentation(tall, options).isNull());
        QVERIFY(composePresentation(source, options, 800 * 160).isNull());
        options.frame = PresentationFrame(7);
        QVERIFY(composePresentation(source, options).isNull());
        options.frame = PresentationFrame(-1);
        QVERIFY(composePresentation(source, options).isNull());
        options.frame = PresentationFrame::Browser;
        options.frameTitle = QString(121, QLatin1Char('a'));
        QVERIFY(composePresentation(source, options).isNull());
    }
    void deviceFramesKeepAllCornersAndAspectRatio()
    {
        const QColor colors[] = {QColor("#f92135"), QColor("#21f935"), QColor("#2135f9"), QColor("#f9df21")};
        for (auto size : {QSize(320, 200), QSize(180, 320), QSize(800, 80), QSize(80, 800)}) {
            QImage source(size, QImage::Format_RGB32);
            source.fill(QColor("#ed21f9"));
            for (int y = 0; y < 16; ++y)
                for (int x = 0; x < 16; ++x) {
                    source.setPixelColor(x, y, colors[0]);
                    source.setPixelColor(size.width() - 1 - x, y, colors[1]);
                    source.setPixelColor(x, size.height() - 1 - y, colors[2]);
                    source.setPixelColor(size.width() - 1 - x, size.height() - 1 - y, colors[3]);
                }
            source.setDevicePixelRatio(2);
            source.setColorSpace(QColorSpace::SRgb);
            const auto original = source.copy();
            for (auto frame : {PresentationFrame::MacDisplay, PresentationFrame::IPad, PresentationFrame::Surface, PresentationFrame::VisionPro}) {
                PresentationOptions options{24, PresentationStyle::Aurora, {}, true, frame, {}};
                const auto output = composePresentation(source, options);
                QVERIFY(!output.isNull());
                QCOMPARE(output.size(), presentationSize(size, options));
                QCOMPARE(output.devicePixelRatio(), 2.);
                QCOMPARE(output.colorSpace(), QColorSpace(QColorSpace::SRgb));
                QPointF centers[4];
                int counts[4] = {};
                for (int y = 0; y < output.height(); ++y)
                    for (int x = 0; x < output.width(); ++x) {
                        const auto pixel = output.pixelColor(x, y);
                        for (int i = 0; i < 4; ++i)
                            if (pixel == colors[i]) {
                                centers[i] += QPointF(x, y);
                                ++counts[i];
                            }
                    }
                for (int i = 0; i < 4; ++i) {
                    QVERIFY(counts[i] > 0);
                    centers[i] /= counts[i];
                }
                QVERIFY(centers[0].x() < centers[1].x());
                QVERIFY(centers[0].y() < centers[2].y());
                const qreal xScale = (centers[1].x() - centers[0].x()) / (size.width() - 16);
                const qreal yScale = (centers[2].y() - centers[0].y()) / (size.height() - 16);
                QVERIFY(qAbs(xScale - yScale) < .025);
                QVERIFY(xScale > .98);
                const auto preview = previewPresentation(source, options, 300);
                QCOMPARE(qMax(preview.width(), preview.height()), 300);
                options.watermark = QStringLiteral("Shot by NkShot");
                QCOMPARE(presentationSize(size, options).height(), output.height() + 36);
                options.watermark.clear();
                options.rounded = false;
                QCOMPARE(composePresentation(source, options), output);
                if (frame == PresentationFrame::IPad || frame == PresentationFrame::Surface)
                    QCOMPARE(output.height() > output.width(), size.height() > size.width());
                QVERIFY(composePresentation(source, options, qint64(output.width()) * output.height() - 1).isNull());
            }
            QCOMPARE(source, original);
        }
        QImage tiny(1, 1, QImage::Format_RGB32);
        tiny.fill(Qt::green);
        for (int frame = 3; frame <= 6; ++frame) {
            PresentationOptions options{0, PresentationStyle::Dark, {}, true, PresentationFrame(frame), {}};
            QVERIFY(!composePresentation(tiny, options).isNull());
            QVERIFY(presentationSize(QSize(8192, 32000), options).isEmpty());
        }
    }
    void sharingCanvasCompositesTransparency() {
        QImage source(100,60,QImage::Format_RGBA8888); source.fill(Qt::transparent);
        const auto result = composePresentation(source, {0, PresentationStyle::Light, {}, false});
        QCOMPARE(result.pixelColor(50,30).alpha(), 255);
        QVERIFY(result.pixelColor(50,30).lightness() > 200);
        QCOMPARE(source.pixelColor(50,30).alpha(), 0);
    }
    void sharingCanvasConvertsTaggedInput() {
        QImage source(40,30,QImage::Format_RGBA8888); source.fill(QColor(120,70,200));
        source.setColorSpace(QColorSpace::DisplayP3);
        const auto expected = source.convertedToColorSpace(QColorSpace::SRgb);
        const auto result = composePresentation(source, {0, PresentationStyle::Light, {}, false});
        QCOMPARE(result.pixelColor(20,15), expected.pixelColor(20,15));
        QCOMPARE(source.colorSpace(), QColorSpace(QColorSpace::DisplayP3));
    }
    void sharingCanvasRejectsInvalid() {
        QImage source(100,60,QImage::Format_RGB32); source.fill(Qt::white);
        PresentationOptions options;
        QVERIFY(composePresentation({}, options).isNull());
        options.padding = -1; QVERIFY(composePresentation(source, options).isNull());
        options.padding = 241; QVERIFY(composePresentation(source, options).isNull());
        options.padding = 0; options.style = PresentationStyle(5); QVERIFY(composePresentation(source, options).isNull());
        options.style = PresentationStyle::Light; options.watermark = QString(121, QLatin1Char('a'));
        QVERIFY(composePresentation(source, options).isNull());
        options.watermark.clear(); QVERIFY(composePresentation(source, options, 5999).isNull());
        QVERIFY(!composePresentation(source, options, 6000).isNull());
    }

    void dimensionsMetadataAndOwnership() {
        QImage source(120, 75, QImage::Format_RGBA8888);
        source.fill(QColor(20, 40, 80, 100));
        source.setDevicePixelRatio(1.5);
        source.setColorSpace(QColorSpace::SRgb);
        auto result = resizedToWidth(source, 64);
        QCOMPARE(result.size(), QSize(64, 40));
        QCOMPARE(result.devicePixelRatio(), 1.5);
        QCOMPARE(result.colorSpace(), source.colorSpace());
        QCOMPARE(result.pixelColor(10, 10).alpha(), 100);
        source.fill(Qt::red);
        QCOMPARE(result.pixelColor(10, 10).alpha(), 100);
        QCOMPARE(resizedToWidth(source, 120), source);
    }
    void rejectsUnsafeSizes() {
        QImage source(100, 100, QImage::Format_RGB32); source.fill(Qt::white);
        QVERIFY(resizedToWidth({}, 100).isNull());
        QVERIFY(resizedToWidth(source, 0).isNull());
        QVERIFY(resizedToWidth(source, 8193).isNull());
        QVERIFY(resizedToWidth(source, 100, 9999).isNull());
        QVERIFY(!resizedToWidth(source, 100, 10000).isNull());
        QImage tall(1, 100, QImage::Format_RGB32); tall.fill(Qt::white);
        QVERIFY(resizedToWidth(tall, 321).isNull());
        QVERIFY(!resizedToWidth(tall, 320).isNull());
    }
};
QTEST_MAIN(ImageTransformsTest)
#include "ImageTransformsTest.moc"
