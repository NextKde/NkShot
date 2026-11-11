// SPDX-License-Identifier: GPL-3.0-only
#include "Imaging/ImageTransforms.h"
#include <QtTest>
#include <QColorSpace>
using namespace NkShot::Imaging;
class ImageTransformsTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
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
QTEST_GUILESS_MAIN(ImageTransformsTest)
#include "ImageTransformsTest.moc"
