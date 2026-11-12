// SPDX-License-Identifier: GPL-3.0-only
#include "RecordingAutoStop.h"
#include <QtTest>
#include <QSignalSpy>
#include <QPointer>
using NkShot::RecordingAutoStop;
class RecordingAutoStopTest:public QObject {
    Q_OBJECT
private Q_SLOTS:
    void disabledAndValidation(){
        RecordingAutoStop p;QSignalSpy stop(&p,&RecordingAutoStop::stopRequested);
        for(int seconds:{0,-1,1,59,61,301,601}){p.begin(seconds,1);p.acknowledge(1,2);p.update(1,2,1000000,false);QCoreApplication::processEvents();QVERIFY(!p.enabled());QVERIFY(!p.active());QCOMPARE(p.remaining(),0);}
        p.begin(60,0);QVERIFY(!p.enabled());QCOMPARE(stop.size(),0);
        for(int seconds:{60,300,600}){p.begin(seconds,7);QVERIFY(p.enabled());QVERIFY(!p.active());QCOMPARE(p.remaining(),qint64(seconds)*1000);}
    }
    void acknowledgePauseResumeAndOneShot(){
        RecordingAutoStop p;QSignalSpy stop(&p,&RecordingAutoStop::stopRequested);p.begin(60,1);
        p.update(1,2,100000,false);QCoreApplication::processEvents();QCOMPARE(stop.size(),0);QCOMPARE(p.remaining(),60000);
        p.acknowledge(99,2);p.acknowledge(1,0);QVERIFY(!p.active());p.acknowledge(1,2);QVERIFY(p.active());
        p.update(1,2,30000,false);QCOMPARE(p.remaining(),30000);p.update(1,2,30000,true);
        QTest::qWait(20);QCOMPARE(p.remaining(),30000);QCOMPARE(stop.size(),0);QVERIFY(p.active());
        p.update(1,2,60000,true);QCoreApplication::processEvents();QCOMPARE(stop.size(),0);
        p.update(1,2,60000,false);QCOMPARE(stop.size(),0);QTRY_COMPARE(stop.size(),1);
        QCOMPARE(stop.at(0).at(0).toULongLong(),quint64(1));QCOMPARE(stop.at(0).at(1).toULongLong(),quint64(2));
        p.update(1,2,61000,false);p.acknowledge(1,2);QCoreApplication::processEvents();QCOMPARE(stop.size(),1);
    }
    void staleQueuedStopAndWrongGeneration(){
        RecordingAutoStop p;QSignalSpy stop(&p,&RecordingAutoStop::stopRequested);p.begin(60,1);p.acknowledge(1,2);p.update(1,2,60000,false);
        p.invalidate();QCoreApplication::processEvents();QCOMPARE(stop.size(),0);QVERIFY(!p.active());
        p.begin(60,3);p.acknowledge(3,4);p.update(3,4,60000,false);
        p.begin(60,5);p.acknowledge(5,6);QCoreApplication::processEvents();QCOMPARE(stop.size(),0);QCOMPARE(p.remaining(),60000);
        p.update(3,4,900000,false);p.update(5,4,900000,false);p.update(5,6,-1,false);QCOMPARE(p.remaining(),60000);
        p.update(5,6,60000,false);p.update(5,6,60000,true);QCoreApplication::processEvents();QCOMPARE(stop.size(),0);
        p.update(5,6,60000,false);QTRY_COMPARE(stop.size(),1);
    }
    void synchronousObserversCannotReviveOldSession(){
        RecordingAutoStop p;QSignalSpy stop(&p,&RecordingAutoStop::stopRequested);
        p.begin(60,1);p.acknowledge(1,2);bool replaced=false;
        auto connection=connect(&p,&RecordingAutoStop::changed,&p,[&]{if(!replaced){replaced=true;p.begin(300,3);p.acknowledge(3,4);}});
        p.update(1,2,60000,false);disconnect(connection);QCoreApplication::processEvents();QCOMPARE(stop.size(),0);QCOMPARE(p.remaining(),300000);
        replaced=false;connection=connect(&p,&RecordingAutoStop::changed,&p,[&]{if(!replaced){replaced=true;p.begin(600,5);}});
        p.begin(60,7);disconnect(connection);QCOMPARE(p.remaining(),600000);p.acknowledge(7,8);QVERIFY(!p.active());p.acknowledge(5,6);QVERIFY(p.active());
        QPointer<RecordingAutoStop> killed=new RecordingAutoStop;
        connect(killed,&RecordingAutoStop::changed,this,[&]{delete killed.data();});killed->begin(60,1);QVERIFY(!killed);
        killed=new RecordingAutoStop;killed->begin(60,1);killed->acknowledge(1,2);
        connect(killed,&RecordingAutoStop::changed,this,[&]{delete killed.data();});killed->update(1,2,60000,false);QVERIFY(!killed);QCoreApplication::processEvents();
    }
    void destructionCancelsQueuedWork(){
        int stops=0;auto p=new RecordingAutoStop;connect(p,&RecordingAutoStop::stopRequested,this,[&]{++stops;});
        p->begin(60,1);p->acknowledge(1,2);p->update(1,2,60000,false);delete p;QCoreApplication::processEvents();QCOMPARE(stops,0);
    }
};
QTEST_GUILESS_MAIN(RecordingAutoStopTest)
#include "RecordingAutoStopTest.moc"
