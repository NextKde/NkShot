// SPDX-License-Identifier: GPL-3.0-only
#include <QtTest>
#include <QSignalSpy>
#include "Imaging/ScrollCaptureController.h"
using namespace NkShot::Imaging;
class ScrollCaptureControllerTest : public QObject {
    Q_OBJECT
    static ScrollOutputLayout layout() { return {QStringLiteral("A"),QRectF(-200,0,200,160),QSize(200,160),1}; }
    static QRectF selection() { return {-180,10,40,100}; }
    static QImage frame(int offset) {
        QImage image(40,100,QImage::Format_RGBA8888);
        for(int y=0;y<100;++y) for(int x=0;x<40;++x) image.setPixelColor(x,y,QColor(x,y+offset,(x+y+offset)%256));
        return image;
    }
private Q_SLOTS:
    void fractionalCoordinateMapping() {
        for (qreal dpr : {1.,1.25,1.5,2.}) {
            auto l=layout(); l.dpr=dpr; l.nativePixels=QSize(qRound(200*dpr),qRound(160*dpr));
            QCOMPARE(scrollSelectionPixels(l,selection()),QRect(qRound(20*dpr),qRound(10*dpr),qRound(40*dpr),qRound(100*dpr)));
            QVERIFY(scrollSelectionPixels(l,QRectF(-210,0,50,100)).isNull());
            QVERIFY(scrollSelectionPixels(l,QRectF(-10,0,50,100)).isNull());
            l.nativePixels.rwidth()+=2; QVERIFY(scrollSelectionPixels(l,selection()).isNull());
        }
        auto l=layout(); l.dpr=0; QVERIFY(scrollSelectionPixels(l,selection()).isNull());
    }
    void hideBarrierAndResult() {
        QObject backend; int calls=0,cancels=0; quint64 current=0;
        ScrollCaptureController c(&backend,[&](quint64 token,QRect r){++calls;current=token;return r==QRect(20,10,40,100);},[&]{++cancels;});
        QSignalSpy hidden(&c,&ScrollCaptureController::hideWindowsRequested), restored(&c,&ScrollCaptureController::restoreWindowsRequested), results(&c,&ScrollCaptureController::resultReady);
        QVERIFY(c.begin(layout(),selection())); QVERIFY(c.captureNext()); QCOMPARE(calls,0);
        QVERIFY(!c.captureNext()); QVERIFY(!c.finish());
        auto ticket=hidden.last().at(0).toULongLong(); QVERIFY(!c.confirmHidden(ticket+1,layout()));
        QVERIFY(c.confirmHidden(ticket,layout())); QCOMPARE(calls,1);
        c.frameCaptured(ticket,layout(),frame(0)); QCOMPARE(c.frameCount(),1); QCOMPARE(restored.count(),1);
        QVERIFY(!c.finish()); QVERIFY(c.captureNext()); ticket=hidden.last().at(0).toULongLong();
        QVERIFY(c.confirmHidden(ticket,layout())); c.frameCaptured(ticket,layout(),frame(60));
        QVERIFY(c.finish()); QCOMPARE(results.count(),1); QCOMPARE(cancels,0);
        const auto result=qvariant_cast<QImage>(results.first().first()); QCOMPARE(result.size(),QSize(40,160));
        QCOMPARE(result.copy(0,0,40,100),frame(0)); QCOMPARE(result.copy(0,60,40,100),frame(60));
        c.frameCaptured(current,layout(),frame(0)); QCOMPARE(results.count(),1);
    }
    void cancelLateAndRestart() {
        QObject backend; int calls=0,cancels=0;
        ScrollCaptureController c(&backend,[&](quint64,QRect){++calls;return true;},[&]{++cancels;});
        QSignalSpy hidden(&c,&ScrollCaptureController::hideWindowsRequested), restored(&c,&ScrollCaptureController::restoreWindowsRequested);
        QVERIFY(c.begin(layout(),selection())); QVERIFY(c.captureNext()); auto old=hidden.last().first().toULongLong();
        c.cancel(); c.cancel(); QCOMPARE(restored.count(),1); QCOMPARE(calls,0);
        QVERIFY(!c.confirmHidden(old,layout())); QVERIFY(c.begin(layout(),selection())); QVERIFY(c.captureNext());
        auto ticket=hidden.last().first().toULongLong(); QVERIFY(ticket!=old); QVERIFY(c.confirmHidden(ticket,layout()));
        c.frameCaptured(old,layout(),frame(0)); QCOMPARE(c.frameCount(),0);
        c.cancel(); QCOMPARE(cancels,1); QCOMPARE(restored.count(),2);
        c.frameCaptured(ticket,layout(),frame(0)); QCOMPARE(c.frameCount(),0);
    }
    void retriesAndLayoutChanges() {
        QObject backend; bool canStart=false;
        ScrollCaptureController c(&backend,[&](quint64,QRect){return canStart;},[]{});
        QSignalSpy hidden(&c,&ScrollCaptureController::hideWindowsRequested), failed(&c,&ScrollCaptureController::failed);
        QVERIFY(c.begin(layout(),selection())); QVERIFY(c.captureNext()); auto token=hidden.last().first().toULongLong();
        QVERIFY(!c.confirmHidden(token,layout())); QCOMPARE(c.state(),ScrollCaptureController::State::Ready);
        canStart=true; QVERIFY(c.captureNext()); token=hidden.last().first().toULongLong();
        auto changed=layout(); changed.logicalGeometry.translate(1,0);
        QVERIFY(!c.confirmHidden(token,changed)); QCOMPARE(c.frameCount(),0);
        QVERIFY(c.captureNext()); token=hidden.last().first().toULongLong(); QVERIFY(c.confirmHidden(token,layout()));
        c.frameCaptured(token,layout(),frame(0));
        QVERIFY(c.captureNext()); token=hidden.last().first().toULongLong(); QVERIFY(c.confirmHidden(token,layout()));
        c.frameCaptured(token,changed,frame(60)); QCOMPARE(c.frameCount(),1); QCOMPARE(c.preview(),frame(0));
        QVERIFY(c.captureNext()); token=hidden.last().first().toULongLong(); QVERIFY(c.confirmHidden(token,layout()));
        c.frameCaptured(token,layout(),frame(0)); QCOMPARE(c.frameCount(),1);
        QVERIFY(failed.last().first().toString().contains(QStringLiteral("Scroll farther")));
    }
    void barrierTimeoutAndDestruction() {
        QObject backend; int calls=0,cancels=0,restores=0;
        auto c=new ScrollCaptureController(&backend,[&](quint64,QRect){++calls;return true;},[&]{++cancels;});
        QSignalSpy hidden(c,&ScrollCaptureController::hideWindowsRequested);
        connect(c,&ScrollCaptureController::restoreWindowsRequested,&backend,[&]{++restores;});
        QVERIFY(c->begin(layout(),selection())); QVERIFY(c->captureNext());
        QTRY_COMPARE_WITH_TIMEOUT(c->state(),ScrollCaptureController::State::Ready,5000);
        QCOMPARE(calls,0); QCOMPARE(cancels,0); QCOMPARE(restores,1);
        QVERIFY(c->captureNext()); auto token=hidden.last().first().toULongLong(); QVERIFY(c->confirmHidden(token,layout()));
        delete c; QCOMPARE(cancels,1); QCOMPARE(restores,2);
    }
    void captureTimeoutRestoresAndRejectsLateFrame() {
        QObject backend; int cancels=0;
        ScrollCaptureController c(&backend,[](quint64,QRect){return true;},[&]{++cancels;});
        QSignalSpy hidden(&c,&ScrollCaptureController::hideWindowsRequested), restored(&c,&ScrollCaptureController::restoreWindowsRequested);
        QVERIFY(c.begin(layout(),selection())); QVERIFY(c.captureNext());
        auto token=hidden.last().first().toULongLong(); QVERIFY(c.confirmHidden(token,layout()));
        c.frameCaptured(token,layout(),frame(0));
        QVERIFY(c.captureNext()); token=hidden.last().first().toULongLong(); QVERIFY(c.confirmHidden(token,layout()));
        QTRY_COMPARE_WITH_TIMEOUT(c.state(),ScrollCaptureController::State::Ready,10000);
        QCOMPARE(cancels,1); QCOMPARE(restored.count(),2); QCOMPARE(c.frameCount(),1);
        c.frameCaptured(token,layout(),frame(60)); QCOMPARE(c.preview(),frame(0));
    }
    void synchronousBackendFailure() {
        QObject backend; ScrollCaptureController *controller=nullptr;
        ScrollCaptureController c(&backend,[&](quint64 token,QRect){controller->frameFailed(token,QStringLiteral("denied"));return false;},[]{});
        controller=&c; QSignalSpy hidden(&c,&ScrollCaptureController::hideWindowsRequested), errors(&c,&ScrollCaptureController::failed);
        QVERIFY(c.begin(layout(),selection())); QVERIFY(c.captureNext());
        QVERIFY(!c.confirmHidden(hidden.last().first().toULongLong(),layout()));
        QCOMPARE(errors.count(),1); QCOMPARE(c.state(),ScrollCaptureController::State::Ready);
    }
    void reentrantCancellationAndBackendLoss() {
        auto backend=new QObject; int calls=0;
        ScrollCaptureController c(backend,[&](quint64,QRect){++calls;return true;},[]{});
        QSignalSpy restores(&c,&ScrollCaptureController::restoreWindowsRequested);
        auto connection=connect(&c,&ScrollCaptureController::stateChanged,&c,[&]{
            if(c.state()==ScrollCaptureController::State::WaitingForHidden) c.cancel();
        });
        QVERIFY(c.begin(layout(),selection())); QVERIFY(!c.captureNext()); QCOMPARE(calls,0); QCOMPARE(restores.count(),0);
        disconnect(connection); QVERIFY(c.begin(layout(),selection())); QVERIFY(c.captureNext());
        delete backend; QCOMPARE(c.state(),ScrollCaptureController::State::Cancelled); QCOMPARE(restores.count(),1);
        QVERIFY(!c.begin(layout(),selection()));
    }
};
QTEST_GUILESS_MAIN(ScrollCaptureControllerTest)
#include "ScrollCaptureControllerTest.moc"
