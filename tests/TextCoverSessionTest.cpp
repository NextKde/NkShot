// SPDX-License-Identifier: GPL-3.0-only
#include "Gui/TextCoverSession.h"
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QPainter>
#include <QColorSpace>
#include <QThreadPool>
#include <QtTest>
class TextCoverSessionTest : public QObject {
    Q_OBJECT
    QImage fixture() {
        QImage image(1200,420,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::white);
        QPainter painter(&image);QFont font(QStringLiteral("DejaVu Sans"));font.setPixelSize(58);painter.setFont(font);painter.setPen(Qt::black);
        painter.drawText(QPoint(50,120),QStringLiteral("SECRET ALPHA"));painter.drawText(QPoint(50,250),QStringLiteral("SECRET BETA"));
        painter.end();image.setColorSpace(QColorSpace::SRgb);image.setDevicePixelRatio(2);return image;
    }
    QImage lookup(QQmlEngine &engine,const QUrl &url) {
        auto provider=dynamic_cast<QQuickImageProvider *>(engine.imageProvider(url.host()));
        return provider?provider->requestImage(url.path().mid(1),nullptr,QSize()):QImage();
    }
    bool finish(TextCoverSession &session) {return QTest::qWaitFor([&]{return !session.busy();},20000);}
private Q_SLOTS:
    void initTestCase() {
        const auto result=NkShot::Imaging::extractLocalText(fixture(),{QStringLiteral("eng")},{});
        if(result.error==NkShot::Imaging::TextExtractionError::Unavailable||result.error==NkShot::Imaging::TextExtractionError::InvalidLanguage)QSKIP("Runtime English Tesseract data unavailable");
        QCOMPARE(result.error,NkShot::Imaging::TextExtractionError::None);QVERIFY(result.wordsComplete);
    }
    void immutableRecognitionSearchAndSelection() {
        QQmlEngine engine;TextCoverSession session(&engine);auto image=fixture();const auto original=image.copy();
        QVERIFY(session.begin(image,{QStringLiteral("eng")}));QVERIFY(session.busy());QVERIFY(!session.ready());image.fill(Qt::red);
        QCOMPARE(session.source(),original);QCOMPARE(session.sourceSize(),original.size());
        const auto imageUrl=session.imageSource();const auto preview=lookup(engine,imageUrl);
        QCOMPARE(preview.width(),1024);QCOMPARE(preview.devicePixelRatio(),1.);QCOMPARE(preview.colorSpace(),QColorSpace(QColorSpace::SRgb));QVERIFY(!preview.hasAlphaChannel());
        QVERIFY(!session.begin(fixture(),{QStringLiteral("eng")}));
        QVERIFY(finish(session));QVERIFY2(session.ready(),qPrintable(session.error()));
        session.search(QStringLiteral("SECRET"));QCOMPARE(session.matchCount(),2);QCOMPARE(session.selectedCount(),2);QVERIFY(!session.selectedRegions().isEmpty());
        for(const auto &rect:session.selectedRegions())QVERIFY(QRect(QPoint(),original.size()).contains(rect));
        QSignalSpy candidatesChanged(&session,&TextCoverSession::candidatesChanged);
        const auto candidates=session.candidates();
        QVERIFY(!candidates.first().toMap().contains(QStringLiteral("selected")));
        session.setSelected(0,false);QCOMPARE(session.selectedCount(),1);QCOMPARE(session.matches().at(0).toMap().value(QStringLiteral("selected")).toBool(),false);
        QVERIFY(!session.isSelected(0));QCOMPARE(session.candidates(),candidates);QCOMPARE(candidatesChanged.count(),0);
        session.setSelected(0,true);QCOMPARE(session.selectedCount(),2);QVERIFY(session.isSelected(0));QCOMPARE(candidatesChanged.count(),0);
        session.setSelected(-1,false);session.setSelected(100,false);QCOMPARE(session.selectedCount(),2);
        session.clearMatches();QCOMPARE(candidatesChanged.count(),1);QCOMPARE(session.matchCount(),0);QVERIFY(session.selectedRegions().isEmpty());QVERIFY(session.ready());
        session.search(QStringLiteral("NOTINTHISPICTURE"));QCOMPARE(session.matchCount(),0);QVERIFY(!session.error().isEmpty());
        session.search(QString(257,QLatin1Char('A')));QCOMPARE(session.matchCount(),0);QVERIFY(!session.error().isEmpty());
        session.search(QStringLiteral("E"));QVERIFY(session.matchCount()>=4);QVERIFY(session.selectedRegions().size()<session.matchCount());
        session.search(QStringLiteral("SECRET"));QCOMPARE(session.selectedCount(),2);QCOMPARE(session.source(),original);
        const auto accepted=session.source();const auto regions=session.selectedRegions();session.clear();
        QVERIFY(!session.ready());QVERIFY(session.source().isNull());QVERIFY(session.imageSource().isEmpty());QVERIFY(lookup(engine,imageUrl).isNull());QVERIFY(session.selectedRegions().isEmpty());
        QCOMPARE(accepted,original);QVERIFY(!regions.isEmpty());
        QVERIFY(session.begin(original,{QStringLiteral("eng")}));QVERIFY(finish(session));QVERIFY(session.ready());QCOMPARE(session.matchCount(),0);
    }
    void cancelAndNoText() {
        QQmlEngine engine;TextCoverSession session(&engine);
        QVERIFY(session.begin(fixture(),{QStringLiteral("eng")}));session.clear();
        QVERIFY(session.sourceSize().isEmpty());QVERIFY(!session.ready());QVERIFY(!session.begin(fixture(),{QStringLiteral("eng")}));
        QVERIFY(finish(session));QVERIFY(!session.ready());QVERIFY(session.imageSource().isEmpty());QCOMPARE(session.matchCount(),0);
        QImage blank(300,150,QImage::Format_RGB32);blank.fill(Qt::white);
        QVERIFY(session.begin(blank,{QStringLiteral("eng")}));QVERIFY(finish(session));QVERIFY(!session.ready());QVERIFY(!session.error().isEmpty());
        session.search(QStringLiteral("SECRET"));QCOMPARE(session.matchCount(),0);
        QVERIFY(!session.begin(QImage(),{QStringLiteral("eng")}));
        QImage wide(8193,1,QImage::Format_RGB32);QVERIFY(!session.begin(wide,{QStringLiteral("eng")}));QVERIFY(!session.ready());
    }
    void destructionAndObservers() {
        auto engine=new QQmlEngine;auto session=new TextCoverSession(engine);
        QVERIFY(session->begin(fixture(),{QStringLiteral("eng")}));delete engine;session->clear();QVERIFY(finish(*session));delete session;
        QQmlEngine other;
        auto pending=new TextCoverSession(&other);QVERIFY(pending->begin(fixture(),{QStringLiteral("eng")}));delete pending;
        QThreadPool::globalInstance()->waitForDone();QCoreApplication::processEvents();
        auto duringClear=new TextCoverSession(&other);QPointer<TextCoverSession> guard(duringClear);
        connect(duringClear,&TextCoverSession::changed,duringClear,[duringClear]{delete duringClear;});
        QVERIFY(!duringClear->begin(fixture(),{QStringLiteral("eng")}));QVERIFY(!guard);
        auto duringCandidates=new TextCoverSession(&other);guard=duringCandidates;
        connect(duringCandidates,&TextCoverSession::candidatesChanged,duringCandidates,[duringCandidates]{delete duringCandidates;});
        QVERIFY(!duringCandidates->begin(fixture(),{QStringLiteral("eng")}));QVERIFY(!guard);
        auto duringBusy=new TextCoverSession(&other);guard=duringBusy;
        connect(duringBusy,&TextCoverSession::changed,duringBusy,[duringBusy]{if(duringBusy->busy())delete duringBusy;});
        duringBusy->begin(fixture(),{QStringLiteral("eng")});QVERIFY(!guard);
        QThreadPool::globalInstance()->waitForDone();QCoreApplication::processEvents();
    }
};
QTEST_MAIN(TextCoverSessionTest)
#include "TextCoverSessionTest.moc"
