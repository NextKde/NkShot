// SPDX-License-Identifier: GPL-3.0-only
#include "Imaging/TextCover.h"
#include <QColorSpace>
#include <QPainter>
#include <QtTest>
#include <limits>
using namespace NkShot::Imaging;
class TextCoverTest : public QObject {
    Q_OBJECT
    static TextWord word(const QString &text,QRect bounds,const QString &separator=QString()) {
        return {text,bounds,separator,0};
    }
private Q_SLOTS:
    void literalCaseWhitespaceAndWholeWords() {
        const QList<TextWord> words{word(QStringLiteral("SECRET"),{1,1,30,10}),
            word(QStringLiteral("Code"),{35,1,25,10},QStringLiteral("  \t")),
            word(QStringLiteral("private"),{5,30,30,12},QStringLiteral("\n"))};
        auto result=findTextCoverMatches(words,{100,80},QStringLiteral(" secret\n code "));
        QVERIFY(result.valid);QVERIFY(!result.overflow);QCOMPARE(result.matches.size(),1);
        QCOMPARE(result.matches.first().regions,QList<QRect>({QRect(0,0,33,13),QRect(33,0,29,13)}));
        QCOMPARE(result.matches.first().text,QStringLiteral("SECRET  \tCode"));
        result=findTextCoverMatches(words,{100,80},QStringLiteral("code\nPRIVATE"));
        QVERIFY(result.valid);QCOMPARE(result.matches.size(),1);QCOMPARE(result.matches.first().regions.size(),2);
        QCOMPARE(result.matches.first().regions.last(),QRect(3,28,34,16));
        result=findTextCoverMatches(words,{100,80},QStringLiteral("cret"));
        QCOMPARE(result.matches.first().regions,QList<QRect>({QRect(0,0,33,13)}));
        result=findTextCoverMatches(words,{100,80},QStringLiteral("absent"));
        QVERIFY(result.valid);QVERIFY(result.matches.isEmpty());
    }
    void unicodeAndNonoverlappingPolicy() {
        const QList<TextWord> words{word(QString::fromUtf8("隐私Σ"),{0,0,20,10}),
            word(QString::fromUtf8("地址"),{25,0,20,10}),
            word(QStringLiteral("aaaa"),{0,20,40,10},QStringLiteral("\n"))};
        const auto result=findTextCoverMatches(words,{60,40},QString::fromUtf8("私σ地址"));
        QVERIFY(result.valid);QCOMPARE(result.matches.size(),1);QCOMPARE(result.matches.first().regions.size(),2);
        auto repeat=findTextCoverMatches(words,{60,40},QStringLiteral("aa"));
        QVERIFY(repeat.valid);QCOMPARE(repeat.matches.size(),2);
        QCOMPARE(repeat.matches.at(0).regions,repeat.matches.at(1).regions);
        repeat=findTextCoverMatches(words,{60,40},QStringLiteral("aaa"));
        QCOMPARE(repeat.matches.size(),1);
    }
    void invalidMetadataAndOverflow() {
        const QList<TextWord> valid{word(QStringLiteral("data"),{0,0,10,10})};
        for(const auto &query:{QString(),QStringLiteral(" \n\t"),QString(257,QChar(u'a')),QString(QChar(u'\0'))})
            QVERIFY(!findTextCoverMatches(valid,{20,20},query).valid);
        for(const auto &bounds:{QRect(-1,0,3,3),QRect(0,0,0,5),QRect(19,0,2,2),QRect(std::numeric_limits<int>::max()-4,0,3,3)})
            QVERIFY(!findTextCoverMatches({word(QStringLiteral("data"),bounds)},{20,20},QStringLiteral("data")).valid);
        QVERIFY(!findTextCoverMatches({word(QStringLiteral("data"),{0,0,3,3},QStringLiteral("forged"))},{20,20},QStringLiteral("data")).valid);
        QVERIFY(!findTextCoverMatches({word(QString(128*1024+1,QChar(u'a')),{0,0,3,3})},{20,20},QStringLiteral("a")).valid);
        QList<TextWord> many;for(int i=0;i<2001;++i)many.append(valid.first());
        QVERIFY(!findTextCoverMatches(many,{20,20},QStringLiteral("data")).valid);
        QVERIFY(!findTextCoverMatches(valid,{8193,10},QStringLiteral("data")).valid);
        QVERIFY(!findTextCoverMatches(valid,{5000,5000},QStringLiteral("data")).valid);
        auto exactly=findTextCoverMatches({word(QString(100,QChar(u'a')),{0,0,10,10})},{20,20},QStringLiteral("a"));
        QVERIFY(exactly.valid);QCOMPARE(exactly.matches.size(),100);
        auto overflow=findTextCoverMatches({word(QString(101,QChar(u'a')),{0,0,10,10})},{20,20},QStringLiteral("a"));
        QVERIFY(!overflow.valid);QVERIFY(overflow.overflow);QVERIFY(overflow.matches.isEmpty());
        const auto label=findTextCoverMatches({word(QString(1000,QChar(u'x')),{0,0,10,10})},{20,20},QString(20,QChar(u'x')));
        QVERIFY(label.valid);QCOMPARE(label.matches.first().text.size(),512);
    }
    void opaquePixelsMetadataAndImmutableSource() {
        QImage source(30,20,QImage::Format_RGBA8888);source.fill(QColor(80,130,190,128));
        source.setPixelColor(20,10,QColor(200,10,50,0));
        source.setColorSpace(QColorSpace::DisplayP3);source.setDevicePixelRatio(2);
        source.setText(QStringLiteral("secret"),QStringLiteral("metadata not for sharing"));
        const auto before=source.copy();
        const QList<QRect> regions{QRect(2,3,8,6),QRect(25,15,5,5)};
        const auto output=coveredTextCopy(source,regions);
        QVERIFY(!output.isNull());QCOMPARE(output.size(),source.size());QCOMPARE(output.devicePixelRatio(),2.);
        QCOMPARE(output.format(),QImage::Format_RGB32);QCOMPARE(output.colorSpace(),QColorSpace(QColorSpace::SRgb));
        QVERIFY(output.textKeys().isEmpty());QCOMPARE(source,before);QCOMPARE(source.text(QStringLiteral("secret")),QStringLiteral("metadata not for sharing"));
        auto normalized=source.convertedToColorSpace(QColorSpace::SRgb);normalized.setDevicePixelRatio(1);
        QImage expected(source.size(),QImage::Format_RGB32);expected.fill(Qt::white);
        {QPainter painter(&expected);painter.drawImage(QPoint(),normalized);}
        for(int y=0;y<output.height();++y)for(int x=0;x<output.width();++x) {
            const QPoint p(x,y);const bool covered=regions.at(0).contains(p)||regions.at(1).contains(p);
            QCOMPARE(output.pixelColor(p),covered?QColor(Qt::black):expected.pixelColor(p));
            QCOMPARE(output.pixelColor(p).alpha(),255);
        }
        QCOMPARE(output.pixelColor(20,10),QColor(Qt::white));
        QCOMPARE(source.colorSpace(),before.colorSpace());QCOMPARE(source.devicePixelRatio(),before.devicePixelRatio());
    }
    void renderRejectsInvalidSelection() {
        QImage source(20,20,QImage::Format_ARGB32);source.fill(Qt::red);
        QVERIFY(coveredTextCopy(source,{}).isNull());QVERIFY(coveredTextCopy(QImage(),{QRect(0,0,2,2)}).isNull());
        for(const auto &r:{QRect(),QRect(-1,0,2,2),QRect(19,19,2,2),QRect(0,0,-1,1),QRect(std::numeric_limits<int>::max()-4,0,3,3)})
            QVERIFY(coveredTextCopy(source,{r}).isNull());
        QList<QRect> many;for(int i=0;i<2001;++i)many.append(QRect(0,0,1,1));
        QVERIFY(coveredTextCopy(source,many).isNull());
        QImage wide(8193,1,QImage::Format_Mono);QVERIFY(coveredTextCopy(wide,{QRect(0,0,1,1)}).isNull());
        QVERIFY(!coveredTextCopy(source,{QRect(0,0,20,20)}).isNull());
        QCOMPARE(source.pixelColor(0,0),QColor(Qt::red));
    }
};
QTEST_GUILESS_MAIN(TextCoverTest)
#include "TextCoverTest.moc"
