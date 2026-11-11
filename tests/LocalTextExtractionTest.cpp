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
        QVERIFY(result.wordsComplete);
        QCOMPARE(result.words.size(), 3);
        QCOMPARE(selectedWordText(result.words, 0, 2), result.text);
        QCOMPARE(result.words[0].text, QStringLiteral("HELLO"));
        QCOMPARE(result.words[1].text, QStringLiteral("NKSHOT"));
        QCOMPARE(result.words[2].text, QStringLiteral("12345"));
        QCOMPARE(selectedWordText(result.words, 2, 1), QStringLiteral("NKSHOT 12345"));
        for (const auto &word : result.words) {
            QVERIFY(image.rect().contains(word.bounds));
            QVERIFY(word.bounds.width() > 0 && word.bounds.height() > 30);
            QCOMPARE(word.lineIndex, 0);
            bool containsInk = false;
            for (int y = word.bounds.top(); y <= word.bounds.bottom() && !containsInk; ++y)
                for (int x = word.bounds.left(); x <= word.bounds.right(); ++x) {
                    const auto color = image.pixelColor(x, y);
                    if (color.alpha() > 127 && color.lightness() < 100) { containsInk = true; break; }
                }
            QVERIFY(containsInk);
        }
        auto unitDpr = image; unitDpr.setDevicePixelRatio(1);
        const auto unitResult = extractLocalText(unitDpr, {QStringLiteral("eng")}, {});
        QVERIFY(unitResult.wordsComplete);
        QCOMPARE(unitResult.words.size(), result.words.size());
        for (int i = 0; i < result.words.size(); ++i)
            QCOMPARE(unitResult.words[i].bounds, result.words[i].bounds);
    }
    void chineseAndNoText() {
        const auto result=extractLocalText(fixture(QStringLiteral("中文截图编辑 12345"),QStringLiteral("Noto Sans CJK SC")),{QStringLiteral("chi_sim")},{});
        QCOMPARE(result.error,TextExtractionError::None);auto normalized=result.text;normalized.remove(u' ');
        QCOMPARE(normalized,QStringLiteral("中文截图编辑12345"));
        QVERIFY(result.wordsComplete);
        QVERIFY(!result.words.isEmpty());
        QCOMPARE(selectedWordText(result.words, 0, result.words.size() - 1), result.text);
        QString wordCharacters;
        for (const auto &word : result.words) {
            QVERIFY(QRect(0, 0, 1000, 220).contains(word.bounds));
            wordCharacters += word.text;
        }
        QCOMPARE(wordCharacters, QStringLiteral("中文截图编辑12345"));
        QImage blank(400,200,QImage::Format_RGB32);blank.fill(Qt::white);
        const auto empty=extractLocalText(blank,{QStringLiteral("eng")},{});
        QCOMPARE(empty.error,TextExtractionError::None);QVERIFY(empty.text.isEmpty());
        QVERIFY(empty.wordsComplete); QVERIFY(empty.words.isEmpty());
    }
    void multilineReadingOrderAndSpacing() {
        QImage image(1200, 340, QImage::Format_RGB32); image.fill(Qt::white);
        {
            QPainter painter(&image); QFont font(QStringLiteral("DejaVu Sans"));
            font.setPixelSize(58); painter.setFont(font); painter.setPen(Qt::black);
            painter.drawText(QPoint(55, 100), QStringLiteral("ALPHA, BETA GAMMA"));
            painter.drawText(QPoint(55, 245), QStringLiteral("DELTA EPSILON 67890"));
        }
        const auto result = extractLocalText(image, {QStringLiteral("eng")}, {});
        QCOMPARE(result.error, TextExtractionError::None);
        QCOMPARE(result.text, QStringLiteral("ALPHA, BETA GAMMA\nDELTA EPSILON 67890"));
        QVERIFY(result.wordsComplete); QCOMPARE(result.words.size(), 6);
        QCOMPARE(selectedWordText(result.words, 0, 5), result.text);
        QCOMPARE(selectedWordText(result.words, 2, 4), QStringLiteral("GAMMA\nDELTA EPSILON"));
        QCOMPARE(selectedWordText(result.words, 4, 2), QStringLiteral("GAMMA\nDELTA EPSILON"));
        for (int i = 0; i < result.words.size(); ++i) {
            const auto &word = result.words[i];
            QVERIFY(image.rect().contains(word.bounds));
            QCOMPARE(word.lineIndex, i < 3 ? 0 : 1);
            QVERIFY(i < 3 ? word.bounds.bottom() < 120 : word.bounds.top() > 170);
        }
        QCOMPARE(result.words[3].separatorBefore, QStringLiteral("\n"));
        QCOMPARE(selectedWordText(result.words, 3, 3), QStringLiteral("DELTA"));
    }
    void selectionSerialization() {
        const QList<TextWord> words{
            {QStringLiteral("中文"), {}, {}, 0},
            {QStringLiteral("编辑"), {}, {}, 0},
            {QStringLiteral("12345"), {}, QStringLiteral(" "), 0},
            {QStringLiteral("NEXT"), {}, QStringLiteral("\n"), 1},
            {QStringLiteral("literal <tag>"), {}, QStringLiteral("   "), 1},
        };
        QCOMPARE(selectedWordText(words, 0, 4), QStringLiteral("中文编辑 12345\nNEXT   literal <tag>"));
        QCOMPARE(selectedWordText(words, 1, 3), QStringLiteral("编辑 12345\nNEXT"));
        QCOMPARE(selectedWordText(words, 3, 1), selectedWordText(words, 1, 3));
        QCOMPARE(selectedWordText(words, 4, 4), QStringLiteral("literal <tag>"));
        QVERIFY(selectedWordText(words, -1, 2).isEmpty());
        QVERIFY(selectedWordText(words, 0, 5).isEmpty());
        QVERIFY(selectedWordText({}, 0, 0).isEmpty());
        QList<TextWord> excessive(2001);
        QVERIFY(selectedWordText(excessive, 0, 2000).isEmpty());
    }
    void boundedMetadataKeepsWholeText() {
        const auto image = fixture(QStringLiteral("HELLO NKSHOT 12345"), QStringLiteral("DejaVu Sans"));
        const auto exact = extractLocalText(image, {QStringLiteral("eng")}, {}, 15000, 3);
        QVERIFY(exact.wordsComplete); QCOMPARE(exact.words.size(), 3);
        const auto limited = extractLocalText(image, {QStringLiteral("eng")}, {}, 15000, 2);
        QCOMPARE(limited.error, TextExtractionError::None);
        QCOMPARE(limited.text, exact.text);
        QVERIFY(!limited.wordsComplete); QVERIFY(limited.words.isEmpty());
        QCOMPARE(extractLocalText(image, {QStringLiteral("eng")}, {}, 15000, 0).error, TextExtractionError::InvalidInput);
        QCOMPARE(extractLocalText(image, {QStringLiteral("eng")}, {}, 15000, 2001).error, TextExtractionError::InvalidInput);
    }
    void rejectionAndCancellation() {
        const auto image=fixture(QStringLiteral("TEST"),QStringLiteral("DejaVu Sans"));
        QCOMPARE(extractLocalText({}, {QStringLiteral("eng")},{}).error,TextExtractionError::InvalidInput);
        QCOMPARE(extractLocalText(image,{},{}).error,TextExtractionError::InvalidLanguage);
        QCOMPARE(extractLocalText(image,{QStringLiteral("../eng")},{}).error,TextExtractionError::InvalidLanguage);
        QCOMPARE(extractLocalText(image,{QStringLiteral("osd")},{}).error,TextExtractionError::InvalidLanguage);
        QCOMPARE(extractLocalText(image,{QStringLiteral("eng")},{},0).error,TextExtractionError::InvalidInput);
        const auto unavailable = extractLocalText(image,{QStringLiteral("nkshot_missing_model")},{});
        QCOMPARE(unavailable.error,TextExtractionError::Unavailable);
        QVERIFY(unavailable.words.isEmpty()); QVERIFY(!unavailable.wordsComplete);
        const auto cancelled=std::make_shared<std::atomic_bool>(true);
        QCOMPARE(extractLocalText(image,{QStringLiteral("eng")},cancelled).error,TextExtractionError::Cancelled);
        QImage wide(8193,25,QImage::Format_RGB32);
        QCOMPARE(extractLocalText(wide,{QStringLiteral("eng")},{}).error,TextExtractionError::InvalidInput);
    }
};
QTEST_MAIN(LocalTextExtractionTest)
#include "LocalTextExtractionTest.moc"
