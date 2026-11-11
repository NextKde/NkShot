// SPDX-License-Identifier: GPL-3.0-only
#include "Platforms/StatusNotifierAvailability.h"
#include <QtTest>
#include <QDBusVirtualObject>
#include <QDBusConnectionInterface>
#include <QDBusVariant>
#include <QProcess>
#include <QTimer>
#include <memory>
namespace {
const QString service=QStringLiteral("org.kde.StatusNotifierWatcher");
const QString path=QStringLiteral("/StatusNotifierWatcher");
class FakeWatcher : public QDBusVirtualObject {
public:
    QString mode=QStringLiteral("true");
    int queries=0;
    QString introspect(const QString &) const override{return {};}
    bool handleMessage(const QDBusMessage &message,const QDBusConnection &connection) override {
        auto bus=connection;
        if(message.interface()==QStringLiteral("org.nkshot.TestWatcher")) {
            if(message.member()==QStringLiteral("Mode")){mode=message.arguments().first().toString();bus.send(message.createReply());return true;}
            if(message.member()==QStringLiteral("Queries")){bus.send(message.createReply(queries));return true;}
            if(message.member()==QStringLiteral("Drop")){bus.send(message.createReply());bus.unregisterService(service);return true;}
            if(message.member()==QStringLiteral("Notify")) {
                const auto kind=message.arguments().first().toString();auto signal=QDBusMessage::createSignal(path,kind==QStringLiteral("PropertiesChanged")?QStringLiteral("org.freedesktop.DBus.Properties"):service,kind);
                if(kind==QStringLiteral("PropertiesChanged"))signal.setArguments({service,QVariantMap{},QStringList{QStringLiteral("IsStatusNotifierHostRegistered")}});
                bus.send(message.createReply());bus.send(signal);return true;
            }
        }
        if(message.interface()==QStringLiteral("org.freedesktop.DBus.Properties")&&message.member()==QStringLiteral("Get")) {
            ++queries;
            if(message.arguments()!=QVariantList{service,QStringLiteral("IsStatusNotifierHostRegistered")}){bus.send(message.createErrorReply(QDBusError::InvalidArgs,QStringLiteral("unexpected property")));return true;}
            if(mode==QStringLiteral("error")){bus.send(message.createErrorReply(QDBusError::UnknownProperty,QStringLiteral("unsupported")));return true;}
            QVariant value=mode==QStringLiteral("wrongtype")?QVariant(QStringLiteral("true")):QVariant(mode!=QStringLiteral("false"));
            const auto reply=message.createReply(QVariant::fromValue(QDBusVariant(value)));
            if(mode==QStringLiteral("late"))QTimer::singleShot(300,this,[bus,reply]{bus.send(reply);});else bus.send(reply);
            return true;
        }
        bus.send(message.createErrorReply(QDBusError::UnknownMethod,QStringLiteral("unsupported")));return true;
    }
};
}
class StatusNotifierAvailabilityTest : public QObject {
    Q_OBJECT
    std::unique_ptr<QProcess> process;
    QDBusMessage control(const QString &method,const QVariantList &args={}) {
        auto message=QDBusMessage::createMethodCall(service,path,QStringLiteral("org.nkshot.TestWatcher"),method);message.setArguments(args);return QDBusConnection::sessionBus().call(message);
    }
    bool start(const QString &mode=QStringLiteral("true")) {
        process=std::make_unique<QProcess>();process->start(QCoreApplication::applicationFilePath(),{QStringLiteral("--fake"),mode});
        return process->waitForStarted()&&QTest::qWaitFor([]{return QDBusConnection::sessionBus().interface()->isServiceRegistered(service).value();},3000);
    }
private Q_SLOTS:
    void initTestCase(){QVERIFY2(qEnvironmentVariableIsSet("NKSHOT_TEST_PRIVATE_BUS"),"Only run on an explicitly private test bus");QVERIFY(QDBusConnection::sessionBus().isConnected());QVERIFY(!QDBusConnection::sessionBus().interface()->isServiceRegistered(service).value());}
    void cleanup(){if(process){process->terminate();process->waitForFinished(2000);process.reset();}QVERIFY(QTest::qWaitFor([]{return !QDBusConnection::sessionBus().interface()->isServiceRegistered(service).value();},1000));}
    void absentThenRegisteredHost(){NkShot::StatusNotifierAvailability state(QDBusConnection::sessionBus());QVERIFY(!state.hostAvailable());QVERIFY(start());QTRY_VERIFY(state.hostAvailable());}
    void property_data(){QTest::addColumn<QString>("mode");for(const char *mode:{"false","wrongtype","error"})QTest::newRow(mode)<<QString::fromLatin1(mode);}
    void property(){QFETCH(QString,mode);QVERIFY(start(mode));NkShot::StatusNotifierAvailability state;QTRY_VERIFY(control(QStringLiteral("Queries")).arguments().value(0).toInt()>0);QTest::qWait(50);QVERIFY(!state.hostAvailable());}
    void hostAndPropertySignals(){
        QVERIFY(start());NkShot::StatusNotifierAvailability state;QSignalSpy changed(&state,&NkShot::StatusNotifierAvailability::changed);QTRY_VERIFY(state.hostAvailable());
        control(QStringLiteral("Mode"),{QStringLiteral("false")});control(QStringLiteral("Notify"),{QStringLiteral("StatusNotifierHostUnregistered")});QTRY_VERIFY(!state.hostAvailable());
        control(QStringLiteral("Mode"),{QStringLiteral("true")});control(QStringLiteral("Notify"),{QStringLiteral("StatusNotifierHostRegistered")});QTRY_VERIFY(state.hostAvailable());
        control(QStringLiteral("Mode"),{QStringLiteral("false")});control(QStringLiteral("Notify"),{QStringLiteral("PropertiesChanged")});QTRY_VERIFY(!state.hostAvailable());QVERIFY(changed.size()>=4);
    }
    void ownerLossAndLateReply(){
        QVERIFY(start());NkShot::StatusNotifierAvailability state;QTRY_VERIFY(state.hostAvailable());const int before=control(QStringLiteral("Queries")).arguments().first().toInt();
        control(QStringLiteral("Mode"),{QStringLiteral("late")});control(QStringLiteral("Notify"),{QStringLiteral("PropertiesChanged")});QTRY_VERIFY(control(QStringLiteral("Queries")).arguments().first().toInt()>before);
        control(QStringLiteral("Drop"));QTRY_VERIFY(!state.hostAvailable());QTest::qWait(400);QVERIFY(!state.hostAvailable());
    }
    void obsoleteReplyCannotOverrideNewQuery(){
        QVERIFY(start());NkShot::StatusNotifierAvailability state;QTRY_VERIFY(state.hostAvailable());const int before=control(QStringLiteral("Queries")).arguments().first().toInt();
        control(QStringLiteral("Mode"),{QStringLiteral("late")});control(QStringLiteral("Notify"),{QStringLiteral("StatusNotifierHostRegistered")});QTRY_VERIFY(control(QStringLiteral("Queries")).arguments().first().toInt()>before);
        control(QStringLiteral("Mode"),{QStringLiteral("false")});control(QStringLiteral("Notify"),{QStringLiteral("PropertiesChanged")});QTRY_VERIFY(!state.hostAvailable());QTest::qWait(400);QVERIFY(!state.hostAvailable());
    }
    void destructionWithPendingReply(){QVERIFY(start(QStringLiteral("late")));auto state=std::make_unique<NkShot::StatusNotifierAvailability>();QTRY_VERIFY(control(QStringLiteral("Queries")).arguments().first().toInt()>0);state.reset();QTest::qWait(400);}
    void reentrantDestruction(){QVERIFY(start());auto state=std::make_unique<NkShot::StatusNotifierAvailability>();connect(state.get(),&NkShot::StatusNotifierAvailability::changed,this,[&]{if(state&&state->hostAvailable())state.reset();});QTRY_VERIFY(!state);}
};
int main(int argc,char **argv){QCoreApplication app(argc,argv);if(app.arguments().contains(QStringLiteral("--fake"))){FakeWatcher watcher;watcher.mode=app.arguments().last();auto bus=QDBusConnection::sessionBus();if(!bus.registerVirtualObject(path,&watcher)||!bus.registerService(service))return 2;return app.exec();}StatusNotifierAvailabilityTest test;return QTest::qExec(&test,argc,argv);}
#include "StatusNotifierAvailabilityTest.moc"
