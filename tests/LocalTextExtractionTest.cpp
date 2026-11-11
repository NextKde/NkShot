// SPDX-License-Identifier: GPL-3.0-only
#include "Imaging/LocalTextExtraction.h"
#include <QtTest>
#include <QPainter>
#include <QClipboard>
using namespace NkShot::Imaging;
class LocalTextExtractionTest : public QObject {
    Q_OBJECT
    static QImage fixture(const QString &text,const QString &family,bool alpha=false) {
        QImage image(1000,220,QImage::Format_ARGB32_Premultiplied);image.fill(alpha?Qt::transparent:Qt::white);
        QPainter p(&image);QFont f(family);f.setPixelSize(64);p.setFont(f);p.setPen(Qt::black);p.drawText(image.rect(),Qt::AlignCenter,text);return image;
    }
private Q_SLOTS:
    void englishAndTransparentHighDpi() {
        auto image=fixture(QStringLiteral("HELLO NKSHOT 12345"),QStringLiteral("DejaVu Sans"),true);image.setDevicePixelRatio(2);
        const auto original=image;QGuiApplication::clipboard()->setText(QStringLiteral("unchanged"));
        const auto result=extractLocalText(image,{QStringLiteral("eng")},{});
        QCOMPARE(result.error,TextExtractionError::None);QCOMPARE(result.text.simplified(),QStringLiteral("HELLO NKSHOT 12345"));
        QCOMPARE(image,original);QCOMPARE(QGuiApplication::clipboard()->text(),QStringLiteral("unchanged"));
    }
    void chineseAndNoText() {
        const auto result=extractLocalText(fixture(QStringLiteral("中文截图编辑 12345"),QStringLiteral("Noto Sans CJK SC")),{QStringLiteral("chi_sim")},{});
        QCOMPARE(result.error,TextExtractionError::None);auto normalized=result.text;normalized.remove(u' ');
        QCOMPARE(normalized,QStringLiteral("中文截图编辑12345"));
        QImage blank(400,200,QImage::Format_RGB32);blank.fill(Qt::white);
        const auto empty=extractLocalText(blank,{QStringLiteral("eng")},{});
        QCOMPARE(empty.error,TextExtractionError::None);QVERIFY(empty.text.isEmpty());
    }
    void rejectionAndCancellation() {
        const auto image=fixture(QStringLiteral("TEST"),QStringLiteral("DejaVu Sans"));
        QCOMPARE(extractLocalText({}, {QStringLiteral("eng")},{}).error,TextExtractionError::InvalidInput);
        QCOMPARE(extractLocalText(image,{},{}).error,TextExtractionError::InvalidLanguage);
        QCOMPARE(extractLocalText(image,{QStringLiteral("../eng")},{}).error,TextExtractionError::InvalidLanguage);
        QCOMPARE(extractLocalText(image,{QStringLiteral("osd")},{}).error,TextExtractionError::InvalidLanguage);
        QCOMPARE(extractLocalText(image,{QStringLiteral("eng")},{},0).error,TextExtractionError::InvalidInput);
        QCOMPARE(extractLocalText(image,{QStringLiteral("nkshot_missing_model")},{}).error,TextExtractionError::Unavailable);
        const auto cancelled=std::make_shared<std::atomic_bool>(true);
        QCOMPARE(extractLocalText(image,{QStringLiteral("eng")},cancelled).error,TextExtractionError::Cancelled);
        QImage wide(8193,25,QImage::Format_RGB32);
        QCOMPARE(extractLocalText(wide,{QStringLiteral("eng")},{}).error,TextExtractionError::InvalidInput);
    }
};
QTEST_MAIN(LocalTextExtractionTest)
#include "LocalTextExtractionTest.moc"
