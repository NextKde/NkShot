// SPDX-License-Identifier: GPL-3.0-only
#include "Platforms/CaptureStreamLease.h"
#include <QtTest>
class TestCaptureStream : public QObject {
    Q_OBJECT
Q_SIGNALS:
    void created();
    void failed();
    void closed();
};
class CaptureStreamLeaseTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void queuedOldCallbacksCannotTouchNewAttempt() {
        QObject receiver;NkShot::CaptureStreamLease lease(&receiver);
        QPointer<TestCaptureStream> old=new TestCaptureStream;
        const auto first=lease.adopt(old);int starts=0,failures=0,stops=0;
        QObject::connect(old,&TestCaptureStream::created,&receiver,[&,raw=old.data(),first]{if(lease.accepts(raw,first)) ++starts;},Qt::QueuedConnection);
        QObject::connect(old,&TestCaptureStream::failed,&receiver,[&,raw=old.data(),first]{if(lease.accepts(raw,first)) ++failures;},Qt::QueuedConnection);
        QObject::connect(old,&TestCaptureStream::closed,&receiver,[&,raw=old.data(),first]{if(lease.accepts(raw,first)) ++stops;},Qt::QueuedConnection);
        Q_EMIT old->created();Q_EMIT old->failed();Q_EMIT old->closed();
        QPointer<TestCaptureStream> current=new TestCaptureStream;
        const auto second=lease.adopt(current);QVERIFY(second!=first);
        QObject::connect(current,&TestCaptureStream::created,&receiver,[&,raw=current.data(),second]{if(lease.accepts(raw,second)) ++starts;},Qt::QueuedConnection);
        Q_EMIT current->created();QCoreApplication::sendPostedEvents(&receiver,QEvent::MetaCall);
        QCOMPARE(starts,1);QCOMPARE(failures,0);QCOMPARE(stops,0);
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QVERIFY(old.isNull());
        Q_EMIT current->created();lease.retire();QCoreApplication::sendPostedEvents(&receiver,QEvent::MetaCall);
        QCOMPARE(starts,1);QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QVERIFY(current.isNull());
    }
    void quiescingSuppressesCallbacksButKeepsSourceAlive() {
        QObject receiver;NkShot::CaptureStreamLease lease(&receiver);
        QPointer<TestCaptureStream> stream=new TestCaptureStream;const auto token=lease.adopt(stream);int stops=0;
        QObject::connect(stream,&TestCaptureStream::closed,&receiver,[&,raw=stream.data(),token]{if(lease.accepts(raw,token))++stops;},Qt::QueuedConnection);
        Q_EMIT stream->closed();lease.quiesce();
        QCoreApplication::sendPostedEvents(&receiver,QEvent::MetaCall);
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        QCOMPARE(stops,0);QVERIFY(!stream.isNull());
        lease.retire();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QVERIFY(stream.isNull());
    }
    void repeatedAttemptsDoNotAccumulateCallbacks() {
        QObject receiver;NkShot::CaptureStreamLease lease(&receiver);int accepted=0;
        for(int i=0;i<20;++i) {
            auto stream=new TestCaptureStream;const auto token=lease.adopt(stream);
            QObject::connect(stream,&TestCaptureStream::created,&receiver,[&,stream,token]{if(lease.accepts(stream,token))++accepted;},Qt::QueuedConnection);
            Q_EMIT stream->created();QCoreApplication::sendPostedEvents(&receiver,QEvent::MetaCall);
            QCOMPARE(accepted,i+1);lease.retire();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        }
    }
};
QTEST_GUILESS_MAIN(CaptureStreamLeaseTest)
#include "CaptureStreamLeaseTest.moc"
