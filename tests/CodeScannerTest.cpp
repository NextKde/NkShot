// SPDX-License-Identifier: GPL-3.0-only
#include "Imaging/CodeScanner.h"

#include <Prison/Barcode>
#include <QClipboard>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QtTest>

using namespace NkShot::Imaging;

class CodeScannerTest : public QObject
{
    Q_OBJECT

    static QImage textFixture(const QString &text, Prison::BarcodeType type = Prison::QRCode, bool transparent = false)
    {
        auto barcode = Prison::Barcode::create(type);
        if (!barcode) {
            return {};
        }
        if (type == Prison::QRCode) {
            // Prison's QString QR writer trims the rendered payload. Its byte
            // overload encodes these exact UTF-8 bytes, including outer spaces.
            barcode->setData(text.toUtf8());
        } else {
            barcode->setData(text);
        }
        auto image = barcode->toImage(type == Prison::QRCode ? QSizeF(512, 512) : QSizeF(1200, 300));
        if (transparent && !image.isNull()) {
            // Prison's QR renderer outputs RGBX8888 even with a transparent
            // background color. Build a valid alpha image from its opaque QR
            // instead, keeping the black modules and clearing white pixels.
            image = image.convertToFormat(QImage::Format_ARGB32);
            for (int y = 0; y < image.height(); ++y) {
                auto line = reinterpret_cast<QRgb *>(image.scanLine(y));
                for (int x = 0; x < image.width(); ++x) {
                    if (line[x] == qRgb(255, 255, 255)) {
                        line[x] = qRgba(0, 0, 0, 0);
                    }
                }
            }
        }
        return image;
    }

    bool mOpenedUrl = false;

private Q_SLOTS:
    void recordOpenedUrl(const QUrl &)
    {
        mOpenedUrl = true;
    }

    void exactQrText_data()
    {
        QTest::addColumn<QString>("payload");
        QTest::newRow("chinese") << QStringLiteral("中文截图编辑：你好，世界！");
        QTest::newRow("url") << QStringLiteral("https://example.invalid/path?q=NkShot&lang=zh#details");
        QTest::newRow("literal-html") << QStringLiteral("  <b>literal text</b>\n<script>alert('no')</script> &amp;  ");
    }

    void exactQrText()
    {
        QFETCH(QString, payload);
        const auto image = textFixture(payload);
        QVERIFY(!image.isNull());
        const auto result = scanImageCode(image);
        QCOMPARE(result.status, CodeScanResult::Text);
        QCOMPARE(result.text, payload);
        QVERIFY(result.binary.isEmpty());
    }

    void transparentHighDpiSnapshot()
    {
        const auto payload = QStringLiteral("中文 + literal <b>markup</b>");
        auto image = textFixture(payload, Prison::QRCode, true);
        QVERIFY(!image.isNull());
        QVERIFY(image.hasAlphaChannel());
        QCOMPARE(image.pixelColor(0, 0).alpha(), 0);
        image.setDevicePixelRatio(2.5);
        const auto original = image.copy();
        const auto cacheKey = image.cacheKey();
        const auto result = scanImageCode(image);
        QCOMPARE(result.status, CodeScanResult::Text);
        QCOMPARE(result.text, payload);
        QCOMPARE(image, original);
        QCOMPARE(image.devicePixelRatio(), 2.5);
        QCOMPARE(image.cacheKey(), cacheKey);
    }

    void binaryQr()
    {
        const auto payload = QByteArray::fromHex("00010203ff807f001b0a");
        auto barcode = Prison::Barcode::create(Prison::QRCode);
        QVERIFY(barcode.has_value());
        barcode->setData(payload);
        const auto image = barcode->toImage(QSizeF(512, 512));
        QVERIFY(!image.isNull());
        const auto result = scanImageCode(image);
        QCOMPARE(result.status, CodeScanResult::Binary);
        QCOMPARE(result.binary, payload);
        QVERIFY(result.text.isEmpty());
    }

    void oneDimensionalBarcode()
    {
        const auto payload = QStringLiteral("NKSHOT-12345");
        const auto image = textFixture(payload, Prison::Code128);
        QVERIFY(!image.isNull());
        const auto result = scanImageCode(image);
        QCOMPARE(result.status, CodeScanResult::Text);
        QCOMPARE(result.text, payload);
        QVERIFY(result.binary.isEmpty());
    }

    void invalidAndBlank()
    {
        const auto invalid = scanImageCode({});
        QCOMPARE(invalid.status, CodeScanResult::InvalidImage);
        QVERIFY(invalid.text.isEmpty());
        QVERIFY(invalid.binary.isEmpty());

        // One-bit storage keeps the oversized-input regression inexpensive.
        QImage oversized(5000, 4001, QImage::Format_Mono);
        QVERIFY(!oversized.isNull());
        QCOMPARE(scanImageCode(oversized).status, CodeScanResult::InvalidImage);

        QImage blank(512, 512, QImage::Format_RGB32);
        blank.fill(Qt::white);
        const auto absent = scanImageCode(blank);
        QCOMPARE(absent.status, CodeScanResult::NotFound);
        QVERIFY(absent.text.isEmpty());
        QVERIFY(absent.binary.isEmpty());
    }

    void noStaleResultsOrSideEffects()
    {
        const auto payload = QStringLiteral("https://example.invalid/do-not-open");
        const auto image = textFixture(payload);
        QVERIFY(!image.isNull());
        QGuiApplication::clipboard()->setText(QStringLiteral("clipboard stays unchanged"));
        QDesktopServices::setUrlHandler(QStringLiteral("https"), this, "recordOpenedUrl");
        const auto result = scanImageCode(image);
        QDesktopServices::unsetUrlHandler(QStringLiteral("https"));
        QCOMPARE(result.status, CodeScanResult::Text);
        QCOMPARE(result.text, payload);
        QVERIFY(!mOpenedUrl);
        QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("clipboard stays unchanged"));

        QImage blank(64, 64, QImage::Format_Grayscale8);
        blank.fill(Qt::white);
        QCOMPARE(scanImageCode(blank).status, CodeScanResult::NotFound);
        QCOMPARE(scanImageCode({}).status, CodeScanResult::InvalidImage);
        QCOMPARE(scanImageCode(image).text, payload);
    }
};

QTEST_MAIN(CodeScannerTest)
#include "CodeScannerTest.moc"
