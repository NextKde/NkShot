// SPDX-License-Identifier: GPL-3.0-only
#include <QtTest>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QFile>
#include <QScopeGuard>
#include <cerrno>
#include <memory>
#include <PipeWireRecord>
#include <fcntl.h>
#include <unistd.h>
class RecordingInitializeFailureTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void failedContextReportsAndTerminates_data() {
        QTest::addColumn<int>("attempt");
        for(int i=0;i<6;++i) QTest::newRow(qPrintable(QString::number(i)))<<i;
    }
    void failedContextReportsAndTerminates() {
        QFETCH(int,attempt);const int stopMode=attempt%3;
        QTemporaryDir folder;QVERIFY(folder.isValid());
        // Isolated, deliberately absent config: no live server or capture needed.
        const bool hadDir=qEnvironmentVariableIsSet("PIPEWIRE_CONFIG_DIR"),hadName=qEnvironmentVariableIsSet("PIPEWIRE_CONFIG_NAME");
        const auto oldDir=qgetenv("PIPEWIRE_CONFIG_DIR");const auto oldName=qgetenv("PIPEWIRE_CONFIG_NAME");
        const auto restore=qScopeGuard([&]{
            if(hadDir) qputenv("PIPEWIRE_CONFIG_DIR",oldDir);else qunsetenv("PIPEWIRE_CONFIG_DIR");
            if(hadName) qputenv("PIPEWIRE_CONFIG_NAME",oldName);else qunsetenv("PIPEWIRE_CONFIG_NAME");
        });
        qputenv("PIPEWIRE_CONFIG_DIR",folder.path().toUtf8());qputenv("PIPEWIRE_CONFIG_NAME","nkshot-missing-context.conf");
        auto recorder=std::make_unique<PipeWireRecord>();
        const int fd=::open("/dev/null",O_RDONLY);QVERIFY(fd>=0);
        recorder->setFd(fd);recorder->setNodeId(1);
        const auto output=folder.filePath(QStringLiteral("recording.mp4"));
        const bool existing=attempt%2;
        if(existing) {QFile file(output);QVERIFY(file.open(QIODevice::WriteOnly));QCOMPARE(file.write("keep existing recording"),23);}
        recorder->setOutput(output);
        QSignalSpy errors(recorder.get(),&PipeWireRecord::errorFound);
        int completed=0;
        connect(recorder.get(),&PipeWireRecord::errorFound,this,[&]{if(stopMode==1) recorder->stop();});
        connect(recorder.get(),&PipeWireRecord::stateChanged,this,[&]{if(recorder->state()==PipeWireRecord::Idle) ++completed;});
        recorder->start();
        if(stopMode==2) recorder->stop();
        const bool settled=QTest::qWaitFor([&]{return completed>0;},3000);
        // An unpatched library fails this condition and its destructor may hang;
        // CTest's process timeout is the regression boundary, not a fake success.
        QVERIFY(settled);QCOMPARE(completed,1);QCOMPARE(errors.count(),1);
        QVERIFY(!errors.first().first().toString().isEmpty());QVERIFY(!recorder->isActive());
        if(existing) {QFile file(output);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),QByteArray("keep existing recording"));}
        else QVERIFY(!QFileInfo::exists(output));
        // Thread-finished cleanup owns the descriptor and must actually run.
        QVERIFY(QTest::qWaitFor([&]{errno=0;return ::fcntl(fd,F_GETFD)==-1 && errno==EBADF;},1000));
        disconnect(recorder.get(),nullptr,this,nullptr);
        QElapsedTimer elapsed;elapsed.start();recorder.reset();
        QVERIFY2(elapsed.elapsed()<2000,"Producer shutdown did not finish promptly");
    }
};
QTEST_MAIN(RecordingInitializeFailureTest)
#include "RecordingInitializeFailureTest.moc"
