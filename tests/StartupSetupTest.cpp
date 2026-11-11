// SPDX-License-Identifier: GPL-3.0-only
// Synthetic capabilities/settings, actual production QML controls. No capture backend.
#include <QtTest>
#include <QQuickView>
#include <QQuickItem>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QtQml/QQmlExtensionPlugin>
#include <memory>
#include <QScopeGuard>
#include <KLocalizedQmlContext>
#include <KLocalizedString>
Q_IMPORT_QML_PLUGIN(Kos_UiPlugin)
class StartupSetupTest : public QObject {
    Q_OBJECT
    std::unique_ptr<QQuickView> view;
    std::unique_ptr<QObject> settings;
    QVariantList captures() const {
        QVariantList list;
        const QStringList names{QStringLiteral("Rectangular Region"),QStringLiteral("All Screens"),QStringLiteral("All Screens (Scaled)"),QStringLiteral("Current Screen"),QStringLiteral("Active Window"),QStringLiteral("Window Under Cursor"),QStringLiteral("Full Screen")};
        for(int i=0;i<names.size();++i)list.append(QVariantMap{{QStringLiteral("display"),names.at(i)},{QStringLiteral("captureMode"),i}});
        return list;
    }
    QVariantList recordings() const {
        return {QVariantMap{{QStringLiteral("display"),QStringLiteral("Screen")},{QStringLiteral("recordingMode"),1}},QVariantMap{{QStringLiteral("display"),QStringLiteral("Window")},{QStringLiteral("recordingMode"),2}},QVariantMap{{QStringLiteral("display"),QStringLiteral("Rectangular Region")},{QStringLiteral("recordingMode"),4}}};
    }
    QObject *root() const { return view->rootObject(); }
    QQuickItem *item(const char *name) const { return root()->findChild<QQuickItem *>(QString::fromLatin1(name)); }
    void reveal(QQuickItem *control) {
        for(auto ancestor=control->parentItem();ancestor;ancestor=ancestor->parentItem()) {
            if(ancestor->property("contentY").isValid() && ancestor->property("contentHeight").isValid()) {
                const auto center=control->mapToItem(ancestor,QPointF(control->width()/2,control->height()/2));
                const qreal maxY=qMax(0.,ancestor->property("contentHeight").toDouble()-ancestor->height());
                ancestor->setProperty("contentY",qBound(0.,ancestor->property("contentY").toDouble()+center.y()-ancestor->height()/2,maxY));
            }
        }
        QCoreApplication::processEvents();
    }
    void click(const char *name) {
        QTest::qWait(30);auto control=item(name);QVERIFY2(control,name);reveal(control);QVERIFY(control->isVisible());QVERIFY(control->isEnabled());
        QTest::mouseClick(view.get(),Qt::LeftButton,Qt::NoModifier,control->mapToScene(QPointF(control->width()/2,control->height()/2)).toPoint());QCoreApplication::processEvents();
    }
    void select(const char *name,int index) {
        click(name);auto control=item(name);auto popup=qvariant_cast<QObject *>(control->property("popup"));QVERIFY(popup);
        QTRY_VERIFY(popup->property("visible").toBool());
        QTest::qWait(40);QTest::keyClick(view.get(),Qt::Key_Home);
        for(int i=0;i<index;++i)QTest::keyClick(view.get(),Qt::Key_Down);
        QTest::keyClick(view.get(),Qt::Key_Return);
        QTRY_VERIFY(!popup->property("visible").toBool());
        QTRY_COMPARE(control->property("currentIndex").toInt(),index);
    }
    void expand() { auto advanced=item("nkshotSetupAdvanced");if(advanced&&!advanced->property("checked").toBool())click("nkshotSetupAdvanced"); }
private Q_SLOTS:
    void initTestCase() { KLocalizedString::setApplicationDomain(QByteArrayLiteral("nkshot")); }
    void init() {
        view=std::make_unique<QQuickView>();view->engine()->rootContext()->setContextObject(new KLocalizedQmlContext(view->engine()));
        QQmlComponent config(view->engine());config.setData(R"QML(import QtQml
QtObject {
 property bool includePointer: false
 property bool includeDecorations: true
 property bool includeShadow: true
 property bool transientOnly: false
 property bool quitAfterSaveCopyExport: false
 property bool captureOnClick: false
 property real captureDelay: 0
 property bool videoIncludePointer: false
 property bool videoRecordSystemAudio: true
 property bool videoRecordMicrophone: true
 property int captureMode: 0
 property int lastUsedRecordingMode: 1
 property int recordingStartDelay: 0
})QML",QUrl());settings.reset(config.create());QVERIFY2(settings,qPrintable(config.errorString()));
        view->setResizeMode(QQuickView::SizeRootObjectToView);
        view->setInitialProperties({{QStringLiteral("settings"),QVariant::fromValue(settings.get())},{QStringLiteral("captureModes"),captures()},{QStringLiteral("recordingModes"),recordings()},{QStringLiteral("recordingAvailable"),true},{QStringLiteral("transientSupported"),true},{QStringLiteral("clickSupported"),true},{QStringLiteral("audioSupported"),true}});
        view->setSource(QUrl::fromLocalFile(QStringLiteral(STARTUP_PANE_PATH)));QVERIFY2(view->status()==QQuickView::Ready,qPrintable(view->errors().isEmpty()?QString():view->errors().first().toString()));
        view->resize(680,820);view->show();QVERIFY(QTest::qWaitForWindowExposed(view.get()));QTest::qWait(30);
    }
    void cleanup() { view.reset();settings.reset(); }
    void allTargetsAndRepeatDispatch() {
        QSignalSpy capture(root(),SIGNAL(captureRequested(int)));QSignalSpy record(root(),SIGNAL(recordRequested(int)));
        for(int mode=0;mode<7;++mode){select("nkshotCaptureTarget",mode);click("nkshotSetupPrimary");QCOMPARE(capture.size(),mode+1);QCOMPARE(capture.last().at(0).toInt(),mode);}
        click("nkshotSetupPrimary");QCOMPARE(capture.size(),8);QCOMPARE(capture.last().at(0).toInt(),6);
        click("nkshotSetupRecordingTab");QCOMPARE(root()->property("modeIndex").toInt(),1);
        const QList<int> modes{1,2,4};for(int i=0;i<3;++i){select("nkshotRecordingTarget",i);click("nkshotSetupPrimary");QCOMPARE(record.size(),i+1);QCOMPARE(record.last().at(0).toInt(),modes.at(i));}
        QCOMPARE(capture.size(),8);
    }
    void recordingStartDelayPreferences() {
        QCOMPARE(settings->property("recordingStartDelay").toInt(), 0);
        click("nkshotSetupRecordingTab");
        const QList<int> delays{0,3,5,10};
        for (int index = 0; index < delays.size(); ++index) {
            select("nkshotRecordingStartDelay", index);
            QCOMPARE(settings->property("recordingStartDelay").toInt(), delays.at(index));
            click("nkshotSetupScreenshotTab");
            click("nkshotSetupRecordingTab");
            QCOMPARE(item("nkshotRecordingStartDelay")->property("currentValue").toInt(), delays.at(index));
        }
        settings->setProperty("recordingStartDelay",3);
        QTRY_COMPARE(item("nkshotRecordingStartDelay")->property("currentValue").toInt(),3);
    }
    void dynamicAndMissingCapabilities() {
        QSignalSpy capture(root(),SIGNAL(captureRequested(int)));
        root()->setProperty("captureModes",QVariantList{captures().at(4)});QCoreApplication::processEvents();click("nkshotSetupPrimary");QCOMPARE(capture.last().at(0).toInt(),4);
        root()->setProperty("captureModes",QVariantList{captures().at(3)});QCoreApplication::processEvents();click("nkshotSetupPrimary");QCOMPARE(capture.size(),2);QCOMPARE(capture.last().at(0).toInt(),3);
        root()->setProperty("captureModes",QVariantList{});QCoreApplication::processEvents();QVERIFY(!item("nkshotSetupPrimary")->isEnabled());
        click("nkshotSetupRecordingTab");root()->setProperty("recordingModes",QVariantList{});QCoreApplication::processEvents();QVERIFY(!item("nkshotSetupPrimary")->isEnabled());
        root()->setProperty("recordingAvailable",false);QCoreApplication::processEvents();QVERIFY(!item("nkshotSetupPrimary")->isEnabled());
        root()->setProperty("captureModes",captures());QCoreApplication::processEvents();click("nkshotSetupScreenshotTab");QVERIFY(item("nkshotSetupPrimary")->isEnabled());
    }
    void countdownAcrossTabs() {
        QSignalSpy cancel(root(),SIGNAL(cancelRequested()));QSignalSpy capture(root(),SIGNAL(captureRequested(int)));QSignalSpy record(root(),SIGNAL(recordRequested(int)));
        root()->setProperty("captureTimeRemaining",2500);root()->setProperty("captureProgress",.4);QCoreApplication::processEvents();
        QVERIFY(!item("nkshotSetupPrimary")->isEnabled());QVERIFY(!item("nkshotSetupPrimary")->isVisible());click("nkshotSetupRecordingTab");QVERIFY(!item("nkshotSetupPrimary")->isEnabled());QVERIFY(!item("nkshotSetupPrimary")->isVisible());click("nkshotSetupCancel");QCOMPARE(cancel.size(),1);QCOMPARE(capture.size(),0);QCOMPARE(record.size(),0);
        root()->setProperty("captureTimeRemaining",0);root()->setProperty("captureProgress",0);QCoreApplication::processEvents();QVERIFY(item("nkshotSetupPrimary")->isEnabled());
    }
    void optionsDependenciesAndPreferences() {
        click("nkshotSetupPointer");QVERIFY(settings->property("includePointer").toBool());expand();
        click("nkshotSetupDecorations");QVERIFY(!settings->property("includeDecorations").toBool());QVERIFY(!item("nkshotSetupShadow")->isEnabled());QVERIFY(settings->property("includeShadow").toBool());
        click("nkshotSetupDecorations");click("nkshotSetupShadow");QVERIFY(!settings->property("includeShadow").toBool());
        click("nkshotSetupTransient");QVERIFY(settings->property("transientOnly").toBool());root()->setProperty("transientSupported",false);QCoreApplication::processEvents();QVERIFY(!item("nkshotSetupTransient")->isVisible());
        click("nkshotSetupQuit");QVERIFY(settings->property("quitAfterSaveCopyExport").toBool());
        click("nkshotSetupRecordingTab");click("nkshotSetupVideoPointer");QVERIFY(settings->property("videoIncludePointer").toBool());
        root()->setProperty("audioSupported",false);QCoreApplication::processEvents();QVERIFY(!item("nkshotSetupSystemAudio")->isEnabled());QVERIFY(!item("nkshotSetupMicrophone")->isEnabled());QVERIFY(settings->property("videoRecordSystemAudio").toBool());QVERIFY(settings->property("videoRecordMicrophone").toBool());
        root()->setProperty("audioSupported",true);QCoreApplication::processEvents();click("nkshotSetupSystemAudio");click("nkshotSetupMicrophone");QVERIFY(!settings->property("videoRecordSystemAudio").toBool());QVERIFY(!settings->property("videoRecordMicrophone").toBool());
    }
    void delayAndClickPolicy() {
        auto delay=item("nkshotSetupDelay");QVERIFY(delay);QVERIFY(delay->isEnabled());
        for(const auto &text:{QStringLiteral("0"),QStringLiteral("1.25"),QStringLiteral("99")}) {
            auto editor=qvariant_cast<QQuickItem *>(delay->property("contentItem"));QVERIFY(editor);reveal(editor);editor->forceActiveFocus();QTest::keyClick(view.get(),Qt::Key_A,Qt::ControlModifier);for(const QChar character:text)QTest::keyClick(view.get(),character.toLatin1());QTest::keyClick(view.get(),Qt::Key_Return);QCoreApplication::processEvents();QCOMPARE(settings->property("captureDelay").toDouble(),text.toDouble());
        }
        expand();click("nkshotSetupCaptureOnClick");QVERIFY(settings->property("captureOnClick").toBool());QVERIFY(!delay->isEnabled());
        root()->setProperty("clickSupported",false);root()->setProperty("onClickOnly",false);QCoreApplication::processEvents();QVERIFY(delay->isEnabled());QVERIFY(settings->property("captureOnClick").toBool());
        root()->setProperty("onClickOnly",true);QCoreApplication::processEvents();QVERIFY(!delay->isEnabled());
    }
    void externalSelectionAndCompactCountdown() {
        settings->setProperty("captureMode",5);QTRY_COMPARE(item("nkshotCaptureTarget")->property("currentValue").toInt(),5);
        expand();
        auto font=qvariant_cast<QFont>(root()->property("font"));font.setPointSize(20);root()->setProperty("font",font);
        view->resize(360,360);root()->setProperty("captureTimeRemaining",3000);root()->setProperty("captureProgress",.3);QTest::qWait(60);
        for(auto child:root()->findChildren<QQuickItem *>()) {
            if(child->isVisible()&&child->property("contentY").isValid())QVERIFY(child->height()>=0);
        }
        QVERIFY(!item("nkshotSetupPrimary")->isVisible());QVERIFY(!item("nkshotSetupPrimary")->isEnabled());
        for(const char *name:{"nkshotSetupTabs","nkshotSetupCancel"}) {
            auto control=item(name);QVERIFY(control);const auto top=control->mapToScene(QPointF());
            QVERIFY2(top.x()>=-1&&top.y()>=-1,name);QVERIFY2(top.x()+control->width()<=361,name);QVERIFY2(top.y()+control->height()<=361,name);
        }
        QSignalSpy cancel(root(),SIGNAL(cancelRequested()));click("nkshotSetupCancel");QCOMPARE(cancel.size(),1);
    }
    void rightToLeftControls() {
        const auto original=QGuiApplication::layoutDirection();
        const auto restore=qScopeGuard([&]{QGuiApplication::setLayoutDirection(original);});
        QGuiApplication::setLayoutDirection(Qt::RightToLeft);QTest::qWait(40);
        click("nkshotSetupPointer");QVERIFY(settings->property("includePointer").toBool());
        auto pointer=item("nkshotSetupPointer");pointer->forceActiveFocus();QTest::keyClick(view.get(),Qt::Key_Space);QVERIFY(!settings->property("includePointer").toBool());
        click("nkshotSetupRecordingTab");QCOMPARE(root()->property("modeIndex").toInt(),1);select("nkshotRecordingTarget",2);
        QSignalSpy record(root(),SIGNAL(recordRequested(int)));click("nkshotSetupPrimary");QCOMPARE(record.size(),1);QCOMPARE(record.first().first().toInt(),4);
    }
    void keyboardDisclosureAndNarrowLayout() {
        auto advanced=item("nkshotSetupAdvanced");QVERIFY(advanced);
        item("nkshotSetupPointer")->forceActiveFocus();
        for(int tab=0;tab<20&&!advanced->hasActiveFocus();++tab)QTest::keyClick(view.get(),Qt::Key_Tab);
        QVERIFY(advanced->hasActiveFocus());reveal(advanced);QTest::keyClick(view.get(),Qt::Key_Space);QCoreApplication::processEvents();QVERIFY(item("nkshotSetupDecorations")->isVisible());
        for(auto control:root()->findChildren<QQuickItem *>()) {
            if(control->property("font").isValid()) { auto font=qvariant_cast<QFont>(control->property("font"));font.setPixelSize(24);control->setProperty("font",font); }
        }
        view->resize(360,480);QCoreApplication::processEvents();QTest::qWait(30);
        for(const char *name:{"nkshotSetupPrimary","nkshotSetupAdvanced","nkshotSetupPointer","nkshotSetupDelay","nkshotSetupDecorations","nkshotSetupShadow","nkshotSetupQuit"}) {
            QTest::qWait(30);auto control=item(name);QVERIFY2(control,name);reveal(control);const QPointF p=control->mapToScene(QPointF());QVERIFY2(p.x()>=-1,name);QVERIFY2(p.x()+control->width()<=view->width()+1,name);QVERIFY2(control->height()>0,name);
        }
    }
};
QTEST_MAIN(StartupSetupTest)
#include "StartupSetupTest.moc"
