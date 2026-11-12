// SPDX-License-Identifier: GPL-3.0-only
#include <QtTest>
#include <QColorSpace>
#include <QPromise>
#include <QThread>
#include "Imaging/ScrollCaptureSession.h"
#include "Imaging/ScrollFrameCrop.h"
using namespace NkShot::Imaging;
class ScrollCaptureSessionTest : public QObject {
    Q_OBJECT
    static QImage page() {
        QImage image(40, 240, QImage::Format_RGBA8888);
        for (int y=0;y<image.height();++y) for (int x=0;x<image.width();++x)
            image.setPixelColor(x,y,QColor((x*7+y)%256,(y*13+x)%256,(x+y*3)%256));
        return image;
    }
    static ScrollCaptureSession::Region region() { return {QRect(-120, 30, 40, 100), QStringLiteral("display-A"), 1}; }
    static QImage transpose(const QImage &input) {
        QImage out(input.height(),input.width(),QImage::Format_RGBA8888);
        for(int y=0;y<input.height();++y)for(int x=0;x<input.width();++x)out.setPixelColor(y,x,input.pixelColor(x,y));
        out.setDevicePixelRatio(input.devicePixelRatio());out.setColorSpace(input.colorSpace());return out;
    }
private Q_SLOTS:
    void horizontalFrameCap() {
        const auto source=transpose(page());const ScrollCaptureSession::Region r{QRect(0,0,100,40),QStringLiteral("cap-output"),1};
        ScrollCaptureSession session;QVERIFY(session.begin(r,ScrollDirection::Horizontal));
        for(int i=0;i<16;++i){const auto token=session.requestFrame();QVERIFY(token);QVERIFY(session.acceptFrame(*token,r,source.copy(i*5,0,100,40)).accepted());}
        QCOMPARE(session.frameCount(),16);QVERIFY(!session.requestFrame());QCOMPARE(session.finish(),source.copy(0,0,175,40));
    }
    void horizontalExactMetadataAndRetry() {
        auto source=transpose(page());source.setDevicePixelRatio(2);source.setColorSpace(QColorSpace::SRgb);
        // Alpha outside the shared overlap must survive the row-wise append.
        source.setPixelColor(219,5,QColor(30,70,90,88));const auto untouched=source.copy();
        const ScrollCaptureSession::Region r{QRect(12,-30,100,40),QStringLiteral("horizontal-output"),2};
        ScrollCaptureSession session;QVERIFY(session.begin(r,ScrollDirection::Horizontal));
        QCOMPARE(session.direction(),ScrollDirection::Horizontal);QVERIFY(!session.begin(r,ScrollDirection::Vertical));
        const auto first=source.copy(0,0,100,40);QVERIFY(session.acceptFrame(*session.requestFrame(),r,first).accepted());
        const auto rejected=session.acceptFrame(*session.requestFrame(),r,first);QCOMPARE(rejected.stitchError,StitchError::NoProgress);QCOMPARE(session.frameCount(),1);
        QVERIFY(session.acceptFrame(*session.requestFrame(),r,source.copy(60,0,100,40)).accepted());
        QVERIFY(session.acceptFrame(*session.requestFrame(),r,source.copy(120,0,100,40)).accepted());
        QCOMPARE(session.seams(),QVector<int>({100,160}));QCOMPARE(session.preview(),source.copy(0,0,220,40));
        const auto result=session.finish();QCOMPARE(result,source.copy(0,0,220,40));QCOMPARE(result.devicePixelRatio(),2);QCOMPARE(result.colorSpace(),source.colorSpace());QCOMPARE(source,untouched);
        QVERIFY(session.begin(r,ScrollDirection::Horizontal));const auto stale=*session.requestFrame();session.cancel();
        QVERIFY(session.begin(r,ScrollDirection::Horizontal));const auto current=*session.requestFrame();
        QCOMPARE(session.acceptFrame(stale,r,first).error,ScrollCaptureSession::Error::StaleRequest);
        QVERIFY(session.acceptFrame(current,r,first).accepted());session.cancel();
        ScrollCaptureSession limited(24,5000);QVERIFY(limited.begin(r,ScrollDirection::Horizontal));
        QVERIFY(limited.acceptFrame(*limited.requestFrame(),r,first).accepted());
        QCOMPARE(limited.acceptFrame(*limited.requestFrame(),r,source.copy(60,0,100,40)).stitchError,StitchError::TooLarge);
        QCOMPARE(limited.preview(),first);QCOMPARE(limited.frameCount(),1);
        auto invalid=r;invalid.physicalRect=QRect(0,0,8193,8);QVERIFY(!session.begin(invalid,ScrollDirection::Horizontal));
    }

