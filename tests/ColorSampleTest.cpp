// SPDX-License-Identifier: GPL-3.0-only
#include "Imaging/ColorSample.h"

#include <QColorSpace>
#include <QtTest>
#include <array>
#include <cmath>
#include <limits>

using namespace NkShot::Imaging;

class ColorSampleTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void independentSnapshot()
    {
        QImage source(3, 2, QImage::Format_ARGB32);
        source.fill(QColor(12, 34, 56, 78));
        source.setDevicePixelRatio(2.5);
        const auto snapshot = colorSampleSnapshot(source);
        QCOMPARE(snapshot.size(), source.size());
        QCOMPARE(snapshot.format(), QImage::Format_ARGB32);
        QCOMPARE(snapshot.devicePixelRatio(), 1.0);
        QCOMPARE(snapshot.colorSpace(), QColorSpace(QColorSpace::SRgb));
        QCOMPARE(snapshot.pixelColor(1, 1), QColor(12, 34, 56, 78));
        QVERIFY(snapshot.constBits() != source.constBits());
        source.fill(Qt::red);
        QCOMPARE(snapshot.pixelColor(1, 1), QColor(12, 34, 56, 78));
        QVERIFY(!source.colorSpace().isValid());
        QCOMPARE(source.devicePixelRatio(), 2.5);
    }

    void externallyBackedSnapshot()
    {
        for (bool tagged : {false, true}) {
            QImage snapshot;
            {
                std::array<QRgb, 4> memory{qRgba(20, 40, 60, 80), qRgb(1, 2, 3), qRgb(4, 5, 6), qRgb(7, 8, 9)};
                QImage source(reinterpret_cast<uchar *>(memory.data()), 2, 2, 8, QImage::Format_ARGB32);
                if (tagged) {
                    source.setColorSpace(QColorSpace::SRgb);
                }
                snapshot = colorSampleSnapshot(source);
                QVERIFY(snapshot.constBits() != source.constBits());
                memory[0] = qRgb(255, 0, 0); // Bypass Qt's copy-on-write.
                QCOMPARE(snapshot.pixelColor(0, 0), QColor(20, 40, 60, 80));
            }
            QCOMPARE(snapshot.pixelColor(1, 1), QColor(7, 8, 9));
        }
    }

    void colorSpaceConversion()
    {
        for (auto profile : {QColorSpace::DisplayP3, QColorSpace::SRgbLinear}) {
            QImage source(2, 1, QImage::Format_ARGB32);
            source.fill(QColor(100, 80, 140, 123));
            source.setColorSpace(profile);
            const auto expected = source.convertedToColorSpace(QColorSpace::SRgb, QImage::Format_ARGB32);
            const auto snapshot = colorSampleSnapshot(source);
            QCOMPARE(snapshot.pixelColor(0, 0), expected.pixelColor(0, 0));
            QVERIFY(snapshot.pixelColor(0, 0) != source.pixelColor(0, 0));
            QCOMPARE(snapshot.pixelColor(0, 0).alpha(), 123);
            QCOMPARE(snapshot.colorSpace(), QColorSpace(QColorSpace::SRgb));
            QCOMPARE(source.colorSpace(), QColorSpace(profile));
        }
    }

    void premultipliedAndPhysicalPixels()
    {
        QImage source(4, 2, QImage::Format_ARGB32_Premultiplied);
        source.fill(QColor(100, 50, 200, 128));
        source.setPixelColor(3, 1, QColor(20, 40, 60, 0));
        source.setDevicePixelRatio(2);
        const auto snapshot = colorSampleSnapshot(source);
        QCOMPARE(snapshot.format(), QImage::Format_ARGB32);
        // Unpremultiplication may retain extra QColor precision; the contract is 8-bit.
        QCOMPARE(snapshot.pixelColor(0, 0).rgba(), source.pixelColor(0, 0).rgba());
        QCOMPARE(snapshot.pixelColor(3, 1).alpha(), 0);
        QCOMPARE(snapshot.size(), QSize(4, 2));
        QCOMPARE(colorSamplePixelAt({3.5, 1.5}, {4, 2}, snapshot.size()), QPoint(3, 1));
    }

    void sizeLimit()
    {
        QVERIFY(colorSampleSnapshot({}).isNull());
        QImage boundary(5000, 4000, QImage::Format_Grayscale8);
        QVERIFY(!boundary.isNull());
        boundary.fill(42);
        {
            const auto snapshot = colorSampleSnapshot(boundary);
            QCOMPARE(snapshot.size(), boundary.size());
            QCOMPARE(snapshot.pixelColor(4999, 3999), QColor(42, 42, 42));
        }
        QImage excessive(5001, 4000, QImage::Format_Grayscale8);
        QVERIFY(!excessive.isNull());
        QVERIFY(colorSampleSnapshot(excessive).isNull());
    }

    void serialization()
    {
        QCOMPARE(colorSampleHex(QColor(10, 171, 255)), QStringLiteral("#0AABFF"));
        QCOMPARE(colorSampleHex(QColor(10, 171, 255, 128)), QStringLiteral("#0AABFF80"));
        QCOMPARE(colorSampleHex(QColor(10, 171, 255, 0)), QStringLiteral("#0AABFF00"));
        QCOMPARE(colorSampleRgb(QColor(10, 171, 255)), QStringLiteral("rgb(10, 171, 255)"));
        QCOMPARE(colorSampleRgb(QColor(10, 171, 255, 128)), QStringLiteral("rgba(10, 171, 255, 0.502)"));
        QCOMPARE(colorSampleRgb(QColor(10, 171, 255, 0)), QStringLiteral("rgba(10, 171, 255, 0)"));
        QCOMPARE(colorSampleRgb(QColor(10, 171, 255, 51)), QStringLiteral("rgba(10, 171, 255, 0.2)"));
        QCOMPARE(colorSampleRgb(QColor(10, 171, 255, 1)), QStringLiteral("rgba(10, 171, 255, 0.004)"));
        QCOMPARE(colorSampleRgb(QColor(10, 171, 255, 254)), QStringLiteral("rgba(10, 171, 255, 0.996)"));
        QVERIFY(colorSampleHex(QColor()).isEmpty());
        QVERIFY(colorSampleRgb(QColor()).isEmpty());
    }

    void aspectFit()
    {
        const QPoint invalid(-1, -1);
        QCOMPARE(colorSamplePixelAt({0, 25}, {100, 100}, {4, 2}), QPoint(0, 0));
        QCOMPARE(colorSamplePixelAt({99.99, 74.99}, {100, 100}, {4, 2}), QPoint(3, 1));
        QCOMPARE(colorSamplePixelAt({25, 50}, {100, 100}, {4, 2}), QPoint(1, 1));
        for (QPointF p : {QPointF(-0.01, 25), QPointF(0, 24.99), QPointF(100, 25), QPointF(0, 75)}) {
            QCOMPARE(colorSamplePixelAt(p, {100, 100}, {4, 2}), invalid);
        }
        QCOMPARE(colorSamplePixelAt({25, 0}, {100, 100}, {2, 4}), QPoint(0, 0));
        QCOMPARE(colorSamplePixelAt({74.99, 99.99}, {100, 100}, {2, 4}), QPoint(1, 3));
        QCOMPARE(colorSamplePixelAt({75, 0}, {100, 100}, {2, 4}), invalid);
        QCOMPARE(colorSamplePixelAt({24.99, 0}, {100, 100}, {2, 4}), invalid);
        QCOMPARE(colorSamplePixelAt({0, 100}, {100, 100}, {2, 4}), invalid);
        QCOMPARE(colorSamplePixelAt({0, 0}, {0.5, 0.5}, {1, 1}), QPoint(0, 0));
        QCOMPARE(colorSamplePixelAt({0.499, 0.499}, {0.5, 0.5}, {100, 100}), QPoint(99, 99));
    }

    void invalidAndExtremeMapping()
    {
        const QPoint invalid(-1, -1);
        const double infinity = std::numeric_limits<double>::infinity();
        const double nan = std::numeric_limits<double>::quiet_NaN();
        for (double v : {infinity, -infinity, nan}) {
            QCOMPARE(colorSamplePixelAt({v, 0}, {100, 100}, {10, 10}), invalid);
            QCOMPARE(colorSamplePixelAt({0, v}, {100, 100}, {10, 10}), invalid);
            QCOMPARE(colorSamplePixelAt({0, 0}, {v, 100}, {10, 10}), invalid);
            QCOMPARE(colorSamplePixelAt({0, 0}, {100, v}, {10, 10}), invalid);
        }
        for (QSizeF s : {QSizeF(0, 100), QSizeF(100, 0), QSizeF(-1, 100), QSizeF(100, -1)}) {
            QCOMPARE(colorSamplePixelAt({0, 0}, s, {10, 10}), invalid);
        }
        for (QSize s : {QSize(0, 10), QSize(10, 0), QSize(-1, 10), QSize(10, -1)}) {
            QCOMPARE(colorSamplePixelAt({0, 0}, {100, 100}, s), invalid);
        }
        const double huge = std::numeric_limits<double>::max();
        const double tiny = std::numeric_limits<double>::denorm_min();
        const int largest = std::numeric_limits<int>::max();
        QCOMPARE(colorSamplePixelAt({huge / 2, huge / 2}, {huge, huge}, {2, 2}), QPoint(1, 1));
        QCOMPARE(colorSamplePixelAt({0, 0}, {tiny, tiny}, {largest, largest}), QPoint(0, 0));
        QCOMPARE(colorSamplePixelAt({tiny, 0}, {tiny, tiny}, {largest, largest}), invalid);
        QCOMPARE(colorSamplePixelAt({std::nextafter(huge, 0.0), 0}, {huge, huge}, {largest, largest}), QPoint(largest - 1, 0));
        QCOMPARE(colorSamplePixelAt({-huge, -huge}, {tiny, tiny}, {1, 1}), invalid);
    }

    void neighborhood()
    {
        QImage source(13, 12, QImage::Format_ARGB32);
        source.setColorSpace(QColorSpace::SRgb);
        source.setDevicePixelRatio(2);
        for (int y = 0; y < source.height(); ++y) {
            for (int x = 0; x < source.width(); ++x) {
                source.setPixelColor(x, y, QColor(x * 10, y * 10, 50, 100 + x));
            }
        }
        for (QPoint selected : {QPoint(0, 0), QPoint(12, 0), QPoint(0, 11), QPoint(12, 11), QPoint(6, 6)}) {
            const auto patch = colorSampleNeighborhood(source, selected);
            QCOMPARE(patch.size(), QSize(11, 11));
            QCOMPARE(patch.devicePixelRatio(), 1.0);
            QCOMPARE(patch.colorSpace(), source.colorSpace());
            QCOMPARE(patch.pixelColor(5, 5), source.pixelColor(selected));
            for (int y = 0; y < 11; ++y) {
                for (int x = 0; x < 11; ++x) {
                    const QPoint p = selected + QPoint(x - 5, y - 5);
                    QCOMPARE(patch.pixelColor(x, y), source.rect().contains(p) ? source.pixelColor(p) : QColor(Qt::transparent));
                }
            }
        }
        for (QPoint p : {QPoint(-1, 0), QPoint(0, -1), QPoint(13, 0), QPoint(0, 12)}) {
            QVERIFY(colorSampleNeighborhood(source, p).isNull());
        }
        QVERIFY(colorSampleNeighborhood({}, {0, 0}).isNull());
    }
};

QTEST_GUILESS_MAIN(ColorSampleTest)
#include "ColorSampleTest.moc"
