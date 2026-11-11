// SPDX-License-Identifier: GPL-3.0-only
#include "RecordingPreparation.h"
#include <QtTest>
#include <QPointer>
#include <memory>
using NkShot::RecordingPreparation;
class RecordingPreparationTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void selectionDoesNotConsumeDelay() {
        RecordingPreparation job;QSignalSpy ready(&job,&RecordingPreparation::readyToStart);
        const auto token=job.begin(80);QVERIFY(token);QCOMPARE(job.phase(),RecordingPreparation::Selecting);
        QTest::qWait(100);QCOMPARE(ready.size(),0);QCOMPARE(job.remainingMilliseconds(),0);
        QElapsedTimer clock;clock.start();QVERIFY(job.targetReady(token));QCOMPARE(job.phase(),RecordingPreparation::Countdown);
        QVERIFY(job.remainingMilliseconds()>0);QTRY_COMPARE_WITH_TIMEOUT(ready.size(),1,1000);QVERIFY(clock.elapsed()>=80);
        QCOMPARE(ready.first().first().toULongLong(),token);QCOMPARE(job.phase(),RecordingPreparation::Starting);
        QVERIFY(job.markRecording(token));QCOMPARE(job.phase(),RecordingPreparation::Recording);QVERIFY(!job.cancel());
        QVERIFY(job.finish(token));QCOMPARE(job.phase(),RecordingPreparation::Idle);QVERIFY(!job.isCurrent(token));
    }
    void delayBoundsAndDuplicateStart() {
        RecordingPreparation job;QVERIFY(!job.begin(-1));QVERIFY(!job.begin(10001));
        for(int delay:{0,3000,5000,10000}) { const auto token=job.begin(delay);QVERIFY(token);QVERIFY(!job.begin(0));QVERIFY(!job.markRecording(token));QVERIFY(job.cancel());QVERIFY(!job.targetReady(token)); }
    }
    void zeroDelayAndQueuedCommitCancellation() {
        RecordingPreparation job;int commits=0;quint64 canceledAttempt=0;QSignalSpy ready(&job,&RecordingPreparation::readyToStart);
        connect(&job,&RecordingPreparation::readyToStart,&job,[&](quint64 token){QTimer::singleShot(0,&job,[&,token]{if(job.isCurrent(token)&&job.phase()==RecordingPreparation::Starting)++commits;});if(token==canceledAttempt)job.cancel();});
        const auto old=job.begin(0);canceledAttempt=old;QVERIFY(job.targetReady(old));QCOMPARE(ready.size(),0);QTRY_COMPARE(ready.size(),1);QCOMPARE(job.phase(),RecordingPreparation::Idle);
        const auto newer=job.begin(0);QVERIFY(newer!=old);QVERIFY(!job.targetReady(old));QVERIFY(!job.markRecording(old));QVERIFY(!job.finish(old));
        QCoreApplication::processEvents();QCOMPARE(commits,0);QVERIFY(job.targetReady(newer));QTRY_COMPARE(commits,1);QVERIFY(job.markRecording(newer));QVERIFY(job.fail(newer));
    }
    void zeroDelayStillCancelableBeforeEventTurn() {
        RecordingPreparation job;QSignalSpy ready(&job,&RecordingPreparation::readyToStart);const auto token=job.begin(0);QVERIFY(job.targetReady(token));QCOMPARE(ready.size(),0);QVERIFY(job.cancel());QTest::qWait(30);QCOMPARE(ready.size(),0);
    }
    void countdownCancelAndLateCallbacks() {
        RecordingPreparation job;QSignalSpy ready(&job,&RecordingPreparation::readyToStart);const auto old=job.begin(100);QVERIFY(job.targetReady(old));QTest::qWait(30);QVERIFY(job.cancel());
        const auto newer=job.begin(30);QVERIFY(!job.targetReady(old));QVERIFY(!job.finish(old));QVERIFY(job.targetReady(newer));QTRY_COMPARE(ready.size(),1);QTest::qWait(130);QCOMPARE(ready.size(),1);QCOMPARE(ready.first().first().toULongLong(),newer);
        QVERIFY(!job.targetReady(newer));QVERIFY(job.cancel());
    }
    void lateEventDeliveryUsesDeadline() {
        RecordingPreparation job;QSignalSpy ready(&job,&RecordingPreparation::readyToStart);const auto token=job.begin(60);QVERIFY(job.targetReady(token));
        QTest::qSleep(120);QCOMPARE(ready.size(),0);QCOMPARE(job.remainingMilliseconds(),0);QTRY_COMPARE(ready.size(),1);QTest::qWait(100);QCOMPARE(ready.size(),1);
    }
    void reentrantCancel_data() { QTest::addColumn<int>("phase");for(int phase:{int(RecordingPreparation::Selecting),int(RecordingPreparation::Countdown),int(RecordingPreparation::Starting)})QTest::newRow(qPrintable(QString::number(phase)))<<phase; }
    void reentrantCancel() {
        QFETCH(int,phase);RecordingPreparation job;QSignalSpy ready(&job,&RecordingPreparation::readyToStart);
        connect(&job,&RecordingPreparation::changed,&job,[&]{if(int(job.phase())==phase)job.cancel();});
        const auto token=job.begin(0);if(job.isCurrent(token))job.targetReady(token);QTRY_COMPARE(job.phase(),RecordingPreparation::Idle);QCOMPARE(ready.size(),0);QVERIFY(!job.isCurrent(token));
    }
    void reentrantDestruction_data() { reentrantCancel_data(); }
    void reentrantDestruction() {
        QFETCH(int,phase);auto job=std::make_unique<RecordingPreparation>();int ready=0;
        connect(job.get(),&RecordingPreparation::readyToStart,this,[&](quint64){++ready;});
        connect(job.get(),&RecordingPreparation::changed,this,[&]{if(job&&int(job->phase())==phase)job.reset();});
        const auto token=job->begin(0);if(job)job->targetReady(token);QTRY_VERIFY(!job);QCOMPARE(ready,0);QTest::qWait(50);
    }
    void destructionDuringCountdown() {
        auto job=std::make_unique<RecordingPreparation>();int ready=0;connect(job.get(),&RecordingPreparation::readyToStart,this,[&](quint64){++ready;});const auto token=job->begin(40);job->targetReady(token);job.reset();QTest::qWait(100);QCOMPARE(ready,0);
    }
    void readySignalCanCancelOrDestroy() {
        auto job=std::make_unique<RecordingPreparation>();connect(job.get(),&RecordingPreparation::readyToStart,this,[&](quint64){job.reset();});const auto token=job->begin(0);job->targetReady(token);QTRY_VERIFY(!job);
        RecordingPreparation second;connect(&second,&RecordingPreparation::readyToStart,&second,[&](quint64){second.cancel();});const auto next=second.begin(0);second.targetReady(next);QTRY_COMPARE(second.phase(),RecordingPreparation::Idle);
    }
};
QTEST_GUILESS_MAIN(RecordingPreparationTest)
#include "RecordingPreparationTest.moc"