    void contextBoundCompletionSafety() {
        QPromise<int> pending; pending.start(); bool invoked=false;
        auto context=new QObject;
        auto chained=pending.future().then(context,[&](int) { invoked=true; });
        delete context; pending.addResult(1); pending.finish();
        QTest::qWait(10); QVERIFY(!invoked);
        QPromise<int> live; live.start(); QObject owner;
        QThread *deliveredThread=nullptr;
        auto delivered=live.future().then(&owner,[&](int) { deliveredThread=QThread::currentThread(); });
        auto worker=QThread::create([&] { live.addResult(2); live.finish(); });
        worker->start(); QVERIFY(worker->wait(1000)); delete worker;
        QTRY_VERIFY(deliveredThread); QCOMPARE(deliveredThread,owner.thread());
    }
    void outputLocalCropValidation() {
        auto screen=page(); screen.setDevicePixelRatio(2); screen.setColorSpace(QColorSpace::SRgb);
        const QRect rect(3,20,30,100);
        auto crop=cropScrollFrame(screen,screen.size(),rect,2);
        QCOMPARE(crop,screen.copy(rect)); QCOMPARE(crop.devicePixelRatio(),2);
        QCOMPARE(crop.colorSpace(),screen.colorSpace());
        QVERIFY(cropScrollFrame(screen,QSize(41,240),rect,2).isNull());
        QVERIFY(cropScrollFrame(screen,screen.size(),rect,1).isNull());
        QVERIFY(cropScrollFrame(screen,screen.size(),QRect(-1,0,20,100),2).isNull());
        QVERIFY(cropScrollFrame(screen,screen.size(),QRect(30,0,20,100),2).isNull());
        QVERIFY(cropScrollFrame(screen,screen.size(),QRect(),2).isNull());
        screen.fill(Qt::red); QVERIFY(crop!=screen.copy(rect));
    }
    void exactFramesAndOwnership() {
        ScrollCaptureSession s;
        const auto original=page();
        QVERIFY(s.begin(region()));
        auto first=original.copy(0,0,40,100);
        const auto token=s.requestFrame(); QVERIFY(token);
        QVERIFY(!s.requestFrame()); QVERIFY(!s.begin(region())); QVERIFY(s.finish().isNull());
        QVERIFY(s.acceptFrame(*token,region(),first).accepted());
        first.fill(Qt::red); QCOMPARE(s.preview(),original.copy(0,0,40,100));
        QVERIFY(s.finish().isNull());
        QVERIFY(s.acceptFrame(*s.requestFrame(),region(),original.copy(0,60,40,100)).accepted());
        QVERIFY(s.acceptFrame(*s.requestFrame(),region(),original.copy(0,120,40,100)).accepted());
        QCOMPARE(s.frameCount(),3);
        auto result=s.finish(); QCOMPARE(result,original.copy(0,0,40,220));
        QCOMPARE(s.state(),ScrollCaptureSession::State::Finished); QCOMPARE(s.frameCount(),0);
        QVERIFY(!s.requestFrame()); s.cancel(); QVERIFY(!result.isNull());
        QCOMPARE(original,page());
    }
    void staleCallbacksCannotConsumeNewRequest() {
        ScrollCaptureSession s; QVERIFY(s.begin(region()));
        const auto old=*s.requestFrame(); s.cancel();
        QVERIFY(s.begin(region())); const auto current=*s.requestFrame(); QVERIFY(old!=current);
        QCOMPARE(s.acceptFrame(old,region(),page().copy(0,0,40,100)).error,ScrollCaptureSession::Error::StaleRequest);
        QCOMPARE(s.captureFailed(old).error,ScrollCaptureSession::Error::StaleRequest);
        QCOMPARE(s.state(),ScrollCaptureSession::State::AwaitingFrame);
        QCOMPARE(s.captureFailed(current).error,ScrollCaptureSession::Error::CaptureFailed);
        QCOMPARE(s.frameCount(),0); QVERIFY(s.requestFrame()); s.cancel();
        QVERIFY(s.preview().isNull());
    }
    void rejectedFramesPreserveProgress() {
        ScrollCaptureSession s; QVERIFY(s.begin(region()));
        const auto first=page().copy(0,0,40,100);
        QVERIFY(s.acceptFrame(*s.requestFrame(),region(),first).accepted());
        auto moved=region(); moved.physicalRect.translate(1,0);
        QCOMPARE(s.acceptFrame(*s.requestFrame(),moved,first).error,ScrollCaptureSession::Error::RegionChanged);
        auto output=region(); output.outputId=QStringLiteral("display-B");
        QCOMPARE(s.acceptFrame(*s.requestFrame(),output,first).error,ScrollCaptureSession::Error::RegionChanged);
        auto dpr=first; dpr.setDevicePixelRatio(2);
        QCOMPARE(s.acceptFrame(*s.requestFrame(),region(),dpr).error,ScrollCaptureSession::Error::InvalidFrame);
        QCOMPARE(s.acceptFrame(*s.requestFrame(),region(),{}).error,ScrollCaptureSession::Error::InvalidFrame);
        auto duplicate=s.acceptFrame(*s.requestFrame(),region(),first);
        QCOMPARE(duplicate.error,ScrollCaptureSession::Error::StitchFailed); QCOMPARE(duplicate.stitchError,StitchError::NoProgress);
        QImage mismatch=first; mismatch.fill(Qt::red);
        QCOMPARE(s.acceptFrame(*s.requestFrame(),region(),mismatch).stitchError,StitchError::NoOverlap);
        QCOMPARE(s.frameCount(),1); QCOMPARE(s.preview(),first);
        QVERIFY(s.acceptFrame(*s.requestFrame(),region(),page().copy(0,60,40,100)).accepted());
        QVERIFY(!s.finish().isNull());
    }
    void frameCapAndHighDpi() {
        ScrollCaptureSession s; auto r=region(); r.devicePixelRatio=2;
        QVERIFY(s.begin(r));
        for (int n=0;n<16;++n) {
            auto frame=page().copy(0,n,40,100); frame.setDevicePixelRatio(2);
            frame.setColorSpace(QColorSpace::SRgb);
            QVERIFY(s.acceptFrame(*s.requestFrame(),r,frame).accepted());
        }
        QCOMPARE(s.frameCount(),16); QVERIFY(!s.requestFrame());
        auto result=s.finish(); auto expected=page().copy(0,0,40,115);
        expected.setDevicePixelRatio(2); expected.setColorSpace(QColorSpace::SRgb);
        QCOMPARE(result,expected);
    }
    void profileAndAmbiguityRecovery() {
        ScrollCaptureSession s; QVERIFY(s.begin(region()));
        QImage first(40,100,QImage::Format_RGBA8888); first.fill(Qt::red);
        QVERIFY(s.acceptFrame(*s.requestFrame(),region(),first).accepted());
        auto changed=first; changed.setPixelColor(0,99,Qt::blue);
        QCOMPARE(s.acceptFrame(*s.requestFrame(),region(),changed).stitchError,StitchError::NoOverlap);
        changed.setColorSpace(QColorSpace::SRgb);
        QCOMPARE(s.acceptFrame(*s.requestFrame(),region(),changed).stitchError,StitchError::IncompatibleFrames);
        QCOMPARE(s.frameCount(),1); QCOMPARE(s.preview(),first);
        auto pending=*s.requestFrame(); s.cancel();
        QCOMPARE(s.acceptFrame(pending,region(),first).error,ScrollCaptureSession::Error::StaleRequest);
        QVERIFY(s.finish().isNull());
    }
    void limitsAndInvalidRegions() {
        ScrollCaptureSession s(24,5000); auto r=region();
        r.outputId.clear(); QVERIFY(!s.begin(r));
        r=region(); r.devicePixelRatio=0; QVERIFY(!s.begin(r));
        r=region(); r.physicalRect.setHeight(24); QVERIFY(!s.begin(r));
        QVERIFY(s.begin(region()));
        const auto first=page().copy(0,0,40,100);
        QVERIFY(s.acceptFrame(*s.requestFrame(),region(),first).accepted());
        QCOMPARE(s.acceptFrame(*s.requestFrame(),region(),page().copy(0,60,40,100)).stitchError,StitchError::TooLarge);
        QCOMPARE(s.preview(),first); QCOMPARE(s.frameCount(),1);
        s.cancel(); QVERIFY(s.begin(region())); QCOMPARE(s.frameCount(),0);
    }
};
QTEST_GUILESS_MAIN(ScrollCaptureSessionTest)
#include "ScrollCaptureSessionTest.moc"
