// SPDX-License-Identifier: GPL-3.0-only
#include "Platforms/RecordingStartGuard.h"
#include <QtTest>
#include <QTimer>
using NkShot::RecordingStartGuard;
class FakeStartRecorder : public QObject {
    Q_OBJECT
public:
    void start(bool immediateFailure) { if (immediateFailure) { Q_EMIT failed(); Q_EMIT idle(); } }
    void stop() { ++stopCalls; } // Does not imply producer quiescence.
    int stopCalls = 0;
Q_SIGNALS:
    void recording();
    void failed();
    void idle();
};
class RecordingStartGuardTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void cancelledProducerRetainsLeaseUntilIdle() {
        RecordingStartGuard guard;FakeStartRecorder recorder;int quiesces=0,retires=0;bool lease=true;
        const auto attempt=guard.markStartIssued();QVERIFY(attempt);
        connect(&recorder,&FakeStartRecorder::recording,this,[&]{QVERIFY(guard.recordingObserved(attempt));if(guard.cancellationRequested())recorder.stop();});
        connect(&recorder,&FakeStartRecorder::idle,this,[&]{if(guard.recorderIdleObserved(attempt)){++quiesces;lease=false;++retires;}});
        recorder.start(false);QVERIFY(guard.cancel(attempt));recorder.stop();QVERIFY(!guard.allowsNewAttempt());QVERIFY(!guard.markStartIssued());QVERIFY(lease);QCOMPARE(retires,0);
        Q_EMIT recorder.recording();QVERIFY(guard.recordingWasObserved());QVERIFY(!guard.allowsNewAttempt());QVERIFY(lease);QCOMPARE(retires,0);QCOMPARE(recorder.stopCalls,2);
        QTimer::singleShot(0,&recorder,&FakeStartRecorder::idle);QTRY_VERIFY(guard.allowsNewAttempt());QVERIFY(!lease);QCOMPARE(quiesces,1);QCOMPARE(retires,1);
        Q_EMIT recorder.idle();QCOMPARE(retires,1);
        const auto retry=guard.markStartIssued();QVERIFY(retry&&retry!=attempt);QVERIFY(!guard.recorderIdleObserved(attempt));QVERIFY(guard.outstanding());QVERIFY(guard.recorderIdleObserved(retry));
    }
    void synchronousErrorAndIdleInsideStart() {
        RecordingStartGuard guard;FakeStartRecorder recorder;int retires=0;const auto attempt=guard.markStartIssued();
        connect(&recorder,&FakeStartRecorder::failed,this,[&]{QVERIFY(guard.cancel(attempt));QVERIFY(!guard.allowsNewAttempt());QVERIFY(!guard.markStartIssued());QCOMPARE(retires,0);});
        connect(&recorder,&FakeStartRecorder::idle,this,[&]{if(guard.recorderIdleObserved(attempt))++retires;});
        recorder.start(true);QVERIFY(guard.allowsNewAttempt());QCOMPARE(retires,1);QVERIFY(!guard.cancel(attempt));QVERIFY(!guard.recordingObserved(attempt));QVERIFY(guard.markStartIssued());
    }
    void notIssuedCanRetryImmediately() {
        RecordingStartGuard guard;QVERIFY(!guard.cancel(0));QVERIFY(!guard.recorderIdleObserved(0));QVERIFY(guard.allowsNewAttempt());const auto attempt=guard.markStartIssued();QVERIFY(attempt);QVERIFY(guard.cancel(attempt));QVERIFY(guard.cancel(attempt));QVERIFY(!guard.allowsNewAttempt());
    }
    void oldCallbacksCannotUnlockNewProducer() {
        RecordingStartGuard guard;const auto old=guard.markStartIssued();QVERIFY(guard.recorderIdleObserved(old));const auto current=guard.markStartIssued();QVERIFY(!guard.cancel(old));QVERIFY(!guard.recordingObserved(old));QVERIFY(!guard.recorderIdleObserved(old));QVERIFY(!guard.allowsNewAttempt());QVERIFY(guard.recordingObserved(current));QVERIFY(!guard.allowsNewAttempt());QVERIFY(guard.recorderIdleObserved(current));
    }
};
QTEST_GUILESS_MAIN(RecordingStartGuardTest)
#include "RecordingStartGuardTest.moc"
