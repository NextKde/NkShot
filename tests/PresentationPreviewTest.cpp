// SPDX-License-Identifier: GPL-3.0-only
#include "Gui/PresentationPreview.h"
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QColorSpace>
#include <QtTest>
class PresentationPreviewTest : public QObject {
    Q_OBJECT
    QImage preview(QQmlEngine &engine, const PresentationPreview &model) {
        const auto url = model.imageSource();
        auto provider = dynamic_cast<QQuickImageProvider *>(engine.imageProvider(url.host()));
        return provider ? provider->requestImage(url.path(), nullptr, {}) : QImage();
    }
private Q_SLOTS:
    void immutableSessionAndOutputAgreement() {
        QQmlEngine engine;
        PresentationPreview model(&engine);
        QImage source(640,360,QImage::Format_RGBA8888);source.fill(QColor(100,70,180,170));
        source.setColorSpace(QColorSpace::DisplayP3);source.setDevicePixelRatio(2);
        const auto original=source.copy();
        QVERIFY(model.begin(source));QVERIFY(!model.ready());QCOMPARE(model.sourceSize(),source.size());
        source.fill(Qt::red);
        for(int style=0;style<3;++style)for(bool rounded:{false,true}) {
            model.update(48,style,QStringLiteral("Preview 中文"),rounded);
            QVERIFY(model.ready());QVERIFY(model.error().isEmpty());
            auto expected=NkShot::Imaging::composePresentation(original,{48,NkShot::Imaging::PresentationStyle(style),QStringLiteral("Preview 中文"),rounded});
            expected.setDevicePixelRatio(1);
            QCOMPARE(preview(engine,model),expected);
        }
        const auto first=model.imageSource();
        model.update(241,2,{},true);QVERIFY(!model.ready());QVERIFY(!model.error().isEmpty());QVERIFY(model.imageSource().isEmpty());
        model.update(0,0,{},false);QVERIFY(model.ready());QVERIFY(model.imageSource()!=first);
        const auto latest=preview(engine,model);QCOMPARE(latest.size(),original.size());
        const auto accepted=model.source();const auto options=model.options();
        model.clear();QVERIFY(!model.ready());QVERIFY(model.sourceSize().isEmpty());QVERIFY(model.imageSource().isEmpty());
        model.update(48,2,{},true);QVERIFY(!model.ready());
        QVERIFY(!NkShot::Imaging::composePresentation(accepted,options).isNull());
        QImage next(40,30,QImage::Format_RGB32);next.fill(Qt::green);
        QVERIFY(model.begin(next));model.update(0,0,{},false);QCOMPARE(preview(engine,model).pixelColor(20,15),QColor(Qt::green));
        QVERIFY(!model.begin({}));QVERIFY(!model.ready());
    }
    void externallyMutableInputIsCopied() {
        QQmlEngine engine;PresentationPreview model(&engine);
        QByteArray buffer(40*30*4, char(0));
        QImage wrapped(reinterpret_cast<uchar *>(buffer.data()),40,30,40*4,QImage::Format_RGBA8888);
        wrapped.fill(QColor(30,80,160));
        QVERIFY(model.begin(wrapped));
        // Change backing storage directly: no QImage detach can protect the caller.
        buffer[0]=char(255);buffer[1]=char(0);buffer[2]=char(0);
        model.update(0,0,{},false);
        QCOMPARE(preview(engine,model).pixelColor(0,0),QColor(30,80,160));
    }
    void boundedPreviewAndLimits() {
        QImage source(1600,2400,QImage::Format_RGB32);source.fill(QColor("#2868b0"));source.setDevicePixelRatio(2);
        const auto original=source.copy();
        const NkShot::Imaging::PresentationOptions options{48,NkShot::Imaging::PresentationStyle::Aurora,QStringLiteral("Test"),true};
        auto result=NkShot::Imaging::previewPresentation(source,options,640);
        QVERIFY(!result.isNull());QCOMPARE(result.height(),640);QVERIFY(result.width()<640);QCOMPARE(result.devicePixelRatio(),1.);
        QCOMPARE(result.pixelColor(result.width()/2,result.height()/2),QColor("#2868b0"));QCOMPARE(source,original);
        QVERIFY(NkShot::Imaging::previewPresentation(source,options,0).isNull());
        QVERIFY(NkShot::Imaging::previewPresentation(source,options,2049).isNull());
        QVERIFY(NkShot::Imaging::previewPresentation(source,options,640,100).isNull());
        QImage tall(1,32000,QImage::Format_RGB32);tall.fill(Qt::green);
        result=NkShot::Imaging::previewPresentation(tall,{0,NkShot::Imaging::PresentationStyle::Light,{},false});
        QCOMPARE(result.size(),QSize(1,1024));
    }
    void engineBeforeModelDestruction() {
        auto engine=new QQmlEngine;auto model=new PresentationPreview(engine);
        QImage source(20,20,QImage::Format_RGB32);source.fill(Qt::white);QVERIFY(model->begin(source));model->update(0,0,{},false);
        delete engine;model->clear();model->update(0,0,{},false);delete model;
    }
};
QTEST_MAIN(PresentationPreviewTest)
#include "PresentationPreviewTest.moc"
