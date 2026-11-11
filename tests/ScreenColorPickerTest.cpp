// SPDX-License-Identifier: GPL-3.0-only
#include "Platforms/ScreenColorPicker.h"
#include <QtTest>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusVirtualObject>
#include <QDBusArgument>
#include <QDBusObjectPath>
#include <QProcess>
#include <QTimer>
#include <limits>
#include <memory>
#include <cstdio>
namespace {
const QString service = QStringLiteral("org.freedesktop.portal.Desktop");
const QString path = QStringLiteral("/org/freedesktop/portal/desktop");
class FakePortal : public QDBusVirtualObject {
public:
    QString mode;
    uint pickCount = 0, closeCount = 0;
    QString introspect(const QString &) const override { return {}; }
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &bus) override {
        if (message.interface() == QStringLiteral("org.nkshot.TestPortal.Control")) {
            if (message.member() == QStringLiteral("SetMode")) { mode = message.arguments().value(0).toString(); bus.send(message.createReply()); return true; }
            if (message.member() == QStringLiteral("PickCount")) { bus.send(message.createReply(pickCount)); return true; }
            if (message.member() == QStringLiteral("CloseCount")) { bus.send(message.createReply(closeCount)); return true; }
        }
        if (message.member() == QStringLiteral("Close")) { ++closeCount; std::printf("CLOSE %s\n", qPrintable(message.path())); std::fflush(stdout); bus.send(message.createReply()); return true; }
        if (message.member() != QStringLiteral("PickColor") || mode == QStringLiteral("missingmethod")) { bus.send(message.createErrorReply(QDBusError::UnknownMethod, QStringLiteral("unsupported"))); return true; }
        ++pickCount; std::puts("PICK"); std::fflush(stdout);
        const QString requestMode = mode;
        const auto options = qdbus_cast<QVariantMap>(message.arguments().at(1));
        QString sender = message.service().mid(1); sender.replace(QLatin1Char('.'), QLatin1Char('_'));
        QString requestPath = path + QStringLiteral("/request/") + sender + QLatin1Char('/') + options.value(QStringLiteral("handle_token")).toString();
        if (requestMode == QStringLiteral("different") || requestMode == QStringLiteral("delayed")) requestPath += QStringLiteral("_actual");
        auto sendResponse = [=] {
            QVariantMap values; QDBusArgument color;
            if (requestMode == QStringLiteral("malformed")) { color.beginStructure(); color << 1.0 << 0.0; color.endStructure(); }
            else { color.beginStructure(); color << (requestMode == QStringLiteral("nan") ? std::numeric_limits<double>::quiet_NaN() : (requestMode == QStringLiteral("range") ? 1.01 : requestMode == QStringLiteral("ui") ? 0.2 : 1.0)) << (requestMode == QStringLiteral("ui") ? 0.4 : 0.0) << (requestMode == QStringLiteral("ui") ? 0.6 : 0.5); color.endStructure(); }
            values.insert(QStringLiteral("color"), QVariant::fromValue(color));
            auto signal = QDBusMessage::createSignal(requestPath, QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"));
            signal.setArguments({uint(requestMode == QStringLiteral("cancel") ? 1 : requestMode == QStringLiteral("failure") ? 2 : 0), values}); bus.send(signal);
        };
        if (requestMode == QStringLiteral("early")) sendResponse();
        const auto methodReply = message.createReply(QVariant::fromValue(QDBusObjectPath(requestMode == QStringLiteral("wrongpath") ? path : requestPath)));
        if (requestMode == QStringLiteral("delayed")) { QTimer::singleShot(800, this, [bus, methodReply]{bus.send(methodReply);}); return true; }
        bus.send(methodReply);
        if (requestMode == QStringLiteral("exit")) QTimer::singleShot(10, qApp, &QCoreApplication::quit);
        else if (requestMode != QStringLiteral("timeout") && requestMode != QStringLiteral("hold") && requestMode != QStringLiteral("early")) QTimer::singleShot(requestMode == QStringLiteral("late") ? 180 : requestMode == QStringLiteral("ui") ? 700 : 10, this, sendResponse);
        return true;
    }
};
}
class ScreenColorPickerTest : public QObject {
    Q_OBJECT
    std::unique_ptr<QProcess> portal;
    bool start(const QString &mode) {
        portal = std::make_unique<QProcess>();
        portal->start(QCoreApplication::applicationFilePath(), {QStringLiteral("--fake"), mode});
        return portal->waitForStarted() && QTest::qWaitFor([&]{ return QDBusConnection::sessionBus().interface()->isServiceRegistered(service).value(); }, 3000);
    }
private Q_SLOTS:
    void initTestCase() { QVERIFY2(qEnvironmentVariableIsSet("NKSHOT_TEST_PRIVATE_BUS"), "Set NKSHOT_TEST_PRIVATE_BUS only for a dedicated test bus"); QVERIFY2(QDBusConnection::sessionBus().isConnected(), "Run in a private dbus-run-session"); QVERIFY(!QDBusConnection::sessionBus().interface()->isServiceRegistered(service).value()); }
    void cleanup() { if (portal) { portal->terminate(); portal->waitForFinished(3000); portal.reset(); } QVERIFY(QTest::qWaitFor([&]{return !QDBusConnection::sessionBus().interface()->isServiceRegistered(service).value();},3000)); }
    void responses_data() { QTest::addColumn<QString>("mode"); QTest::addColumn<QString>("status"); for (const auto &mode : {"success", "early", "different", "cancel", "failure", "malformed", "nan", "range", "wrongpath", "missingmethod", "exit"}) QTest::newRow(mode) << QString::fromLatin1(mode) << QString::fromLatin1((QByteArray(mode)=="success"||QByteArray(mode)=="early"||QByteArray(mode)=="different")?"success":QByteArray(mode)=="cancel"?"cancelled":"error"); }
    void responses() {
        QFETCH(QString, mode); QFETCH(QString, status); QVERIFY(start(mode)); ScreenColorPicker picker(1000, nullptr); picker.request({});
        QVERIFY(QTest::qWaitFor([&]{return !picker.busy();},3000)); QCOMPARE(picker.status(),status);
        QCOMPARE(picker.hasColor(),status==QStringLiteral("success"));
        if(picker.hasColor()){QCOMPARE(picker.hexValue(),QStringLiteral("#FF0080"));QCOMPARE(picker.rgbValue(),QStringLiteral("rgb(255, 0, 128)"));}
        else if(status==QStringLiteral("error")) QVERIFY(!picker.error().isEmpty());
    }
    void missingService() { ScreenColorPicker picker(1000,nullptr);picker.request({});QVERIFY(QTest::qWaitFor([&]{return !picker.busy();},3000));QCOMPARE(picker.status(),QStringLiteral("error"));QVERIFY(!picker.hasColor()); }
    void timeoutCloses() { QVERIFY(start(QStringLiteral("timeout")));ScreenColorPicker picker(1000,nullptr);picker.request({});QByteArray picked;QVERIFY(QTest::qWaitFor([&]{picked+=portal->readAllStandardOutput();return picked.contains("PICK");},900));QVERIFY(QTest::qWaitFor([&]{return !picker.busy();},2000));QVERIFY(picker.error().contains(QStringLiteral("timed out")));QByteArray traffic;QVERIFY(QTest::qWaitFor([&]{traffic+=portal->readAllStandardOutput();return traffic.contains("CLOSE");},1000)); }
    void cancelLateAndRepeat() {
        QVERIFY(start(QStringLiteral("late")));ScreenColorPicker picker(1000,nullptr);picker.request({});QByteArray picked;QVERIFY(QTest::qWaitFor([&]{picked+=portal->readAllStandardOutput();return picked.contains("PICK");},1000));QVERIFY(picker.busy());picker.cancel();QCOMPARE(picker.status(),QStringLiteral("cancelled"));
        QTest::qWait(220);QVERIFY(!picker.hasColor());picker.request({});QVERIFY(QTest::qWaitFor([&]{return !picker.busy();},2000));QVERIFY(picker.hasColor());
        picker.request({});QVERIFY(!picker.hasColor());picker.cancel();QTest::qWait(220);QVERIFY(!picker.hasColor());
    }
    void destructionCloses() { QVERIFY(start(QStringLiteral("timeout")));auto picker=std::make_unique<ScreenColorPicker>();picker->request({});QByteArray traffic;QVERIFY(QTest::qWaitFor([&]{traffic+=portal->readAllStandardOutput();return traffic.contains("PICK");},2000));QVERIFY(picker->busy());picker.reset();QVERIFY(QTest::qWaitFor([&]{traffic+=portal->readAllStandardOutput();return traffic.contains("CLOSE");},1000)); }
    void delayedMethodReplyCleanup_data() { QTest::addColumn<int>("operation");QTest::newRow("cancel")<<0;QTest::newRow("destruct")<<1;QTest::newRow("timeout")<<2; }
    void delayedMethodReplyCleanup() {
        QFETCH(int,operation);QVERIFY(start(QStringLiteral("delayed")));
        auto picker=std::make_unique<ScreenColorPicker>(operation==2?400:2000,nullptr);picker->request({});
        QByteArray traffic;QVERIFY(QTest::qWaitFor([&]{traffic+=portal->readAllStandardOutput();return traffic.contains("PICK");},1000));
        if(operation==0)picker->cancel();else if(operation==1)picker.reset();
        QByteArray closes;QVERIFY(QTest::qWaitFor([&]{closes+=portal->readAllStandardOutput();return closes.contains("_actual");},2000));
        if(picker){QVERIFY(!picker->busy());QVERIFY(!picker->hasColor());}
    }
    void successThenInvalidParent() {
        QVERIFY(start(QStringLiteral("success")));ScreenColorPicker picker;
        for(const auto &invalid : {QStringLiteral("invalid"),QString(4097,QLatin1Char('x'))}) {
            picker.request({});QVERIFY(QTest::qWaitFor([&]{return !picker.busy();},2000));QVERIFY(picker.hasColor());
            picker.request(invalid);QVERIFY(!picker.busy());QVERIFY(!picker.hasColor());QVERIFY(picker.hexValue().isEmpty());QVERIFY(picker.rgbValue().isEmpty());QCOMPARE(picker.status(),QStringLiteral("error"));
        }
    }
    void reentrantDestruction() {
        QVERIFY(start(QStringLiteral("success")));
        auto picker=std::make_unique<ScreenColorPicker>();
        connect(picker.get(),&ScreenColorPicker::changed,this,[&]{if(picker && picker->busy())picker.reset();});
        picker->request({});QVERIFY(!picker);QTest::qWait(100);
        auto count=QDBusMessage::createMethodCall(service,path,QStringLiteral("org.nkshot.TestPortal.Control"),QStringLiteral("PickCount"));
        const auto reply=QDBusConnection::sessionBus().call(count);
        QCOMPARE(reply.type(),QDBusMessage::ReplyMessage);QCOMPARE(reply.arguments().value(0).toUInt(),uint(0));
    }
    void reentrantCancel() { QVERIFY(start(QStringLiteral("success")));ScreenColorPicker picker;connect(&picker,&ScreenColorPicker::changed,&picker,[&]{if(picker.busy())picker.cancel();});picker.request({});QVERIFY(!picker.busy());QTest::qWait(100);QVERIFY(!picker.hasColor());QCOMPARE(picker.status(),QStringLiteral("cancelled")); }
};
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    if(app.arguments().contains(QStringLiteral("--fake"))) {
        FakePortal fake;fake.mode=app.arguments().last();auto bus=QDBusConnection::sessionBus();
        if(!bus.registerVirtualObject(path,&fake,QDBusConnection::SubPath)||!bus.registerService(service))return 2;
        return app.exec();
    }
    ScreenColorPickerTest test;return QTest::qExec(&test,argc,argv);
}
#include "ScreenColorPickerTest.moc"
