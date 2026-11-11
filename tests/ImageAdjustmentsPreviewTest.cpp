// SPDX-License-Identifier: GPL-3.0-only
#include "Gui/ImageAdjustmentsPreview.h"
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QColorSpace>
#include <QtTest>
class ImageAdjustmentsPreviewTest : public QObject {
    Q_OBJECT
    QImage lookup(QQmlEngine &engine,const QUrl &url) {
        auto provider=dynamic_cast<QQuickImageProvider *>(engine.imageProvider(url.host()));
        return provider?provider->requestImage(url.path().mid(1),nullptr,QSize()):QImage();
    }
private Q_SLOTS:
    void immutableInputAndPreview() {
        QQmlEngine engine;ImageAdjustmentsPreview model(&engine);
        QByteArray storage(640*360*4, char(0));
        QImage external(reinterpret_cast<uchar *>(storage.data()),640,360,640*4,QImage::Format_RGBA8888);
        external.fill(QColor(70,120,190,170));external.setColorSpace(QColorSpace::DisplayP3);external.setDevicePixelRatio(2);
        const auto original=external.copy();
        QVERIFY(model.begin(external));QVERIFY(!model.ready());QCOMPARE(model.sourceSize(),original.size());
        // Mutate the external allocation directly, not a QImage detach-on-write.
        storage.fill(char(0));
        QCOMPARE(model.source(),original);QCOMPARE(model.source().devicePixelRatio(),2.);
        QCOMPARE(model.source().colorSpace(),QColorSpace(QColorSpace::DisplayP3));
        auto normalized=original.convertedToColorSpace(QColorSpace::SRgb);normalized.setDevicePixelRatio(1);
        for(const auto &options:{NkShot::Imaging::AdjustmentOptions{},NkShot::Imaging::AdjustmentOptions{25,-30,60},NkShot::Imaging::AdjustmentOptions{-100,100,-100}}) {
            model.update(options.brightness,options.contrast,options.saturation);
            QVERIFY(model.ready());QVERIFY(model.error().isEmpty());
            const auto preview=lookup(engine,model.imageSource());
            QCOMPARE(preview,NkShot::Imaging::adjustedCopy(normalized,options));
            QCOMPARE(preview.devicePixelRatio(),1.);QCOMPARE(preview.colorSpace(),QColorSpace(QColorSpace::SRgb));
            QCOMPARE(model.options().brightness,options.brightness);QCOMPARE(model.options().contrast,options.contrast);QCOMPARE(model.options().saturation,options.saturation);
            QCOMPARE(model.source(),original);
        }
    }
    void limitsRevisionClearAndRestart() {
        QQmlEngine engine;ImageAdjustmentsPreview model(&engine);
        QImage source(1600,2400,QImage::Format_ARGB32_Premultiplied);source.fill(QColor(QStringLiteral("#2a7db2")));source.setDevicePixelRatio(2);
        QVERIFY(model.begin(source));model.update(20,-10,30);QVERIFY(model.ready());
        auto oldUrl=model.imageSource();const auto image=lookup(engine,oldUrl);
        QVERIFY(!image.isNull());QCOMPARE(image.height(),1024);QVERIFY(image.width()<=1024);QCOMPARE(image.devicePixelRatio(),1.);QCOMPARE(image.colorSpace(),QColorSpace(QColorSpace::SRgb));
        model.update(21,-10,30);QVERIFY(model.ready());QVERIFY(model.imageSource()!=oldUrl);QVERIFY(lookup(engine,oldUrl).isNull());
        for(const auto &bad:{NkShot::Imaging::AdjustmentOptions{-101,0,0},NkShot::Imaging::AdjustmentOptions{0,101,0},NkShot::Imaging::AdjustmentOptions{0,0,101}}) {
            model.update(bad.brightness,bad.contrast,bad.saturation);QVERIFY(!model.ready());QVERIFY(!model.error().isEmpty());QVERIFY(model.imageSource().isEmpty());
        }
        model.update(0,0,0);QVERIFY(model.ready());oldUrl=model.imageSource();
        const auto accepted=model.source();const auto options=model.options();
        model.clear();QVERIFY(!model.ready());QVERIFY(model.sourceSize().isEmpty());QVERIFY(model.imageSource().isEmpty());QVERIFY(model.error().isEmpty());QVERIFY(lookup(engine,oldUrl).isNull());
        model.update(20,10,5);QVERIFY(!model.ready());
        QVERIFY(!NkShot::Imaging::adjustedCopy(accepted,options).isNull());
        QImage next(30,20,QImage::Format_RGB32);next.fill(Qt::green);
        QVERIFY(model.begin(next));QVERIFY(!model.ready());QCOMPARE(model.options().brightness,0);QCOMPARE(model.options().contrast,0);QCOMPARE(model.options().saturation,0);
        model.update(0,0,0);QVERIFY(model.ready());QCOMPARE(lookup(engine,model.imageSource()).pixelColor(15,10),QColor(Qt::green));
        QVERIFY(!model.begin(QImage()));QVERIFY(!model.ready());QVERIFY(model.sourceSize().isEmpty());
        QImage tall(1,32000,QImage::Format_RGB32);tall.fill(Qt::white);QVERIFY(model.begin(tall));model.update(1,0,0);QCOMPARE(lookup(engine,model.imageSource()).size(),QSize(1,1024));
        QImage tooWide(8193,1,QImage::Format_RGB32);QVERIFY(!model.begin(tooWide));QVERIFY(!model.ready());
    }
    void engineBeforeModelAndProviderRemoval() {
        auto engine=new QQmlEngine;auto model=new ImageAdjustmentsPreview(engine);
        QImage source(20,20,QImage::Format_RGB32);source.fill(Qt::white);
        QVERIFY(model->begin(source));model->update(10,0,0);QVERIFY(model->ready());
        delete engine;model->update(0,0,0);model->clear();QVERIFY(!model->ready());QVERIFY(!model->begin(source));delete model;
        QQmlEngine other;
        auto temporary=new ImageAdjustmentsPreview(&other);QVERIFY(temporary->begin(source));temporary->update(0,0,0);const auto providerId=temporary->imageSource().host();
        QVERIFY(other.imageProvider(providerId));delete temporary;QVERIFY(!other.imageProvider(providerId));
    }
};
QTEST_MAIN(ImageAdjustmentsPreviewTest)
#include "ImageAdjustmentsPreviewTest.moc"
