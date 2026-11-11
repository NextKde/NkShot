// SPDX-License-Identifier: GPL-3.0-only
#include "UiCapture.h"
#include "RecordingAdapterChecks.h"
#include "RecordingPreparationUiChecks.h"
#include "RecordingControlsUiChecks.h"
#include "LocalMediaOpenUiChecks.h"
#include "TranslationUiChecks.h"
#include "ScrollSessionUiChecks.h"
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QJsonDocument>
#include <QJsonObject>
#include "Gui/ViewerWindow.h"
#include "Gui/ScrollCaptureWindow.h"
#include "Platforms/ScrollFrameKWin.h"
#include "Platforms/ScreenColorPicker.h"
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QProcess>
#include "Gui/PinnedImageWindow.h"
#include "Gui/ImageCopyEditor.h"
#include "ImageExportUiChecks.h"
#include "MediaDropUiChecks.h"
#include "Gui/QuickNoteWindow.h"
#include "Gui/ColorPickerWindow.h"
#include "Imaging/ColorSample.h"
#include "Gui/OcrTextWindow.h"
#include "Gui/InlineMessageModel.h"
#include "Imaging/ImageTransforms.h"
#include "Gui/CaptureWindow.h"
#include "Gui/SelectionEditor.h"
#include "Gui/Selection.h"
#include <QScreen>
#include <memory>
#include <limits>
#include "Gui/TextContextMenu.h"
#include <QtTest/QTest>
#include <QtTest/QSignalSpy>
#include "SpectacleCore.h"
#include "CaptureModeModel.h"
#include "RecordingModeModel.h"
#include <KQuickImageEditor/AnnotationDocument>
#include <KQuickImageEditor/AnnotationTool>

#include <QApplication>
#include <QDebug>
#include <QImage>
#include <QFontInfo>
#include <QLineF>
#include <QInputMethodEvent>
#include <QClipboard>
#include <QScopeGuard>
#include <QThreadPool>
#include <QWheelEvent>
#include "settings.h"
#include <QMimeData>
#include <QFileInfo>
#include <QFile>
#include <QFileDialog>
#include <QTemporaryDir>
#include <QLineEdit>
#include <QIcon>
#include <QDir>
#include <QStandardPaths>
#include <QPointer>
#include <QPalette>
#include <QPainter>
#include <QQuickItem>
#include <QJSValue>
#include <QTimer>
#include <cstring>

static QQuickItem *visualItem(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name) return root;
    for (auto child : root->childItems()) {
        if (auto found = visualItem(child, name)) return found;
    }
    return nullptr;
}

static bool openTaskMenu(ViewerWindow *viewer, const QString &name)
{
    auto menu = viewer->rootObject()->findChild<QObject *>(QStringLiteral("nkshotMoreMenu"));
    auto task = viewer->rootObject()->findChild<QObject *>(name);
    if (!menu || !task || !QMetaObject::invokeMethod(menu, "open")) { qWarning() << "Task lookup" << menu << task; return false; }
    QTest::qWait(200);
    auto row = visualItem(viewer->contentItem(), QStringLiteral("nkshotTask_") + name);
    if (!row || !row->isVisible()) { qWarning() << "Task row" << row; return false; }
    QTest::mouseClick(viewer, Qt::LeftButton, Qt::NoModifier,
        row->mapToScene(QPointF(row->width()/2, row->height()/2)).toPoint());
    const bool opened = QTest::qWaitFor([&] { return task->property("opened").toBool(); }, 1000);
    qInfo() << "Task opened" << name << opened;
    QTest::qWait(200);
    viewer->grabWindow().save(qEnvironmentVariable("NKSHOT_TEST_UI_CAPTURE") + QStringLiteral(".task.png"));
    return opened;
}

static bool samePixels(const QImage &a, const QImage &b)
{
    if (a.isNull() || a.size() != b.size()) return false;
    const auto left = a.convertToFormat(QImage::Format_RGBA8888);
    const auto right = b.convertToFormat(QImage::Format_RGBA8888);
    for (int y = 0; y < left.height(); ++y) {
        if (std::memcmp(left.constScanLine(y), right.constScanLine(y), left.width() * 4) != 0) return false;
    }
    return true;
}



static void runStartupChecks(QApplication &application, ViewerWindow *viewer, const QString &destination)
{
    const auto fail = [&](const char *reason) { qCritical() << "Startup UI:" << reason; viewer->close(); application.exit(255); };
    auto core = SpectacleCore::instance();
    if (!core->annotationDocument()->baseImage().isNull() || core->videoMode()) { fail("requires launchonly empty state"); return; }
    auto pane = viewer->rootObject()->findChild<QObject *>(QStringLiteral("nkshotSetupPane"));
    if (!pane) { fail("setup pane missing"); return; }
    const auto control = [&](const char *name) { return visualItem(viewer->contentItem(), QString::fromLatin1(name)); };
    const auto click = [&](const char *name) {
        auto item = control(name); if (!item || !item->isVisible() || !item->isEnabled()) return false;
        for (auto ancestor=item->parentItem(); ancestor; ancestor=ancestor->parentItem()) {
            if (ancestor->property("contentY").isValid() && ancestor->property("contentHeight").isValid()) {
                const auto point=item->mapToItem(ancestor,QPointF(item->width()/2,item->height()/2));
                const auto maxY=qMax(0.,ancestor->property("contentHeight").toDouble()-ancestor->height());
                ancestor->setProperty("contentY",qBound(0.,ancestor->property("contentY").toDouble()+point.y()-ancestor->height()/2,maxY));
            }
        }
        QTest::qWait(50);
        QTest::mouseClick(viewer,Qt::LeftButton,Qt::NoModifier,item->mapToScene(QPointF(item->width()/2,item->height()/2)).toPoint());
        QTest::qWait(80); return true;
    };
    const auto capture = [&] (const QString &suffix) { QTest::qWait(200); return viewer->grabWindow().save(destination+suffix); };
    const auto heading = control("nkshotSetupAreaHeading");
    if (!heading) { fail("area heading");return; }
    qInfo() << "Startup resolved heading font" << heading->property("font").value<QFont>().family()
            << QFontInfo(heading->property("font").value<QFont>()).family()
            << "application" << QFontInfo(application.font()).family();
    const auto originalPointer=Settings::includePointer();
    const auto originalDecorations=Settings::includeDecorations();
    const auto originalShadow=Settings::includeShadow();
    const auto originalQuit=Settings::quitAfterSaveCopyExport();
    const auto originalDelay=Settings::captureDelay();
    const auto originalClick=Settings::captureOnClick();
    const auto originalMode=Settings::captureMode();
    const auto originalVideo=core->currentVideo();
    const auto originalMic=Settings::videoRecordMicrophone();
    application.clipboard()->setText(QStringLiteral("startup unchanged"));
    auto restore=qScopeGuard([&] {
        Settings::setIncludePointer(originalPointer);Settings::setIncludeDecorations(originalDecorations);
        Settings::setIncludeShadow(originalShadow);Settings::setQuitAfterSaveCopyExport(originalQuit);
        Settings::setCaptureDelay(originalDelay);Settings::setCaptureOnClick(originalClick);Settings::setCaptureMode(originalMode);Settings::setVideoRecordMicrophone(originalMic);
    });
    if (qEnvironmentVariableIsSet("NKSHOT_TEST_STARTUP_ERROR")) {
        InlineMessageModel::instance()->push(InlineMessageModel::Error, QStringLiteral("Synthetic layout check: no screen was captured."));
        QTest::qWait(500);
    }
    if (!capture(QString())) { fail("initial screenshot"); return; }
    auto target=control("nkshotCaptureTarget");
    const auto modelObject = [](const QVariant &value) {
        return value.canConvert<QJSValue>() ? value.value<QJSValue>().toQObject() : value.value<QObject *>();
    };
    auto captureModel=qobject_cast<CaptureModeModel *>(modelObject(pane->property("captureModes")));
    auto recordingModel=qobject_cast<RecordingModeModel *>(modelObject(pane->property("recordingModes")));
    if (!captureModel || !recordingModel) { fail("actual QML model instances");return; }
    if (!target || target->property("count").toInt()!=captureModel->rowCount()) { fail("full production target model");return; }
    if (target->property("count").toInt()>1) {
        if (!click("nkshotCaptureTarget")) { fail("target open");return; }
        QTest::keyClick(viewer,Qt::Key_End);QTest::keyClick(viewer,Qt::Key_Return);QTest::qWait(100);
        if (Settings::captureMode()!=target->property("currentValue").toInt()) { fail("selection persistence");return; }
    }
    if (!click("nkshotSetupPointer") || Settings::includePointer()==originalPointer) { fail("pointer toggle");return; }
    if (control("nkshotSetupDelay")->isEnabled()) {
        if (!click("nkshotSetupDelay")) { fail("delay reachable");return; }
        auto editor=control("nkshotSetupDelay")->property("contentItem").value<QQuickItem *>();
        if (!editor) { fail("delay editor");return; }
        editor->forceActiveFocus();QTest::keyClick(viewer,Qt::Key_A,Qt::ControlModifier);
        for(const char character: QByteArrayLiteral("1.25"))QTest::keyClick(viewer,character);
        QTest::keyClick(viewer,Qt::Key_Return);QTest::qWait(100);
        if (qAbs(Settings::captureDelay()-1.25)>0.001) {fail("decimal delay persisted");return;}
    }
    auto advanced=control("nkshotSetupAdvanced");
    if (!advanced) {fail("advanced disclosure");return;}
    advanced->forceActiveFocus();QTest::keyClick(viewer,Qt::Key_Space);QTest::qWait(100);
    const QSize stableSize=viewer->size();
    if (!control("nkshotSetupDecorations")->isVisible() || !click("nkshotSetupDecorations")) { fail("keyboard disclosure/options");return; }
    if (control("nkshotSetupShadow")->isEnabled()!=Settings::includeDecorations() || Settings::includeShadow()!=originalShadow) { fail("shadow dependency");return; }
    if (!click("nkshotSetupQuit") || Settings::quitAfterSaveCopyExport()==originalQuit || viewer->size()!=stableSize) { fail("quit preference/stable size");return; }
    capture(QStringLiteral(".options.png"));
    if (!click("nkshotSetupRecordingTab") || pane->property("modeIndex").toInt()!=1) {fail("recording tab");return;}
    const bool supported=core->videoPlatform()->supportedRecordingModes()!=VideoPlatform::NoRecordingModes;
    if (!supported && control("nkshotSetupPrimary")->isEnabled()) {fail("unsupported recording start");return;}
    auto microphone=control("nkshotSetupMicrophone");
    if (microphone && microphone->isEnabled()) {
        if (!click("nkshotSetupMicrophone") || Settings::videoRecordMicrophone()==originalMic) {fail("microphone preference reachable");return;}
    }
    qInfo()<<"Startup real platform recording available:"<<supported;
    capture(QStringLiteral(".recording.png"));
    if (!click("nkshotSetupScreenshotTab")) {fail("screenshot return");return;}
    // Exercise actual QAbstractItemModel notifications with same-count replacements.
    // This alters only UI models, never platform capabilities or capture backends.
    const auto oldGrab=core->imagePlatform()->supportedGrabModes();
    const auto oldRecording=core->videoPlatform()->supportedRecordingModes();
    auto restoreModels=qScopeGuard([&] { captureModel->setGrabModes(oldGrab);recordingModel->setRecordingModes(oldRecording); });
    QSignalSpy captureReset(captureModel,&QAbstractItemModel::modelReset);
    QSignalSpy recordingReset(recordingModel,&QAbstractItemModel::modelReset);
    captureModel->setGrabModes(ImagePlatform::AllScreens);QTest::qWait(80);
    captureModel->setGrabModes(ImagePlatform::ActiveWindow);QTest::qWait(80);
    if (captureReset.size()!=2 || target->property("count").toInt()!=1 || target->property("currentValue").toInt()!=CaptureModeModel::ActiveWindow) {qWarning()<<captureModel<<captureModel->rowCount()<<pane->property("captureModes")<<captureReset.size()<<target->property("count")<<target->property("currentValue")<<target->property("currentIndex");fail("same-count capture reset");return;}
    recordingModel->setRecordingModes(VideoPlatform::Screen);QTest::qWait(80);
    recordingModel->setRecordingModes(VideoPlatform::Window);QTest::qWait(80);
    if (recordingReset.size()!=2 || control("nkshotRecordingTarget")->property("currentValue").toInt()!=VideoPlatform::Window) {fail("same-count recording reset");return;}
    restoreModels.dismiss();captureModel->setGrabModes(oldGrab);recordingModel->setRecordingModes(oldRecording);QTest::qWait(80);
    if (core->imagePlatform()->supportedShutterModes().testFlag(ImagePlatform::Immediate)) {
        Settings::setCaptureOnClick(false);Settings::setCaptureDelay(99);
        if (!click("nkshotSetupPrimary") || !QTest::qWaitFor([&]{return core->captureTimeRemaining()>90000;},1500)) {fail("real Core countdown dispatch");return;}
        // Cancel more than 90 seconds before the backend's scheduled doGrab.
        viewer->showNormal();viewer->requestActivate();QTest::qWait(150);
        capture(QStringLiteral(".countdown.png"));
        const auto cancel = control("nkshotSetupCancel");
        const auto more = control("nkshotStartupMore");
        const QRectF windowBounds(QPointF(),viewer->size());
        if (!cancel || !more || control("nkshotSetupPrimary")->isVisible()
            || !windowBounds.contains(cancel->mapRectToScene(QRectF(0,0,cancel->width(),cancel->height())))
            || !windowBounds.contains(more->mapRectToScene(QRectF(0,0,more->width(),more->height())))) {fail("countdown cancel outside window");return;}
        if (!click("nkshotSetupRecordingTab") || !click("nkshotSetupCancel")
            || !QTest::qWaitFor([&]{return core->captureTimeRemaining()==0;},1000)) {fail("real Core countdown cancellation");return;}
        QTest::qWait(100);
        if (!click("nkshotSetupScreenshotTab")) {fail("return after countdown");return;}
        qInfo()<<"Startup actual Core 99-second countdown started and cancelled before doGrab";
    }
    if (!click("nkshotStartupMore")) {fail("more entry");return;}
    QTest::keyClick(viewer,Qt::Key_Escape);QTest::qWait(100);
    if (application.clipboard()->text()!=QStringLiteral("startup unchanged") || !core->annotationDocument()->baseImage().isNull()
        || core->currentVideo()!=originalVideo || core->captureTimeRemaining()!=0) {fail("state isolation");return;}
    qInfo()<<"Startup real QML tabs/target/settings/keyboard/disclosure/no-capability/model reset/state isolation passed";
    Settings::setIncludePointer(originalPointer);Settings::setIncludeDecorations(originalDecorations);
    Settings::setIncludeShadow(originalShadow);Settings::setQuitAfterSaveCopyExport(originalQuit);
    Settings::setCaptureDelay(originalDelay);Settings::setCaptureOnClick(originalClick);Settings::setCaptureMode(originalMode);Settings::setVideoRecordMicrophone(originalMic);
    restore.dismiss();QTest::qWait(100);
    viewer->close();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);application.exit(0);
}

static void runScreenColorChecks(QApplication &application, ViewerWindow *viewer, const QString &destination)
{
    const auto fail=[&](const char *step){
        qCritical()<<"Screen color UI failure:"<<step;
        for(auto window:QGuiApplication::allWindows())if(auto picker=qobject_cast<ColorPickerWindow *>(window))picker->close();
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        application.exit(254);
    };
    if (!qEnvironmentVariableIsSet("NKSHOT_TEST_PRIVATE_BUS")) {fail("explicit private bus required");return;}
    const bool external=qEnvironmentVariableIsSet("NKSHOT_TEST_SCREEN_EXTERNAL");
    const bool video=qEnvironmentVariableIsSet("NKSHOT_TEST_SCREEN_VIDEO");
    const bool startup=qEnvironmentVariableIsSet("NKSHOT_TEST_SCREEN_STARTUP");
    const bool noImage=video||startup;
    const auto expectedStatus=qEnvironmentVariable("NKSHOT_TEST_SCREEN_EXPECT",QStringLiteral("success"));
    const auto expectedHex=qEnvironmentVariable("NKSHOT_TEST_SCREEN_HEX",QStringLiteral("#336699"));
    auto document=SpectacleCore::instance()->annotationDocument();
    const auto original=startup ? document->baseImage() : document->renderToImage();const auto depth=document->undoStackDepth();
    if(startup&&!original.isNull()){fail("launch-only must have no image");return;}
    const auto originalVideo=SpectacleCore::instance()->currentVideo();
    QApplication::clipboard()->setText(QStringLiteral("screen color unchanged"));
    QProcess fake;
    const QString service=QStringLiteral("org.freedesktop.portal.Desktop");
    auto bus=QDBusConnection::sessionBus();
    if (!bus.isConnected()) {fail("private bus connection");return;}
    if (!external) {
        if (bus.interface()->isServiceRegistered(service).value()) {fail("private portal owner conflict");return;}
        fake.start(QCoreApplication::applicationDirPath()+QStringLiteral("/screen_color_picker_test"),{QStringLiteral("--fake"),QStringLiteral("ui")});
        if (!fake.waitForStarted() || !QTest::qWaitFor([&]{return bus.interface()->isServiceRegistered(service).value();},3000)) {fail("fake portal startup");return;}
    }
    const auto stopFake=qScopeGuard([&]{if(fake.state()!=QProcess::NotRunning){fake.terminate();fake.waitForFinished(3000);}});
    const auto mode=[&](const QString &value){
        auto message=QDBusMessage::createMethodCall(service,QStringLiteral("/org/freedesktop/portal/desktop"),QStringLiteral("org.nkshot.TestPortal.Control"),QStringLiteral("SetMode"));
        message.setArguments({value});return bus.call(message,QDBus::Block,1000).type()!=QDBusMessage::ErrorMessage;
    };
    if(video)SpectacleCore::instance()->setVideoMode(true);
    if(startup){
        auto button=visualItem(viewer->contentItem(),QStringLiteral("nkshotStartupMore"));
        if(!button||!button->isVisible()){fail("launch-only More entry");return;}
        QTest::mouseClick(viewer,Qt::LeftButton,Qt::NoModifier,button->mapToScene(QPointF(button->width()/2,button->height()/2)).toPoint());
    } else {
        auto more=viewer->rootObject()->findChild<QObject *>(QStringLiteral("nkshotMoreMenu"));
        if(!more||!QMetaObject::invokeMethod(more,"open")){fail("More menu");return;}
    }
    QTest::qWait(150);
    if(startup&&!viewer->grabWindow().save(destination+QStringLiteral(".entry.png"))){fail("launch-only entry capture");return;}
    auto entry=visualItem(viewer->contentItem(),startup?QStringLiteral("nkshotStartupColorAction"):QStringLiteral("nkshotColorPickerAction"));
    if(!entry||!entry->isVisible()||!entry->isEnabled()){fail("independent menu entry");return;}
    QTest::mouseClick(viewer,Qt::LeftButton,Qt::NoModifier,entry->mapToScene(QPointF(entry->width()/2,entry->height()/2)).toPoint());
    QPointer<ColorPickerWindow> picker;
    if(!QTest::qWaitFor([&]{for(auto w:QGuiApplication::allWindows())if(auto p=qobject_cast<ColorPickerWindow *>(w))picker=p;return picker&&picker->status()==QQuickView::Ready;},3000)){fail("picker open");return;}
    picker->resize(qEnvironmentVariableIsSet("NKSHOT_TEST_SCREEN_COMPACT")?QSize(420,520):QSize(640,560));picker->requestActivate();QTest::qWait(150);
    const auto click=[&](const char *name){auto item=visualItem(picker->contentItem(),QString::fromLatin1(name));if(!item||!item->isVisible()||!item->isEnabled())return false;QTest::mouseClick(picker,Qt::LeftButton,Qt::NoModifier,item->mapToScene(QPointF(item->width()/2,item->height()/2)).toPoint());return true;};
    if(noImage){
        if(picker->property("hasImage").toBool()||!picker->property("screenMode").toBool()){fail("video defaults to independent Screen");return;}
        if(!click("nkshotColorSource")){fail("no-image source selector");return;}
        QTest::keyClick(picker,Qt::Key_Home);QTest::keyClick(picker,Qt::Key_Return);
        auto selector=visualItem(picker->contentItem(),QStringLiteral("nkshotColorSource"));
        if(!picker->property("screenMode").toBool()||!selector||selector->property("currentIndex").toInt()!=1){fail("no-image Screenshot disabled for keyboard");return;}
    }
    else {
        if(!click("nkshotColorSource")){fail("source selector");return;}
        QTest::keyClick(picker,Qt::Key_End);QTest::keyClick(picker,Qt::Key_Return);
        if(!picker->property("screenMode").toBool()){fail("Screen source selection");return;}
    }
    QTest::qWait(200); // Source-mode layout and the combo popup must settle before hit testing.
    if(picker->hasSample()||picker->property("screenPending").toBool()||QApplication::clipboard()->text()!=QStringLiteral("screen color unchanged")){fail("no implicit request or copy");return;}
    if(!click("nkshotScreenPick")){fail("explicit Pick");return;}
    if(!external) {
        if(!QTest::qWaitFor([&]{return picker->property("screenPending").toBool();},1000)){qWarning()<<picker->property("screenStatus")<<picker->property("screenMode")<<picker->isVisible();fail("pending state");return;}
        QTest::qWait(80);
        if(!picker->grabWindow().save(destination+QStringLiteral(".waiting.png"))){fail("pending capture");return;}
    }
    if(qEnvironmentVariableIsSet("NKSHOT_TEST_SCREEN_EXTERNAL_CANCEL")){
        const auto pickMarker=qEnvironmentVariable("NKSHOT_TEST_SCREEN_PICK_MARKER");
        const auto pickLog=qEnvironmentVariable("NKSHOT_TEST_SCREEN_PICK_LOG");
        if(!pickMarker.isEmpty()){
            if(!QTest::qWaitFor([&]{return QFileInfo::exists(pickMarker);},5000)){fail("external method received before cancellation");return;}
        } else if(!pickLog.isEmpty()){
            if(!QTest::qWaitFor([&]{QFile log(pickLog);return log.open(QIODevice::ReadOnly)&&log.readAll().contains("\"event\": \"pick\"");},3000)){fail("external method received before cancellation");return;}
        } else QTest::qWait(300);
        if(!click("nkshotScreenCancel")){fail("external stop waiting");return;}
    }
    if(!QTest::qWaitFor([&]{return !picker->property("screenPending").toBool();},5000)){fail("portal response");return;}
    auto portalJob=picker->findChild<ScreenColorPicker *>();
    if(!portalJob||portalJob->status()!=expectedStatus){qWarning()<<(portalJob?portalJob->status():QString())<<picker->property("screenStatus");fail("exact portal outcome");return;}
    if(qEnvironmentVariableIsSet("NKSHOT_TEST_SCREEN_EXTERNAL_CANCEL")){
        QTest::qWait(1800);
        if(portalJob->status()!=expectedStatus||picker->hasSample()||QApplication::clipboard()->text()!=QStringLiteral("screen color unchanged")){fail("late response after stop waiting ignored");return;}
    }
    if(expectedStatus==QStringLiteral("success")){
        if(!picker->hasSample()||picker->hexValue()!=expectedHex||QApplication::clipboard()->text()!=QStringLiteral("screen color unchanged")){qWarning()<<picker->hexValue()<<picker->property("screenStatus");fail("wire result and no auto copy");return;}
        if(!click("nkshotColorCopyHex")||QApplication::clipboard()->text()!=expectedHex){fail("explicit HEX copy");return;}
        if(!click("nkshotColorCopyRgb")||QApplication::clipboard()->text()!=picker->rgbValue()){fail("explicit RGB copy");return;}
        QTest::keyClick(picker,Qt::Key_C,Qt::ControlModifier);
        if(QApplication::clipboard()->text()!=expectedHex){fail("keyboard copy");return;}
    } else {
        if(picker->hasSample()||QApplication::clipboard()->text()!=QStringLiteral("screen color unchanged")){fail("failed selection cannot copy");return;}
        QTest::keyClick(picker,Qt::Key_C,Qt::ControlModifier);QTest::qWait(80);
        if(QApplication::clipboard()->text()!=QStringLiteral("screen color unchanged")){fail("failed selection keyboard copy remains inert");return;}
    }
    QTest::qWait(200);
    if(!picker->grabWindow().save(destination)){fail("result capture");return;}
    if(!external){
        if(!mode(QStringLiteral("hold"))||!click("nkshotScreenPick")||!QTest::qWaitFor([&]{return picker->property("screenPending").toBool();},1000)||!click("nkshotScreenCancel")
            ||!QTest::qWaitFor([&]{return !picker->property("screenPending").toBool();},1000)||picker->hasSample()){fail("stop waiting");return;}
        QTest::qWait(200);if(!picker->grabWindow().save(destination+QStringLiteral(".cancelled.png"))){fail("cancel capture");return;}
        if(!mode(QStringLiteral("malformed"))||!click("nkshotScreenPick")||!QTest::qWaitFor([&]{return !picker->property("screenPending").toBool();},3000)||picker->hasSample()){fail("malformed tuple rejected");return;}
        QTest::qWait(200);if(!picker->grabWindow().save(destination+QStringLiteral(".error.png"))){fail("error capture");return;}
        if(!mode(QStringLiteral("ui"))||!click("nkshotScreenPick")||!QTest::qWaitFor([&]{return !picker->property("screenPending").toBool();},3000)||!picker->hasSample()){fail("explicit retry");return;}
        if(QApplication::clipboard()->text()!=expectedHex){fail("retry must not copy");return;}
        if(!mode(QStringLiteral("hold"))||!click("nkshotScreenPick")||!QTest::qWaitFor([&]{return picker->property("screenPending").toBool();},1000)){fail("pending minimize setup");return;}
        picker->setWindowStates(Qt::WindowMinimized);
        if(!QTest::qWaitFor([&]{return !picker->property("screenPending").toBool();},1000)||picker->hasSample()){fail("minimize invalidates pending result");return;}
        picker->setWindowStates(Qt::WindowNoState);picker->show();picker->requestActivate();QTest::qWait(150);
        if(picker->property("screenPending").toBool()||picker->hasSample()){fail("restore does not restart or revive color");return;}
        if(!noImage){
            if(!click("nkshotColorSource")){fail("return source selector");return;}
            QTest::keyClick(picker,Qt::Key_Home);QTest::keyClick(picker,Qt::Key_Return);
            if(picker->property("screenMode").toBool()||picker->hasSample()){fail("screenshot state isolated");return;}
            auto viewport=visualItem(picker->contentItem(),QStringLiteral("nkshotColorViewport"));
            if(!viewport||!viewport->isVisible()){fail("screenshot viewport returns");return;}
            QTest::mouseClick(picker,Qt::LeftButton,Qt::NoModifier,viewport->mapToScene(QPointF(viewport->width()/2,viewport->height()/2)).toPoint());
            if(!picker->hasSample()||QApplication::clipboard()->text()!=expectedHex){fail("image sampling still explicit");return;}
        }
    }
    if(!external){
        if(!picker->property("screenMode").toBool()){
            if(!click("nkshotColorSource")){fail("close-pending source selector");return;}
            QTest::keyClick(picker,Qt::Key_End);QTest::keyClick(picker,Qt::Key_Return);QTest::qWait(150);
        }
        if(!mode(QStringLiteral("hold"))||!click("nkshotScreenPick")||!QTest::qWaitFor([&]{return picker->property("screenPending").toBool();},1000)){fail("pending Escape setup");return;}
        QTest::keyClick(picker,Qt::Key_Escape);
        if(!picker||picker->property("screenPending").toBool()||picker->hasSample()){fail("Escape stops waiting without reviving old color");return;}
        if(!click("nkshotScreenPick")||!QTest::qWaitFor([&]{return picker->property("screenPending").toBool();},1000)||!click("nkshotColorClose")){fail("close pending request");return;}
    } else QTest::keyClick(picker,Qt::Key_Escape);
    if(!QTest::qWaitFor([&]{return picker.isNull();},1000)||!(startup ? document->baseImage().isNull() : samePixels(original,document->renderToImage()))||depth!=document->undoStackDepth()
        ||originalVideo!=SpectacleCore::instance()->currentVideo()){fail("close and source/history/video isolation");return;}
    qInfo()<<"Screen portal UI passed; external:"<<external<<"video:"<<video<<"launch-only:"<<startup<<"expected result:"<<expectedStatus;
    application.exit(0);
}

static void runWordScrollChecks(QApplication &application, ViewerWindow *viewer, const QString &destination)
{
    const auto fail = [&](const char *step) { qCritical() << "Word autoscroll failure:" << step; application.exit(251); };
    const auto configured = Settings::ocrLanguages();
    const auto restore = qScopeGuard([configured] { Settings::setOcrLanguages(configured); });
    Settings::setOcrLanguages({QStringLiteral("eng")});
    auto document = SpectacleCore::instance()->annotationDocument();
    const auto original = document->renderToImage();
    const auto depth = document->undoStackDepth();
    QImage fixture(1600, 1600, QImage::Format_ARGB32); fixture.fill(Qt::white); fixture.setDevicePixelRatio(2);
    {
        QPainter painter(&fixture); painter.scale(0.5, 0.5);
        QFont font(QStringLiteral("DejaVu Sans")); font.setPixelSize(40); painter.setFont(font); painter.setPen(Qt::black);
        const QStringList labels{QStringLiteral("ALPHA"), QStringLiteral("BRAVO"), QStringLiteral("CHARLIE"), QStringLiteral("DELTA"), QStringLiteral("ECHO"), QStringLiteral("FOXTROT")};
        for (int row = 0; row < 12; ++row)
            for (int col = 0; col < 6; ++col)
                painter.drawText(QRect(40 + col * 250, 30 + row * 130, 220, 80), Qt::AlignCenter, labels[col]);
    }
    QPointer<PinnedImageWindow> pin = new PinnedImageWindow(fixture, viewer->engine());
    pin->resize(qEnvironmentVariableIsSet("NKSHOT_TEST_WORDS_COMPACT") ? QSize(260, 180) : QSize(620, 420));
    pin->show(); pin->requestActivate(); QTest::qWait(150);
    auto viewport = visualItem(pin->contentItem(), QStringLiteral("nkshotPinViewport"));
    auto snapshot = visualItem(pin->contentItem(), QStringLiteral("nkshotPinSnapshot"));
    if (!viewport || !snapshot) { fail("items"); return; }
    QApplication::clipboard()->setText(QStringLiteral("autoscroll untouched"));
    const auto recognize = [&] {
        pin->selectTextOnImage();
        return QTest::qWaitFor([&] { return !pin->extractingText(); }, 15000) && pin->textSelectionActive() && pin->textWords().size() == 72;
    };
    if (!recognize()) { qWarning() << pin->textWords().size(); fail("fixture recognition"); return; }
    const auto words = pin->textWords();
    // Tesseract may group the grid into columns/blocks. Directional drag anchors
    // are spatial corners, while selection serialization remains logical OCR order.
    const auto nearestWord = [&](const QPointF &target) {
        int result=0; qreal distance=std::numeric_limits<qreal>::max();
        for (int i=0;i<words.size();++i) {
            const qreal candidate=QLineF(target,words[i].toMap().value(QStringLiteral("bounds")).toRect().center()).length();
            if(candidate<distance) { distance=candidate; result=i; }
        }
        return result;
    };
    const int topLeft=nearestWord(QPointF(0,0));
    const int topRight=nearestWord(QPointF(fixture.width(),0));
    const int bottomLeft=nearestWord(QPointF(0,fixture.height()));
    const auto offset = [&] { return QPointF(viewport->property("contentX").toReal(), viewport->property("contentY").toReal()); };
    const auto bounds = [&] { return QPointF(viewport->property("contentWidth").toReal()-viewport->width(), viewport->property("contentHeight").toReal()-viewport->height()); };
    const auto localPoint = [&](qreal x, qreal y) { return viewport->mapToScene(QPointF(x,y)).toPoint(); };
    const auto center = [&] { return localPoint(viewport->width()/2, viewport->height()/2); };
    const auto wordPoint = [&](int i) {
        return snapshot->mapToScene(QPointF(words[i].toMap().value(QStringLiteral("bounds")).toRect().center()) * viewport->property("imageScale").toReal()).toPoint();
    };
    const auto begin = [&](int index) {
        const auto box = words[index].toMap().value(QStringLiteral("bounds")).toRect();
        pin->selectWordAt(box.center().x(), box.center().y()); QTest::qWait(30);
        const auto point = wordPoint(index);
        if (!viewport->mapRectToScene(QRectF(0,0,viewport->width(),viewport->height())).contains(point)) return false;
        QTest::mousePress(pin, Qt::LeftButton, Qt::NoModifier, point);
        return pin->selectionFirst()==index && pin->selectionLast()==index;
    };
    const auto stable = [&] {
        const auto before = offset(); QTest::qWait(180);
        return QLineF(before, offset()).length() < 0.1;
    };
    const auto release = [&] { QTest::mouseRelease(pin, Qt::LeftButton, Qt::NoModifier, center()); };
    QTest::keyClick(pin, Qt::Key_1, Qt::ControlModifier); QTest::qWait(80);
    // A blank press does not establish an anchor, even when subsequently held at an edge.
    viewport->setProperty("contentX",0); viewport->setProperty("contentY",0);
    QTest::mousePress(pin, Qt::LeftButton, Qt::NoModifier, localPoint(2,2));
    QTest::mouseMove(pin, localPoint(viewport->width()-2, viewport->height()-2));
    if (!stable() || pin->selectionFirst()!=-1) { release(); fail("blank press starts no drag"); return; }
    release();
    // Hold an unmoving pointer at each edge for multiple timer frames. No repeated move events.
    for (int direction=0; direction<4; ++direction) {
        const bool horizontal = direction < 2;
        const bool forward = direction % 2 == 0;
        const int anchor = horizontal ? (forward ? topLeft : topRight) : (forward ? topLeft : bottomLeft);
        if (!begin(anchor)) { fail("visible drag anchor"); return; }
        const QPoint edge = horizontal
            ? localPoint(forward ? viewport->width()-2 : 2, viewport->height()/2)
            : localPoint(viewport->width()/2, forward ? viewport->height()-2 : 2);
        QTest::mouseMove(pin, edge, 20);
        const auto before = offset(); const int firstActive = pin->activeWord();
        QTest::qWait(220); const auto middle = offset();
        QTest::qWait(350); const auto after = offset();
        const qreal a = horizontal ? before.x() : before.y();
        const qreal b = horizontal ? middle.x() : middle.y();
        const qreal c = horizontal ? after.x() : after.y();
        if ((forward ? !(b>a && c>b) : !(b<a && c<b)) || pin->activeWord()==firstActive
            || pin->selectionFirst()>anchor || pin->selectionLast()<anchor) {
            qWarning() << direction << before << middle << after << firstActive << pin->activeWord();
            for (int i=0;i<words.size();++i) qWarning() << "OCR word" << i << words[i];
            release(); fail("stationary edge scroll and extension"); return;
        }
        // Move to the neutral center without releasing: scrolling must stop.
        QTest::mouseMove(pin, center(), 20);
        if (!stable()) { release(); fail("center stop"); return; }
        const int heldFirst=pin->selectionFirst(), heldLast=pin->selectionLast(), heldActive=pin->activeWord();
        QTest::keyClick(pin,Qt::Key_End);
        if (pin->selectionFirst()!=heldFirst || pin->selectionLast()!=heldLast || pin->activeWord()!=heldActive) {
            release(); fail("held drag ignores navigation without resetting anchor"); return;
        }
        // Boundary clamping while still dragging, including reverse left/up movement.
        const auto maximum = bounds();
        viewport->setProperty(horizontal ? "contentX" : "contentY", forward ? (horizontal ? maximum.x() : maximum.y()) - 5 : 5);
        QTest::mouseMove(pin, edge, 20); QTest::qWait(180);
        const auto end = offset(); const qreal actual = horizontal ? end.x() : end.y();
        const qreal expected = forward ? (horizontal ? maximum.x() : maximum.y()) : 0;
        if (qAbs(actual-expected)>0.1 || !stable()) { release(); fail("edge clamp"); return; }
        QTest::mouseRelease(pin, Qt::LeftButton, Qt::NoModifier, edge);
        if (!stable()) { fail("release stop"); return; }
        QStringList selected;
        for(int i=pin->selectionFirst(); i<=pin->selectionLast(); ++i) selected << words[i].toMap().value(QStringLiteral("text")).toString();
        if (pin->selectedText().simplified()!=selected.join(u' ')) { fail("logical order"); return; }
    }
    // Reverse across the original anchor without releasing the button. Use one
    // physical column so both spatial and OCR logical direction are unambiguous.
    const int reverseAnchor=nearestWord(QPointF(150,800));
    const auto anchorBox=words[reverseAnchor].toMap().value(QStringLiteral("bounds")).toRect();
    if (!begin(reverseAnchor)) { fail("cross-anchor start"); return; }
    const auto anchorX=viewport->mapFromScene(wordPoint(reverseAnchor)).x();
    QTest::mouseMove(pin,localPoint(anchorX,viewport->height()-2),20);
    if (!QTest::qWaitFor([&] {
            return pin->activeWord()>reverseAnchor && pin->selectionFirst()==reverseAnchor
                && words[pin->activeWord()].toMap().value(QStringLiteral("bounds")).toRect().center().y()>anchorBox.center().y();
        },3000)) { release(); fail("forward half of cross-anchor gesture"); return; }
    // Continue until the original anchor has scrolled above the viewport; the
    // return gesture must genuinely scroll back, not merely cross a visible word.
    QTest::qWait(int(viewport->height()*1000/350)+200);
    const qreal reverseStartY=offset().y();
    if(reverseStartY<=anchorBox.bottom()) { release(); fail("anchor scrolls out before reverse"); return; }
    QTest::mouseMove(pin,localPoint(anchorX,2),20);
    if (!QTest::qWaitFor([&] {
            return offset().y()<reverseStartY && pin->activeWord()<reverseAnchor && pin->selectionLast()==reverseAnchor
                && words[pin->activeWord()].toMap().value(QStringLiteral("bounds")).toRect().center().y()<anchorBox.center().y();
        },3000)) { release(); fail("same-gesture reverse crosses preserved anchor"); return; }
    QTest::mouseRelease(pin,Qt::LeftButton,Qt::NoModifier,localPoint(anchorX,2));
    if (!stable()) { fail("reverse release stop"); return; }
    QStringList reversed;
    for(int i=pin->selectionFirst();i<=pin->selectionLast();++i) reversed << words[i].toMap().value(QStringLiteral("text")).toString();
    if(pin->selectedText().simplified()!=reversed.join(u' ')) { fail("cross-anchor logical order"); return; }
    // Timer endpoint mapping also runs at a non-unit scale, not only 100%.
    QTest::keyClick(pin, Qt::Key_Minus, Qt::ControlModifier); QTest::qWait(60);
    const qreal reducedScale = viewport->property("imageScale").toReal();
    if (!(reducedScale > 0 && reducedScale < 1) || !begin(topLeft)) { fail("scaled drag start"); return; }
    QTest::mouseMove(pin, localPoint(viewport->width()/2, viewport->height()-2), 20);
    const auto scaledStart = offset(); const int scaledActive = pin->activeWord();
    QTest::qWait(400);
    if (offset().y() <= scaledStart.y() || pin->activeWord() == scaledActive
        || pin->selectionFirst() > topLeft || pin->selectionLast() < topLeft) {
        release(); fail("scaled stationary-edge extension"); return;
    }
    release();
    if (!stable()) { fail("scaled release stop"); return; }
    QTest::keyClick(pin, Qt::Key_1, Qt::ControlModifier); QTest::qWait(60);
    // Diagonal edge movement must advance both axes from the same fixed viewport pointer.
    if (!begin(topLeft)) { fail("diagonal anchor"); return; }
    QTest::mouseMove(pin, localPoint(viewport->width()-2,viewport->height()-2),20);
    const auto diagonalStart=offset(); const int diagonalActive=pin->activeWord(); QTest::qWait(350);
    if (offset().x()<=diagonalStart.x() || offset().y()<=diagonalStart.y() || pin->activeWord()==diagonalActive
        || pin->selectionFirst()>topLeft || pin->selectionLast()<topLeft) { release(); fail("two axis drag"); return; }
    if (!pin->grabWindow().save(destination)) { release(); fail("capture"); return; }
    // A pointer far outside must not jump the endpoint many screens ahead.
    // Its clamped endpoint remains near the currently visible physical image edge.
    const auto beforeOutside = offset();
    const QPoint farOutside(pin->width()+10000,pin->height()+10000);
    QTest::mouseMove(pin, farOutside, 1);
    const auto afterOutside = offset();
    const auto activeBox = words[pin->activeWord()].toMap().value(QStringLiteral("bounds")).toRect();
    const qreal scale = viewport->property("imageScale").toReal();
    if (QLineF(beforeOutside,afterOutside).length()>40
        || activeBox.top() > (afterOutside.y()+viewport->height())/scale+130
        || activeBox.left() > (afterOutside.x()+viewport->width())/scale+250) {
        release(); fail("outside endpoint remains at viewport edge"); return;
    }
    // Release outside the viewport remains an unconditional stop.
    QTest::mouseRelease(pin, Qt::LeftButton, Qt::NoModifier, farOutside);
    if (!stable()) { fail("outside release stop"); return; }
    // Cancellation and lifecycle changes terminate an active edge drag, not just recognition.
    for (int operation=0; operation<5; ++operation) {
        if (!pin->textSelectionActive() && !recognize()) { fail("reopen"); return; }
        if (!begin(topLeft)) { fail("lifecycle anchor"); return; }
        QTest::mouseMove(pin,localPoint(viewport->width()-2,viewport->height()-2),20); QTest::qWait(100);
        if (operation==0) pin->cancelTextSelection();
        else if (operation==1) QTest::keyClick(pin,Qt::Key_Escape);
        else if (operation==2) { pin->hide(); pin->show(); }
        else if (operation==3) { pin->setWindowStates(Qt::WindowMinimized); QTest::qWait(30); pin->setWindowStates(Qt::WindowNoState); pin->show(); }
        else { pin->close(); QTest::qWait(200); if(pin) { fail("close"); return; } break; }
        QTest::qWait(30);
        if (pin->textSelectionActive() || !stable()) { release(); fail("lifecycle stop"); return; }
        release(); pin->requestActivate();
        if (!samePixels(fixture,pin->snapshot()) || pin->textWords().size()!=0) { fail("immutable pin / mode cleanup"); return; }
    }
    if (QApplication::clipboard()->text()!=QStringLiteral("autoscroll untouched")
        || !samePixels(original,document->renderToImage()) || depth!=document->undoStackDepth()) { fail("clipboard/source/history untouched"); return; }
    qInfo() << "Word autoscroll: four stationary edges, boundaries, center/outside release, scaled mapping, cross-anchor reversal, diagonal, logical order and five lifecycle stops passed";
    application.exit(0);
}

static void runInlineTextChecks(QApplication &application, ViewerWindow *viewer, const QString &destination)
{
    const auto fail = [&](const char *step) { qCritical() << "Inline text failure:" << step; application.exit(250); };
    const auto configured = Settings::ocrLanguages();
    const auto restore = qScopeGuard([configured] { Settings::setOcrLanguages(configured); });
    const bool chinese = qEnvironmentVariableIsSet("NKSHOT_TEST_WORDS_CHINESE");
    const bool blank = qEnvironmentVariableIsSet("NKSHOT_TEST_WORDS_BLANK");
    const bool missing = qEnvironmentVariableIsSet("NKSHOT_TEST_WORDS_MISSING");
    Settings::setOcrLanguages({missing ? QStringLiteral("nkshot_missing_model") : chinese ? QStringLiteral("chi_sim") : QStringLiteral("eng")});
    auto document = SpectacleCore::instance()->annotationDocument();
    const auto original = document->renderToImage();
    const auto depth = document->undoStackDepth();
    QImage fixture(1200, 500, QImage::Format_ARGB32); fixture.fill(Qt::white); fixture.setDevicePixelRatio(2);
    if (!blank) {
        QPainter painter(&fixture); painter.scale(0.5, 0.5);
        QFont font(chinese ? QStringLiteral("Noto Sans CJK SC") : QStringLiteral("DejaVu Sans")); font.setPixelSize(68); painter.setFont(font); painter.setPen(Qt::black);
        painter.drawText(QRect(40, 45, 1120, 150), Qt::AlignCenter, chinese ? QStringLiteral("中文截图编辑 12345") : QStringLiteral("ALPHA BETA GAMMA"));
        painter.drawText(QRect(40, 260, 1120, 150), Qt::AlignCenter, chinese ? QStringLiteral("第二行识别 67890") : QStringLiteral("DELTA EPSILON ZETA"));
    }
    QPointer<PinnedImageWindow> pin = new PinnedImageWindow(fixture, viewer->engine());
    pin->resize(qEnvironmentVariableIsSet("NKSHOT_TEST_WORDS_COMPACT") ? QSize(260, 180) : QSize(820, 500));
    pin->show(); pin->requestActivate(); QTest::qWait(200);
    QSignalSpy statuses(pin, &PinnedImageWindow::textExtractionStatus);
    const auto click = [&](const char *name) {
        auto item = visualItem(pin->contentItem(), QString::fromLatin1(name));
        if (!item || !item->isVisible() || !item->isEnabled()) return false;
        QTest::mouseClick(pin, Qt::LeftButton, Qt::NoModifier, item->mapToScene(QPointF(item->width()/2, item->height()/2)).toPoint());
        return true;
    };
    QApplication::clipboard()->setText(QStringLiteral("untouched"));
    if (!click("nkshotPinExtractText")) { fail("menu button"); return; }
    QTest::qWait(100);
    if (!click("nkshotPinSelectText") || !pin->textSelectionActive() || !pin->extractingText()) { fail("inline action"); return; }
    pin->selectTextOnImage(); // Coalesce while recognition is active.
    if (!QTest::qWaitFor([&] { return !pin->extractingText(); }, 15000)) { fail("recognition timeout"); return; }
    if (blank || missing) {
        const auto message = statuses.isEmpty() ? QString() : statuses.last().first().toString();
        if (pin->textSelectionActive() || !pin->textWords().isEmpty() || !message.contains(missing ? QStringLiteral("unavailable") : QStringLiteral("No text"))
            || QApplication::clipboard()->text() != QStringLiteral("untouched")) { fail("failure boundary"); return; }
        QTest::qWait(100); if (!pin->grabWindow().save(destination)) { fail("failure capture"); return; }
        pin->close(); application.exit(0); return;
    }
    const auto words = pin->textWords();
    auto viewport = visualItem(pin->contentItem(), QStringLiteral("nkshotPinViewport"));
    auto snapshot = visualItem(pin->contentItem(), QStringLiteral("nkshotPinSnapshot"));
    auto preview = visualItem(pin->contentItem(), QStringLiteral("nkshotPinSelectedText"));
    auto copyText = visualItem(pin->contentItem(), QStringLiteral("nkshotPinCopyText"));
    if (!pin->textSelectionActive() || words.size() < 4 || !viewport || !snapshot || !preview || !copyText || copyText->isEnabled()
        || preview->property("textFormat").toInt() != 0 || !viewport->hasActiveFocus()) { fail("ready state"); return; }
    QTest::keyClick(pin, Qt::Key_C, Qt::ControlModifier);
    if (QApplication::clipboard()->text() != QStringLiteral("untouched")) { fail("empty text copy"); return; }
    const auto pointForWord = [&](int index) {
        const auto box = words[index].toMap().value(QStringLiteral("bounds")).toRect();
        return snapshot->mapToScene(QPointF(box.center()) * viewport->property("imageScale").toReal()).toPoint();
    };
    QTest::mouseClick(pin, Qt::LeftButton, Qt::NoModifier, pointForWord(0));
    if (pin->selectedText() != words[0].toMap().value(QStringLiteral("text")).toString() || QApplication::clipboard()->text() != QStringLiteral("untouched")) { fail("first word"); return; }
    QTest::mouseClick(pin, Qt::LeftButton, Qt::ShiftModifier, pointForWord(2));
    if (pin->selectionFirst() != 0 || pin->selectionLast() != 2) { fail("shift range"); return; }
    QTest::mousePress(pin, Qt::LeftButton, Qt::NoModifier, pointForWord(2));
    QTest::mouseMove(pin, pointForWord(0), 60); QTest::mouseRelease(pin, Qt::LeftButton, Qt::NoModifier, pointForWord(0));
    if (pin->selectionFirst() != 0 || pin->selectionLast() != 2) { fail("reverse drag"); return; }
    QTest::keyClick(pin, Qt::Key_End, Qt::ShiftModifier);
    if (pin->selectionLast() != words.size()-1 || !pin->selectedText().contains(u'\n')) { fail("multi-line keyboard range"); return; }
    QTest::keyClick(pin, Qt::Key_A, Qt::ControlModifier);
    const auto all = pin->selectedText();
    auto normalized = all.simplified(); if (chinese) normalized.remove(u' ');
    if (normalized != (chinese ? QStringLiteral("中文截图编辑12345第二行识别67890") : QStringLiteral("ALPHA BETA GAMMA DELTA EPSILON ZETA"))) { qWarning() << all; fail("selected text order"); return; }
    if (!click("nkshotPinCopyText") || QApplication::clipboard()->text() != all) { fail("explicit text copy"); return; }
    QTest::keyClick(pin, Qt::Key_C, Qt::ControlModifier);
    if (QApplication::clipboard()->text() != all) { fail("text shortcut"); return; }
    if (!click("nkshotPinCopy") || !samePixels(QApplication::clipboard()->image(), fixture) || pin->selectedText() != all) { fail("separate image copy"); return; }
    QTest::qWait(100);
    if (!pin->grabWindow().save(destination + QStringLiteral(".all.png"))) { fail("all capture"); return; }
    // Save As in text mode still exports the complete immutable source image.
    QTemporaryDir folder;
    const auto output = folder.filePath(QStringLiteral("pin.png"));
    pin->saveAs(); QTest::qWait(100);
    QPointer<QFileDialog> saveDialog;
    for (auto widget : QApplication::topLevelWidgets())
        if (widget->objectName() == QStringLiteral("nkshotPinSaveDialog")) saveDialog = qobject_cast<QFileDialog *>(widget);
    auto filename = saveDialog ? saveDialog->findChild<QLineEdit *>(QStringLiteral("fileNameEdit")) : nullptr;
    if (!folder.isValid() || !filename) { fail("image save dialog"); return; }
    filename->setFocus(); filename->selectAll(); QTest::keyClicks(filename, output);
    if (!QTest::qWaitFor([&] { return saveDialog->selectedFiles() == QStringList{output}; }, 1000)
        || !QMetaObject::invokeMethod(saveDialog, "accept")
        || !QTest::qWaitFor([&] { return saveDialog.isNull(); }, 1000)
        || !samePixels(QImage(output), fixture) || !pin->textSelectionActive()) { fail("image export in text mode"); return; }
    pin->requestActivate(); QTest::qWait(50);
    // Zoom/pan does not alter source word bounds; keyboard selection reveals its word.
    QTest::keyClick(pin, Qt::Key_1, Qt::ControlModifier); QTest::qWait(80);
    viewport->setProperty("contentX", 150); viewport->setProperty("contentY", 0);
    const QPoint wheelPoint = viewport->mapToScene(QPointF(viewport->width()/2, viewport->height()/2)).toPoint();
    QWheelEvent scroll(wheelPoint, pin->mapToGlobal(wheelPoint), QPoint(), QPoint(0, -120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(pin, &scroll); QTest::qWait(50);
    if (viewport->property("contentY").toReal() <= 0) { fail("plain wheel scrolling"); return; }
    viewport->forceActiveFocus(); QTest::keyClick(pin, Qt::Key_Home); QTest::qWait(80);
    if (pin->selectionFirst() != 0 || pin->selectionLast() != 0 || pin->textWords() != words) { fail("zoom / metadata preservation"); return; }
    if (!viewport->mapRectToScene(QRectF(0, 0, viewport->width(), viewport->height())).contains(pointForWord(0))) { fail("Home reveals first word"); return; }
    QTest::keyClick(pin, Qt::Key_Right, Qt::ShiftModifier);
    if (pin->selectionFirst() != 0 || pin->selectionLast() != 1) { fail("arrow extension"); return; }
    const auto visiblePoint = pointForWord(1);
    if (!viewport->mapRectToScene(QRectF(0, 0, viewport->width(), viewport->height())).contains(visiblePoint)) { fail("keyboard reveal"); return; }
    QTest::mouseClick(pin, Qt::LeftButton, Qt::NoModifier, visiblePoint);
    if (pin->selectionFirst() != 1 || pin->selectionLast() != 1) { fail("panned hit mapping"); return; }
    QTest::keyClick(pin, Qt::Key_0, Qt::ControlModifier); QTest::qWait(80);
    QTest::mouseClick(pin, Qt::LeftButton, Qt::NoModifier, pointForWord(0));
    QTest::mouseClick(pin, Qt::LeftButton, Qt::ShiftModifier, pointForWord(2));
    if (!pin->grabWindow().save(destination)) { fail("selection capture"); return; }
    // Tab must allow returning to the image after using a footer action.
    click("nkshotPinCopyText");
    bool focused = false;
    for (int i = 0; i < 12 && !focused; ++i) { QTest::keyClick(pin, Qt::Key_Tab); focused = viewport->hasActiveFocus(); }
    if (!focused) { fail("Tab return"); return; }
    QTest::keyClick(pin, Qt::Key_Escape); QTest::qWait(50);
    if (!pin || !pin->isVisible() || pin->textSelectionActive() || !pin->textWords().isEmpty()) { fail("Escape mode cleanup"); return; }
    QTest::keyClick(pin, Qt::Key_C, Qt::ControlModifier);
    if (!samePixels(QApplication::clipboard()->image(), fixture)) { fail("restored image shortcut"); return; }
    // Cancel and hide/show while work is in flight must invalidate its completion.
    pin->selectTextOnImage(); pin->cancelTextSelection();
    if (!QTest::qWaitFor([&] { return !pin->extractingText(); }, 15000) || pin->textSelectionActive() || !pin->textWords().isEmpty()) { fail("cancel late completion"); return; }
    pin->selectTextOnImage(); pin->hide(); pin->show();
    if (!QTest::qWaitFor([&] { return !pin->extractingText(); }, 15000) || pin->textSelectionActive() || !pin->textWords().isEmpty()) { fail("hide epoch"); return; }
    pin->selectTextOnImage();
    if (!QTest::qWaitFor([&] { return !pin->extractingText(); }, 15000) || !pin->textSelectionActive()) { fail("reopen"); return; }
    pin->setWindowStates(Qt::WindowMinimized); QTest::qWait(50);
    if (pin->textSelectionActive()) { fail("minimize cleanup"); return; }
    pin->setWindowStates(Qt::WindowNoState); pin->show();
    if (!samePixels(fixture, pin->snapshot()) || !samePixels(original, document->renderToImage()) || depth != document->undoStackDepth()) { fail("immutable source"); return; }
    pin->selectTextOnImage(); pin->close(); QTest::qWait(200);
    if (pin) { fail("close during work"); return; }
    QThreadPool::globalInstance()->waitForDone(); QTest::qWait(30);
    for (auto window : QGuiApplication::allWindows()) if (qobject_cast<OcrTextWindow *>(window)) { fail("unexpected result window"); return; }
    qInfo() << "Inline OCR word selection, order, explicit text/image copy, DPR/zoom/pan, keyboard and cancellation tests passed";
    application.exit(0);
}


// Real local media/UI export probe. Input is an explicitly supplied synthetic
// fixture; this test never records a desktop or publishes personal media.
static void runVideoClipChecks(QApplication &application, ViewerWindow *viewer, const QString &destination)
{
    const auto fail = [&](const char *step) { qCritical() << "Video clip UI failure:" << step; application.exit(253); };
    const auto source = QUrl::fromLocalFile(qEnvironmentVariable("NKSHOT_TEST_PLAYBACK"));
    QFile originalFile(source.toLocalFile());
    if (!originalFile.open(QIODevice::ReadOnly)) { fail("source file"); return; }
    const auto originalBytes = originalFile.readAll(); originalFile.close();
    QApplication::clipboard()->setText(QStringLiteral("clip export does not copy"));
    const auto originalCurrentVideo = SpectacleCore::instance()->currentVideo();
    auto document = SpectacleCore::instance()->annotationDocument();
    const auto originalImage = document->renderToImage(); const auto originalDepth = document->undoStackDepth();
    SpectacleCore::instance()->setVideoMode(true);
    QQuickItem *recording = nullptr;
    if (!QTest::qWaitFor([&] { recording = visualItem(viewer->contentItem(), QStringLiteral("nkshotRecordingView")); return recording != nullptr; }, 3000)) { fail("recording view"); return; }
    recording->setProperty("sourceUrl", source);
    if (!QTest::qWaitFor([&] { return recording->property("hasContent").toBool(); }, 15000)) { fail("source decode"); return; }
    auto player = recording->findChild<QObject *>(QStringLiteral("nkshotMediaPlayer"));
    auto job = viewer->property("clipJob").value<QObject *>();
    auto popup = recording->findChild<QObject *>(QStringLiteral("nkshotClipPopup"));
    if (!player || !job || !popup) { fail("clip objects"); return; }
    const bool sourceAudio = player->property("hasAudio").toBool();
    const auto sourceDuration = player->property("duration").toLongLong();
    const auto click = [&](const char *name) {
        auto item = visualItem(viewer->contentItem(), QString::fromLatin1(name));
        if (!item || !item->isVisible() || !item->isEnabled()) { qWarning() << "Unavailable clip item" << name << item; return false; }
        QTest::mouseClick(viewer, Qt::LeftButton, Qt::NoModifier,
            item->mapToScene(QPointF(item->width()/2, item->height()/2)).toPoint());
        return true;
    };
    const auto open = [&] {
        if (!click("nkshotPlaybackSettings")) return false;
        QTest::qWait(100);
        if (!click("nkshotOpenClip")) return false;
        return QTest::qWaitFor([&] { return popup->property("opened").toBool() && !job->property("busy").toBool(); }, 15000);
    };
    const auto edit = [&](const char *name, const QString &text, int expected) {
        auto spin = visualItem(viewer->contentItem(), QString::fromLatin1(name));
        auto editor = spin ? qobject_cast<QQuickItem *>(spin->property("contentItem").value<QObject *>()) : nullptr;
        if (!editor || !editor->isEnabled()) return false;
        QTest::mouseClick(viewer, Qt::LeftButton, Qt::NoModifier,
            editor->mapToScene(QPointF(editor->width()/2, editor->height()/2)).toPoint());
        editor->forceActiveFocus(); QTest::keyClick(viewer, Qt::Key_A, Qt::ControlModifier);
        for (const auto key : text.toLatin1()) QTest::keyClick(viewer, key);
        QTest::keyClick(viewer, Qt::Key_Tab); QTest::qWait(50);
        return spin->property("value").toInt() == expected;
    };
    const bool independent = qEnvironmentVariableIsSet("NKSHOT_TEST_CLIP_INDEPENDENT");
    const int startCs = independent ? 125 : 135, endCs = independent ? 475 : 315;
    const auto range = [&] { return edit("nkshotClipEnd", QString::number(endCs/100.0, 'f', 2), endCs)
        && edit("nkshotClipStart", QString::number(startCs/100.0, 'f', 2), startCs); };
    const auto previewVisible = [&] {
        auto preview = visualItem(viewer->contentItem(), QStringLiteral("nkshotSoftwareVideoPreview"));
        auto panel = qobject_cast<QQuickItem *>(popup->property("contentItem").value<QObject *>());
        return preview && panel && preview->isVisible() && preview->height()>60
            && preview->mapToScene(QPointF(0,preview->height())).y() <= panel->mapToScene(QPointF(0,0)).y();
    };
    const auto dialog = [&]() -> QFileDialog * {
        for (auto widget : QApplication::topLevelWidgets())
            if (widget->objectName() == QStringLiteral("nkshotClipSaveDialog")) return qobject_cast<QFileDialog *>(widget);
        return nullptr;
    };
    const auto choose = [&](const QString &path) {
        if (!click("nkshotClipExport") || !QTest::qWaitFor([&] { return dialog(); }, 1000)) return false;
        auto save = dialog(); auto filename = save->findChild<QLineEdit *>(QStringLiteral("fileNameEdit"));
        if (!filename) return false;
        filename->setFocus(); filename->selectAll(); QTest::keyClicks(filename, path);
        if (!QTest::qWaitFor([&] { return save->selectedFiles() == QStringList{path}; }, 1000)) return false;
        return QMetaObject::invokeMethod(save, "accept");
    };
    const bool missingTools = qEnvironmentVariableIsSet("NKSHOT_TEST_CLIP_MISSING");
    const bool unsupportedSource = qEnvironmentVariableIsSet("NKSHOT_TEST_CLIP_UNSUPPORTED");
    if (missingTools || unsupportedSource) {
        const auto oldPath = qgetenv("PATH"); const bool hadPath = qEnvironmentVariableIsSet("PATH");
        const auto restorePath = qScopeGuard([&] { if (missingTools) { if (hadPath) qputenv("PATH", oldPath); else qunsetenv("PATH"); } });
        if (missingTools) qputenv("PATH", "/nkshot-test-no-programs");
        if (!open() || job->property("ready").toBool() || job->property("error").toString().isEmpty()) { fail("unavailable export explanation"); return; }
        auto exportButton = visualItem(viewer->contentItem(), QStringLiteral("nkshotClipExport"));
        if (!exportButton || exportButton->isEnabled() || !job->property("outputUrl").toUrl().isEmpty()
            || !viewer->grabWindow().save(destination)) { fail("unavailable export disabled"); return; }
        if (!originalFile.open(QIODevice::ReadOnly) || originalFile.readAll()!=originalBytes
            || recording->property("sourceUrl").toUrl()!=source
            || QApplication::clipboard()->text()!=QStringLiteral("clip export does not copy")
            || SpectacleCore::instance()->currentVideo()!=originalCurrentVideo
            || !samePixels(originalImage,document->renderToImage()) || originalDepth!=document->undoStackDepth()) { fail("unavailable export preserves source"); return; }
        qInfo() << "Video clip unavailable export UI passed, missing tools:" << missingTools
                << "unsupported playable source:" << unsupportedSource << job->property("error").toString();
        click("nkshotClipCancel"); application.exit(0); return;
    }
    if (qEnvironmentVariableIsSet("NKSHOT_TEST_CLIP_GIF")) {
        const auto format = [&](const QString &expected, bool requireActive = false) {
            // The accepted Save dialog may still await deleteLater after a
            // synchronous validation failure. Respect its production lock.
            if (!QTest::qWaitFor([&] {
                    auto item=visualItem(viewer->contentItem(),QStringLiteral("nkshotClipFormat"));
                    return !viewer->property("choosingClipOutput").toBool() && item && item->isVisible() && item->isEnabled();
                },1000)) return false;
            if ((requireActive && !job->property("busy").toBool()) || !click("nkshotClipFormat")) return false;
            QTest::keyClick(viewer, expected==QStringLiteral("gif") ? Qt::Key_End : Qt::Key_Home);
            QTest::keyClick(viewer, Qt::Key_Return);
            return QTest::qWaitFor([&] { return job->property("outputFormat").toString()==expected
                && !job->property("busy").toBool() && job->property("ready").toBool(); },15000);
        };
        const auto playGif = [&](const QString &output) {
        recording->setProperty("sourceUrl",QUrl::fromLocalFile(output));
        auto animated=visualItem(viewer->contentItem(),QStringLiteral("nkshotAnimatedImage"));
        if (!animated || !QTest::qWaitFor([&] { return recording->property("hasAnimatedImage").toBool()
            && !recording->property("playbackFailed").toBool() && player->property("source").toUrl().isEmpty(); },10000)) { return false; }
        const auto size=animated->property("sourceSize").toSize();
        if (size.isEmpty() || size.width()>640 || size.height()>640 || animated->property("frameCount").toInt()<2) { return false; }
        if ((independent || qEnvironmentVariableIsSet("NKSHOT_TEST_CLIP_GIF_LIMIT")) && size!=QSize(640,360)) return false;
        if (animated->property("frameCount").toInt()!=(endCs-startCs)/10) return false;
        if (recording->property("playing").toBool()) QMetaObject::invokeMethod(recording,"togglePlay");
        if (!QTest::qWaitFor([&] { return !recording->property("playing").toBool(); },1000)) { return false; }
        QTest::qWait(150); const int paused=animated->property("currentFrame").toInt(); QTest::qWait(250);
        if (animated->property("currentFrame").toInt()!=paused || !click("nkshotPlaybackToggle")
            || !QTest::qWaitFor([&] { return recording->property("playing").toBool() && animated->property("currentFrame").toInt()!=paused; },2000)) { return false; }
        int previous=animated->property("currentFrame").toInt();
        if (!QTest::qWaitFor([&] { const int current=animated->property("currentFrame").toInt(); const bool wrapped=current<previous; previous=current; return wrapped; },5000)) { return false; }
        QMetaObject::invokeMethod(recording,"togglePlay");
        if (!QTest::qWaitFor([&] { return !recording->property("playing").toBool(); },1000)) { return false; }
        if (!viewer->grabWindow().save(destination+QStringLiteral(".playback.png"))) { return false; }
        return true;
        };
        if (!independent) {
            if (!click("nkshotPlaybackSettings")) { fail("GIF playback settings"); return; }
            QTest::qWait(100);
            if (sourceAudio && !click("nkshotPlaybackMute")) { fail("GIF playback mute"); return; }
            if (!click("nkshotPlaybackSpeed")) { fail("GIF playback speed"); return; }
            QTest::keyClick(viewer,Qt::Key_End); QTest::keyClick(viewer,Qt::Key_Return); QTest::qWait(100);
            if (qAbs(player->property("playbackRate").toReal()-2)>0.01) { fail("GIF 2x preview preference"); return; }
            QTest::keyClick(viewer,Qt::Key_Escape); QTest::qWait(100);
        }
        if (!open() || !job->property("ready").toBool() || job->property("outputFormat").toString()!=QStringLiteral("mp4")
            || !format(QStringLiteral("gif"))) { fail("GIF initial format selection"); return; }
        if (independent) {
            const auto output=qEnvironmentVariable("NKSHOT_TEST_CLIP_OUTPUT",QStringLiteral("/logs/073-independent-export.gif"));
            if (QFileInfo::exists(output) || !range() || !previewVisible() || !viewer->grabWindow().save(destination)
                || !choose(output)
                || !QTest::qWaitFor([&] { return !job->property("busy").toBool() && job->property("outputUrl").toUrl()==QUrl::fromLocalFile(output); },60000)) {
                qWarning()<<job->property("error"); fail("independent real UI GIF export"); return;
            }
            QFile result(output);
            if (!result.open(QIODevice::ReadOnly)) { fail("independent GIF read"); return; }
            const auto bytes=result.readAll(); result.close();
            if (!bytes.startsWith("GIF89a") || !bytes.contains(QByteArray::fromHex("21ff0b4e45545343415045322e300301000000"))
                || bytes.size()>20*1024*1024 || !viewer->grabWindow().save(destination+QStringLiteral(".saved.png"))
                || !click("nkshotClipCancel") || !playGif(output)) { fail("independent GIF format/decode/play"); return; }
            recording->setProperty("sourceUrl",source);
            if (!originalFile.open(QIODevice::ReadOnly) || originalFile.readAll()!=originalBytes
                || QApplication::clipboard()->text()!=QStringLiteral("clip export does not copy")
                || SpectacleCore::instance()->currentVideo()!=originalCurrentVideo
                || !samePixels(originalImage,document->renderToImage()) || originalDepth!=document->undoStackDepth()) { fail("independent GIF source unchanged"); return; }
            qInfo()<<"Independent real UI GIF exported 1.25 to 4.75 seconds; Qt decoded 640x360/35 frames and looped/paused/resumed:"<<output;
            application.exit(0); return;
        }
        auto exportButton=visualItem(viewer->contentItem(),QStringLiteral("nkshotClipExport"));
        auto startField=visualItem(viewer->contentItem(),QStringLiteral("nkshotClipStart"));
        auto endField=visualItem(viewer->contentItem(),QStringLiteral("nkshotClipEnd"));
        if (!exportButton || !startField || !endField) { fail("GIF time controls"); return; }
        if (qEnvironmentVariableIsSet("NKSHOT_TEST_CLIP_GIF_LIMIT")) {
            if (sourceDuration<16000 || startField->property("value").toInt()!=0
                || endField->property("value").toInt()!=job->property("durationMs").toLongLong()/10
                || exportButton->isEnabled() || popup->property("validRange").toBool()
                || !viewer->grabWindow().save(destination+QStringLiteral(".initial-limit.png"))) { fail("full-source GIF limit preserves requested range"); return; }
        }
        if (!range() || !previewVisible()) { fail("GIF typed range and visible preview"); return; }
        // The 0.20-second GIF minimum is explained without silently changing input.
        if (!edit("nkshotClipEnd",QStringLiteral("1.45"),145) || exportButton->isEnabled()
            || popup->property("validRange").toBool() || startField->property("value").toInt()!=135
            || endField->property("value").toInt()!=145) { fail("short GIF range rejected without clamping"); return; }
        if (!viewer->grabWindow().save(destination+QStringLiteral(".limit.png")) || !range()) { fail("GIF short limit capture/recovery"); return; }
        if (qEnvironmentVariableIsSet("NKSHOT_TEST_CLIP_GIF_LIMIT")) {
            if (sourceDuration<16000 || !edit("nkshotClipStart",QStringLiteral("0.00"),0)
                || !edit("nkshotClipEnd",QStringLiteral("16.00"),1600) || exportButton->isEnabled()
                || popup->property("validRange").toBool() || endField->property("value").toInt()!=1600
                || !viewer->grabWindow().save(destination+QStringLiteral(".long-limit.png")) || !range()) { fail("long GIF range rejected without clamping"); return; }
        }
        if (!click("nkshotClipPreview")
            || !QTest::qWaitFor([&] { return recording->property("playing").toBool() && player->property("position").toLongLong()>1400; },2000)
            || !QTest::qWaitFor([&] { return !recording->property("playing").toBool(); },5000)) { fail("GIF source range preview"); return; }
        if (!viewer->grabWindow().save(destination) || !click("nkshotClipExport")
            || !QTest::qWaitFor([&] { return dialog(); },1000)) { fail("GIF Save dialog"); return; }
        QPointer<QFileDialog> save=dialog();
        if (save->defaultSuffix()!=QStringLiteral("gif") || !save->selectedNameFilter().contains(QStringLiteral("*.gif"))) { fail("GIF extension/filter"); return; }
        save->reject();
        if (!QTest::qWaitFor([&] { return !save; },1000) || job->property("busy").toBool()
            || !popup->property("opened").toBool() || endField->property("value").toInt()!=315) { fail("GIF Save cancellation"); return; }
        QTemporaryDir folder;
        const auto output=folder.filePath(QStringLiteral("clip 'quoted' name.gif"));
        if (!folder.isValid() || !choose(output)
            || !QTest::qWaitFor([&] { return !job->property("busy").toBool() && job->property("outputUrl").toUrl()==QUrl::fromLocalFile(output); },60000)) {
            qWarning()<<job->property("error"); fail("GIF real export"); return;
        }
        QFile outputFile(output);
        if (!outputFile.open(QIODevice::ReadOnly)) { fail("GIF file read"); return; }
        const auto gifBytes=outputFile.readAll(); outputFile.close();
        const auto infiniteLoop=QByteArray::fromHex("21ff0b4e45545343415045322e300301000000");
        if (!gifBytes.startsWith("GIF89a") || !gifBytes.contains(infiniteLoop) || gifBytes.size()>20*1024*1024
            || recording->property("sourceUrl").toUrl()!=source || !viewer->grabWindow().save(destination+QStringLiteral(".saved.png"))) { fail("GIF output format/size/loop/source isolation"); return; }
        // Changing output format clears the completed job's UI, not the selected range.
        if (!format(QStringLiteral("mp4")) || !job->property("outputUrl").toUrl().isEmpty()
            || !job->property("error").toString().isEmpty() || job->property("progress").toReal()!=0
            || startField->property("value").toInt()!=135 || endField->property("value").toInt()!=315
            || !format(QStringLiteral("gif"))) { fail("GIF/MP4 format transition resets status only"); return; }
        QSignalSpy exported(job,SIGNAL(exported(QUrl)));
        if (!exported.isValid() || !choose(output)
            || !QTest::qWaitFor([&] { return !job->property("busy").toBool() && !job->property("error").toString().isEmpty(); },3000)
            || !exported.isEmpty() || !outputFile.open(QIODevice::ReadOnly) || outputFile.readAll()!=gifBytes) { fail("existing GIF remains unchanged"); return; }
        outputFile.close();
        if (!format(QStringLiteral("mp4")) || !job->property("error").toString().isEmpty()
            || !job->property("outputUrl").toUrl().isEmpty() || !format(QStringLiteral("gif"))) { fail("format switch clears prior error"); return; }
        const auto formatCancelled=folder.filePath(QStringLiteral("format-cancelled.gif"));
        if (!choose(formatCancelled) || !job->property("busy").toBool() || !format(QStringLiteral("mp4"),true)
            || QFileInfo::exists(formatCancelled) || !job->property("outputUrl").toUrl().isEmpty()
            || !format(QStringLiteral("gif"))) { fail("format switch cancels active GIF export"); return; }
        const auto cancelled=folder.filePath(QStringLiteral("cancelled.gif"));
        if (!choose(cancelled) || !job->property("busy").toBool() || !click("nkshotClipCancel")
            || !QTest::qWaitFor([&] { return !job->property("busy").toBool(); },10000) || QFileInfo::exists(cancelled)) { fail("GIF active cancel"); return; }
        if (popup->property("opened").toBool()) { click("nkshotClipCancel"); QTest::qWait(100); }
        if (!open() || job->property("outputFormat").toString()!=QStringLiteral("mp4")
            || !format(QStringLiteral("gif")) || !range() || !click("nkshotClipCancel")) { fail("GIF cancel/reopen recovery"); return; }
        if (!playGif(output)) { fail("GIF actual decode/frame/loop/pause/resume"); return; }
        recording->setProperty("sourceUrl",source);
        if (!QTest::qWaitFor([&] { return recording->property("hasContent").toBool() && !recording->property("hasAnimatedImage").toBool()
            && player->property("source").toUrl()==source; },15000)
            || !originalFile.open(QIODevice::ReadOnly) || originalFile.readAll()!=originalBytes
            || QApplication::clipboard()->text()!=QStringLiteral("clip export does not copy")
            || SpectacleCore::instance()->currentVideo()!=originalCurrentVideo
            || !samePixels(originalImage,document->renderToImage()) || originalDepth!=document->undoStackDepth()) { fail("GIF source/editor/clipboard unchanged"); return; }
        qInfo()<<"GIF clip UI: format/time inputs, minimum gating, visible preview, GIF Save cancel/filter, real new export, format status reset, existing-file rejection, active format-switch/cancel/reopen and actual AnimatedImage frames/loop/pause/resume passed; maximum checked:" << qEnvironmentVariableIsSet("NKSHOT_TEST_CLIP_GIF_LIMIT");
        application.exit(0); return;
    }
    if (independent) {
        const auto output = qEnvironmentVariable("NKSHOT_TEST_CLIP_OUTPUT", QStringLiteral("/logs/072-independent-export.mp4"));
        if (QFileInfo::exists(output)) { fail("independent output must be a new path"); return; }
        if (!open() || !job->property("ready").toBool() || !range() || !previewVisible()
            || !viewer->grabWindow().save(destination) || !choose(output)
            || !QTest::qWaitFor([&] { return !job->property("busy").toBool() && job->property("outputUrl").toUrl()==QUrl::fromLocalFile(output); }, 60000)) {
            qWarning() << job->property("error"); fail("independent real UI export"); return;
        }
        if (!viewer->grabWindow().save(destination+QStringLiteral(".saved.png")) || !click("nkshotClipCancel")) { fail("independent result UI"); return; }
        recording->setProperty("sourceUrl", QUrl::fromLocalFile(output));
        if (!QTest::qWaitFor([&] { return recording->property("hasContent").toBool()
            && player->property("source").toUrl()==QUrl::fromLocalFile(output)
            && qAbs(player->property("duration").toLongLong()-3500)<=150; },15000)
            || player->property("hasAudio").toBool()!=sourceAudio || !click("nkshotPlaybackToggle")
            || !QTest::qWaitFor([&] { return player->property("position").toLongLong()>100; },3000)) { fail("independent output playback"); return; }
        QMetaObject::invokeMethod(player,"pause");
        recording->setProperty("sourceUrl",source);
        if (!originalFile.open(QIODevice::ReadOnly) || originalFile.readAll()!=originalBytes
            || QApplication::clipboard()->text()!=QStringLiteral("clip export does not copy")
            || SpectacleCore::instance()->currentVideo()!=originalCurrentVideo
            || !samePixels(originalImage,document->renderToImage()) || originalDepth!=document->undoStackDepth()) { fail("independent source unchanged"); return; }
        qInfo() << "Independent real UI clip exported 1.25 to 4.75 seconds and Qt-decoded/played:" << output;
        application.exit(0); return;
    }
    // Playback preferences are deliberately not export settings.
    if (!click("nkshotPlaybackSettings")) { fail("playback settings"); return; }
    QTest::qWait(100);
    if (sourceAudio && !click("nkshotPlaybackMute")) { fail("mute preference"); return; }
    if (!click("nkshotPlaybackSpeed")) { fail("speed preference"); return; }
    QTest::keyClick(viewer, Qt::Key_End); QTest::keyClick(viewer, Qt::Key_Return); QTest::qWait(100);
    if (qAbs(player->property("playbackRate").toReal()-2)>0.01 || !click("nkshotOpenClip")
        || !QTest::qWaitFor([&] { return popup->property("opened").toBool() && !job->property("busy").toBool(); }, 15000)
        || !job->property("ready").toBool() || job->property("durationMs").toLongLong()<4000 || !range()) { fail("open and typed range"); return; }
    QObject *titleLabel = nullptr;
    for (auto object : popup->findChildren<QObject *>()) {
        if (object->property("text").toString()==QStringLiteral("Export a clip")) { titleLabel=object; break; }
    }
    if (!titleLabel) { fail("clip title label"); return; }
    const QFontInfo titleFont(titleLabel->property("font").value<QFont>());
    qInfo() << "Actual clip title resolved font:" << titleFont.family() << titleFont.styleName()
            << "pixel size" << titleFont.pixelSize() << "weight" << titleFont.weight() << "exact match" << titleFont.exactMatch();
    if (!previewVisible()) { fail("video preview remains visible above panel"); return; }
    if (!click("nkshotClipPreview")
        || !QTest::qWaitFor([&] { return recording->property("playing").toBool() && player->property("position").toLongLong()>1400; }, 2000)
        || !QTest::qWaitFor([&] { return !recording->property("playing").toBool(); }, 5000)) { fail("bounded range preview"); return; }
    const auto stopped = player->property("position").toLongLong(); QTest::qWait(180);
    if (stopped<1250 || stopped>3300 || qAbs(player->property("position").toLongLong()-stopped)>30) { fail("preview end stability"); return; }
    if (!viewer->grabWindow().save(destination)) { fail("range capture"); return; }
    // Escape leaves the original media and reopens without stale export work.
    QTest::keyClick(viewer, Qt::Key_Escape); QTest::qWait(100);
    if (popup->property("opened").toBool() || job->property("busy").toBool() || recording->property("sourceUrl").toUrl()!=source
        || !open() || !job->property("ready").toBool() || !range()) { fail("Escape/reopen"); return; }
    if (!click("nkshotClipExport") || !QTest::qWaitFor([&] { return dialog(); }, 1000)) { fail("save dialog"); return; }
    QPointer<QFileDialog> save = dialog(); save->reject();
    if (!QTest::qWaitFor([&] { return !save; }, 1000) || job->property("busy").toBool()
        || !popup->property("opened").toBool()) { fail("save cancel preserves range"); return; }
    QTemporaryDir folder;
    const auto output = folder.filePath(QStringLiteral("clip 'quoted' name.mp4"));
    if (!folder.isValid() || !choose(output)
        || !QTest::qWaitFor([&] { return !job->property("busy").toBool() && job->property("outputUrl").toUrl()==QUrl::fromLocalFile(output); }, 60000)) {
        qWarning() << job->property("error"); fail("real clip export"); return;
    }
    if (!QFileInfo::exists(output) || QFileInfo(output).size()<100 || recording->property("sourceUrl").toUrl()!=source
        || !viewer->grabWindow().save(destination+QStringLiteral(".saved.png"))) { fail("export result/source isolation"); return; }
    // First-stage exports never replace existing files, even when selected in Save.
    QFile outputFile(output); if (!outputFile.open(QIODevice::ReadOnly)) { fail("output read"); return; }
    const auto outputBytes=outputFile.readAll(); outputFile.close();
    QSignalSpy exported(job, SIGNAL(exported(QUrl)));
    if (!exported.isValid() || !choose(output)
        || !QTest::qWaitFor([&] { return !job->property("busy").toBool() && !job->property("error").toString().isEmpty(); }, 3000)
        || !exported.isEmpty()) { fail("existing target rejected"); return; }
    if (!outputFile.open(QIODevice::ReadOnly) || outputFile.readAll()!=outputBytes) { fail("existing target unchanged"); return; } outputFile.close();
    if (!click("nkshotClipCancel") || !open() || !job->property("ready").toBool() || !range()) { fail("existing target recovery"); return; }
    // Cancel while the encoder job is active; no partial target becomes visible.
    const auto cancelled=folder.filePath(QStringLiteral("cancelled.mp4"));
    if (!choose(cancelled) || !job->property("busy").toBool() || !click("nkshotClipCancel")
        || !QTest::qWaitFor([&] { return !job->property("busy").toBool(); }, 10000) || QFileInfo::exists(cancelled)) { fail("cancel active export"); return; }
    if (popup->property("opened").toBool()) { click("nkshotClipCancel"); QTest::qWait(100); }
    if (!open() || !job->property("ready").toBool() || !range()) { fail("cancel/reopen recovery"); return; }
    const auto switched=folder.filePath(QStringLiteral("switched.mp4"));
    const auto second=folder.filePath(QStringLiteral("second source.mp4"));
    if (!QFile::copy(source.toLocalFile(),second) || !choose(switched) || !job->property("busy").toBool()) { fail("source-switch job"); return; }
    recording->setProperty("sourceUrl",QUrl::fromLocalFile(second));
    if (!QTest::qWaitFor([&] { return !job->property("busy").toBool() && !popup->property("opened").toBool(); },10000)
        || QFileInfo::exists(switched)) { fail("source switch cancels stale export"); return; }
    // Decode and play the actual newly written clip, not the source or a mock frame.
    recording->setProperty("sourceUrl",QUrl::fromLocalFile(output));
    if (!QTest::qWaitFor([&] { return recording->property("hasContent").toBool()
        && player->property("source").toUrl()==QUrl::fromLocalFile(output) && qAbs(player->property("duration").toLongLong()-1800)<=150; },15000)) { fail("output decode"); return; }
    const auto duration=player->property("duration").toLongLong();
    if (qAbs(duration-1800)>150 || player->property("hasAudio").toBool()!=sourceAudio || !click("nkshotPlaybackToggle")
        || !QTest::qWaitFor([&] { return player->property("position").toLongLong()>100; },3000)) { fail("output duration/audio/play"); return; }
    QMetaObject::invokeMethod(player,"pause");
    recording->setProperty("sourceUrl",source);
    if (!QTest::qWaitFor([&] { return recording->property("hasContent").toBool()
        && player->property("source").toUrl()==source
        && player->property("duration").toLongLong()==sourceDuration; },15000)) { fail("original player restored"); return; }
    if (!originalFile.open(QIODevice::ReadOnly) || originalFile.readAll()!=originalBytes) { fail("original file bytes unchanged"); return; }
    if (QApplication::clipboard()->text()!=QStringLiteral("clip export does not copy")) { fail("clipboard unchanged"); return; }
    if (SpectacleCore::instance()->currentVideo()!=originalCurrentVideo) { fail("current video unchanged"); return; }
    if (!samePixels(originalImage,document->renderToImage()) || originalDepth!=document->undoStackDepth()) { fail("annotation document unchanged"); return; }
    qInfo() << "Video clip UI: typed non-keyframe range, bounded preview, explicit export, Save cancel/existing target rejection, active cancellation/reopen, source epoch, output decode/audio/duration and original state checks passed";
    application.exit(0);
}

void installUiCaptureHook(QApplication &application)
{
    const QString destination = qEnvironmentVariable("NKSHOT_TEST_UI_CAPTURE");
    if (destination.isEmpty()) {
        return;
    }

    if (qEnvironmentVariableIsSet("NKSHOT_TEST_SCROLL_SESSION") || qEnvironmentVariableIsSet("NKSHOT_TEST_TRANSLATION") || qEnvironmentVariableIsSet("NKSHOT_TEST_VIDEO_CLIP") || qEnvironmentVariableIsSet("NKSHOT_TEST_PIN_WORDS") || qEnvironmentVariableIsSet("NKSHOT_TEST_CODE") || qEnvironmentVariableIsSet("NKSHOT_TEST_PIN_TEXT") || qEnvironmentVariableIsSet("NKSHOT_TEST_BEAUTIFY") || qEnvironmentVariableIsSet("NKSHOT_TEST_RESIZE") || qEnvironmentVariableIsSet("NKSHOT_TEST_OCR_SAVE") || qEnvironmentVariableIsSet("NKSHOT_TEST_PIN_SAVE") || qEnvironmentVariableIsSet("NKSHOT_TEST_COMBINE")) {
        // The harness drives widget filenames; production retains native dialogs.
        application.setAttribute(Qt::AA_DontUseNativeDialogs);
    }

    if (qEnvironmentVariableIsSet("NKSHOT_TEST_STARTUP_RTL")) {
        application.setLayoutDirection(Qt::RightToLeft);
    }
    if (qEnvironmentVariableIsSet("NKSHOT_TEST_STARTUP_LARGE_FONT")) {
        auto font = application.font();
        font.setPointSizeF(font.pointSizeF() * 1.5);
        application.setFont(font);
    }

    if (qEnvironmentVariableIsSet("NKSHOT_TEST_DARK")) {
        // Supply a dark desktop palette to the real controls in offscreen tests.
        // This tests palette adaptation without requiring a running desktop.
        QPalette palette = application.palette();
        palette.setColor(QPalette::Window, QColor(QStringLiteral("#1c1c1e")));
        palette.setColor(QPalette::Base, QColor(QStringLiteral("#242426")));
        palette.setColor(QPalette::Button, QColor(QStringLiteral("#2c2c2e")));
        for (const auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
            palette.setColor(role, QColor(QStringLiteral("#f5f5f7")));
        }
        palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#70a7ff")));
        palette.setColor(QPalette::HighlightedText, Qt::black);
        palette.setColor(QPalette::Mid, QColor(QStringLiteral("#454548")));
        palette.setColor(QPalette::Light, QColor(QStringLiteral("#3a3a3c")));
        application.setPalette(palette);
    }

    // Offscreen platforms do not supply desktop icon search paths. Restore the
    // standard data-directory lookup without substituting any image assets.
    auto iconPaths = QIcon::themeSearchPaths();
    for (const auto &dataPath : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)) {
        const auto iconPath = dataPath + QStringLiteral("/icons");
        if (QDir(iconPath).exists() && !iconPaths.contains(iconPath)) {
            iconPaths.append(iconPath);
        }
    }
    QIcon::setThemeSearchPaths(iconPaths);
    if (QIcon::themeName().isEmpty()) {
        QIcon::setThemeName(QStringLiteral("breeze"));
    }

    // This hook is compiled only in opt-in developer builds. It captures the
    // actual rendered ViewerWindow, not a mock scene or a composited preview.
    QTimer::singleShot(1000, &application, [&application, destination]() {
        QPointer<ViewerWindow> viewer = ViewerWindow::instance();
        if (!viewer || viewer->status() != QQuickView::Ready) {
            qCritical() << "NkShot UI capture: no ready viewer";
            application.exit(2);
            return;
        }
        if (qEnvironmentVariableIsSet("NKSHOT_TEST_EDITOR_MATERIAL")) {
            auto backdrop = viewer->rootObject()->findChild<QObject *>(QStringLiteral("nkshotEditorBackdrop"));
            auto material = viewer->rootObject()->findChild<QObject *>(QStringLiteral("nkshotEditorMaterial"));
            QVariant excludesCapture;
            if (!backdrop || !material || !QMetaObject::invokeMethod(backdrop, "sourceExcludesCapture", Q_RETURN_ARG(QVariant, excludesCapture))
                || !excludesCapture.toBool()) { application.exit(61); return; }
            qInfo() << "NkShot editor backdrop excludes consumers; active/sampling"
                    << backdrop->property("active") << material->property("sampling");
            if (qEnvironmentVariable("QT_QUICK_BACKEND") == QStringLiteral("software")
                && (backdrop->property("active").toBool() || material->property("sampling").toBool())) {
                application.exit(62); return;
            }
        }
        if (qEnvironmentVariableIsSet("NKSHOT_TEST_CAPTURE_OVERLAY")) {
            // Construct the real selection window with the already loaded,
            // synthetic document. This does not read/capture the desktop.
            auto document = SpectacleCore::instance()->annotationDocument();
            const auto screenSize = application.primaryScreen()->geometry().size();
            document->setBaseImage(document->renderToImage().scaled(screenSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
            document->tool()->setType(AnnotationTool::NoTool);
            auto capture = std::shared_ptr<CaptureWindow>(CaptureWindow::makeUnique(
                CaptureWindow::Image, application.primaryScreen(), viewer->engine()));
            viewer->hide();
            capture->show();
            SelectionEditor::instance()->reset();
            SelectionEditor::instance()->selection()->setRect({120, 100, 480, 320});
            QTimer::singleShot(1000, &application, [&application, capture, destination]() {
                if (qEnvironmentVariableIsSet("NKSHOT_TEST_REGION_DRAG")) {
                    SelectionEditor::instance()->selection()->setRect({});
                    QTest::mousePress(capture.get(), Qt::LeftButton, Qt::NoModifier, QPoint(80, 100));
                    QTest::mouseMove(capture.get(), QPoint(480, 400), 30);
                    QTest::mouseRelease(capture.get(), Qt::LeftButton, Qt::NoModifier, QPoint(480, 400));
                    const auto rect = SelectionEditor::instance()->selection()->rectF();
                    if (qAbs(rect.x() - 80) > 1 || qAbs(rect.y() - 100) > 1
                        || qAbs(rect.width() - 400) > 1 || qAbs(rect.height() - 300) > 1) {
                        qCritical() << "NkShot region drag mismatch" << rect;
                        application.exit(17);
                        return;
                    }
                    QTest::qWait(400);
                    qInfo() << "NkShot actual selection-window mouse-drag regression passed" << rect;
                }
                if (qEnvironmentVariableIsSet("NKSHOT_TEST_CAPTURE_MORE")) {
                    auto menu = capture->rootObject()->findChild<QObject *>(QStringLiteral("nkshotCaptureMoreMenu"));
                    if (!menu || !QMetaObject::invokeMethod(menu, "open")) {
                        application.exit(18);
                        return;
                    }
                    QTest::qWait(300);
                }
                for (const auto &name : {QStringLiteral("nkshotCaptureDone"), QStringLiteral("nkshotCaptureCancel")}) {
                    auto action = visualItem(capture->contentItem(), name);
                    if (!action || !action->isVisible() || action->width() < 20 || action->height() < 20
                        || !action->parentItem() || action->parentItem()->width() < 200 || action->parentItem()->height() < 30) {
                        qCritical() << "NkShot capture finalizer missing or clipped" << name;
                        application.exit(70); return;
                    }
                    const auto bounds = action->mapRectToScene(QRectF(0, 0, action->width(), action->height()));
                    if (!QRectF(0, 0, capture->width(), capture->height()).contains(bounds)) { application.exit(71); return; }
                }
                const auto pixels = capture->grabWindow();
                const bool saved = !pixels.isNull() && pixels.save(destination);
                qInfo() << "NkShot actual CaptureOverlay with synthetic input:" << destination << pixels.size();
                application.exit(saved ? 0 : 16);
            });
            return;
        }
        viewer->setAnnotating(qEnvironmentVariableIsSet("NKSHOT_TEST_EDITING"));
        const auto testTool = qEnvironmentVariable("NKSHOT_TEST_TOOL");
        if (testTool == QStringLiteral("text")) {
            SpectacleCore::instance()->annotationDocument()->tool()->setType(AnnotationTool::TextTool);
        } else if (testTool == QStringLiteral("rectangle")) {
            SpectacleCore::instance()->annotationDocument()->tool()->setType(AnnotationTool::RectangleTool);
        } else if (testTool == QStringLiteral("blur")) {
            SpectacleCore::instance()->annotationDocument()->tool()->setType(AnnotationTool::BlurTool);
        }
        if (qEnvironmentVariableIsSet("NKSHOT_TEST_PROPERTY_TRANSACTION")) {
            auto document = SpectacleCore::instance()->annotationDocument();
            document->tool()->setType(AnnotationTool::RectangleTool);
            document->tool()->setStrokeWidth(4);
            document->tool()->setStrokeColor(QColor(QStringLiteral("#ff7043")));
            document->beginItem({100, 100});
            document->continueItem({340, 250});
            document->finishItem();
            document->tool()->setType(AnnotationTool::SelectTool);
            document->selectItem({90, 90, 270, 180});
        }
        const auto dimensions = qEnvironmentVariable("NKSHOT_TEST_UI_SIZE", qEnvironmentVariableIsSet("NKSHOT_TEST_STARTUP") ? QStringLiteral("480x440") : QStringLiteral("960x660")).split(u'x');
        if (dimensions.size() == 2) {
            const int width = dimensions[0].toInt();
            const int height = dimensions[1].toInt();
            if (width >= 320 && width <= 3840 && height >= 240 && height <= 2160) {
                viewer->resize(width, height);
            }
        }
        if (qEnvironmentVariableIsSet("NKSHOT_TEST_MORE") || qEnvironmentVariableIsSet("NKSHOT_TEST_MORE_TOOLS")
            || qEnvironmentVariableIsSet("NKSHOT_TEST_ZOOM") || qEnvironmentVariableIsSet("NKSHOT_TEST_PROPERTIES")) {
            const auto menuName = qEnvironmentVariableIsSet("NKSHOT_TEST_PROPERTIES") ? QStringLiteral("nkshotPropertiesPopup")
                : qEnvironmentVariableIsSet("NKSHOT_TEST_ZOOM") ? QStringLiteral("nkshotZoomPopup")
                : qEnvironmentVariableIsSet("NKSHOT_TEST_MORE_TOOLS")
                ? QStringLiteral("nkshotExtraToolsMenu") : QStringLiteral("nkshotMoreMenu");
            auto menu = viewer->rootObject()->findChild<QObject *>(menuName);
            if (!menu || !QMetaObject::invokeMethod(menu, "open")) {
                qCritical() << "NkShot UI capture: More menu unavailable";
                application.exit(5);
                return;
            }
        }
        QTimer::singleShot(500, &application, [&application, viewer, destination]() {
            if (!viewer) {
                application.exit(3);
                return;
            }
            if (qEnvironmentVariableIsSet("NKSHOT_TEST_SCROLL_SESSION")) {
                QString failure; const bool ok=ScrollSessionUiChecks::run(application,destination,&failure);
                qInfo()<<"Controlled scroll session UI"<<ok<<failure;
                application.exit(ok ? 0 : 230);return;
            }
            if (qEnvironmentVariableIsSet("NKSHOT_TEST_TRANSLATION")) {
                QString failure; const bool ok=TranslationUiChecks::run(application,destination,&failure);
                qInfo()<<"Translation UI"<<ok<<failure;
                application.exit(ok ? 0 : 228);return;
            }
            if (qEnvironmentVariableIsSet("NKSHOT_TEST_LOCAL_MEDIA_OPEN")) {
                QString failure; const bool ok=LocalMediaOpenUiChecks::run(application,destination,&failure);
                qInfo()<<"Local media Open UI"<<ok<<failure;
                application.exit(ok ? 0 : 235); return;
            }
            if (qEnvironmentVariableIsSet("NKSHOT_TEST_RECORDING_CONTROLS") || qEnvironmentVariableIsSet("NKSHOT_TEST_RECORDING_PAUSE")) {
                QString failure; const bool ok=RecordingControlsUiChecks::run(application,destination,&failure);
                qInfo()<<"Recording controls UI"<<ok<<failure;
                application.exit(ok?0:227); return;
            }
            if (qEnvironmentVariableIsSet("NKSHOT_TEST_RECORDING_ADAPTER")) {
                QJsonObject report; const bool ok=runRecordingAdapterChecks(&report);
                qInfo().noquote()<<QJsonDocument(report).toJson(QJsonDocument::Compact);
                if (ok && qEnvironmentVariableIsSet("NKSHOT_TEST_RECORDING_CLEANUP_WARNING")) {
                    const auto core = SpectacleCore::instance();
                    const auto platform = core->videoPlatform();
                    const auto warning = report.value(QStringLiteral("cleanupWarning")).toString();
                    const auto phase = platform->preparation()->phase();
                    const auto source = core->annotationDocument()->baseImage();
                    const auto depth = core->annotationDocument()->undoStackDepth();
                    const auto video = core->currentVideo();
                    const bool videoMode = core->videoMode();
                    const QString clipboard = application.clipboard()->text();
                    auto messages = InlineMessageModel::instance();
                    const int before = messages->rowCount();
                    if (warning.isEmpty() || platform->supportedRecordingModes() != VideoPlatform::NoRecordingModes
                        || phase != NkShot::RecordingPreparation::Idle) {
                        qCritical() << "Cleanup warning bridge requires real warning and idle Null platform";
                        application.exit(223); return;
                    }
                    // Test-only signal handoff: the message came from real adapter
                    // cleanup refusal above. No native capture or capability override.
                    Q_EMIT platform->temporaryOutputCleanupWarning(warning);
                    const auto hasWarning = [&] {
                        for (int row = before; row < messages->rowCount(); ++row) {
                            const auto index = messages->index(row,0);
                            if (messages->data(index, InlineMessageModel::TypeRole).toInt() == InlineMessageModel::Warning
                                && messages->data(index, Qt::DisplayRole).toString() == warning) return true;
                        }
                        return false;
                    };
                    if (!QTest::qWaitFor(hasWarning,1500)) {
                        qCritical() << "Actual cleanup warning not presented by Core";
                        application.exit(224); return;
                    }
                    auto notifications = visualItem(viewer->contentItem(),QStringLiteral("nkshotNotifications"));
                    if (!notifications) notifications = visualItem(viewer->rootObject(),QStringLiteral("nkshotNotifications"));
                    if (!QTest::qWaitFor([&] { return notifications && notifications->isVisible()
                            && notifications->height() > 20 && notifications->width() > 100; },1500)) {
                        qCritical() << "Cleanup warning layout unavailable"; application.exit(225); return;
                    }
                    // InlineMessageList animates height; wait until content extent
                    // reaches the configured capped height, then request a frame.
                    if (!QTest::qWaitFor([&] {
                            const auto content = notifications->property("contentHeight").toReal();
                            const auto maximum = notifications->property("maximumHeight").toReal();
                            return qAbs(notifications->height()-qMin(content,maximum)) < 1;
                        },1500)) { application.exit(225); return; }
                    QSignalSpy frame(viewer,&QQuickWindow::frameSwapped); viewer->update();
                    if (!QTest::qWaitFor([&] { return !frame.isEmpty(); },1500)
                        || platform->preparation()->phase() != phase || core->currentVideo() != video
                        || core->videoMode() != videoMode || core->annotationDocument()->baseImage() != source
                        || core->annotationDocument()->undoStackDepth() != depth || application.clipboard()->text() != clipboard
                        || !viewer->grabWindow().save(destination)) {
                        qCritical() << "Cleanup warning changed media/state or failed screenshot";
                        application.exit(226); return;
                    }
                    qInfo() << "Synthetic adapter cleanup warning shown by actual Core/QML; no native recording";
                }
                application.exit(ok?0:221); return;
            }
            if (qEnvironmentVariableIsSet("NKSHOT_TEST_RECORDING_PREPARATION")) {
                QString failure; const bool ok=RecordingPreparationUiChecks::run(application,destination,&failure);
                qInfo()<<"Recording preparation UI"<<ok<<failure;
                application.exit(ok?0:222); return;
            }
            if (qEnvironmentVariableIsSet("NKSHOT_TEST_STARTUP")) { runStartupChecks(application, viewer, destination); return; }
            if (qEnvironmentVariableIsSet("NKSHOT_TEST_SCREEN_COLOR")) { runScreenColorChecks(application, viewer, destination); return; }
            if (qEnvironmentVariableIsSet("NKSHOT_TEST_PIN_WORD_SCROLL")) { runWordScrollChecks(application, viewer, destination); return; }
            if (qEnvironmentVariableIsSet("NKSHOT_TEST_PIN_WORDS")) { runInlineTextChecks(application, viewer, destination); return; }
            if (qEnvironmentVariableIsSet("NKSHOT_TEST_TASK_MENU")) {
                if (qEnvironmentVariableIsSet("NKSHOT_TEST_TASK_MENU_VIDEO")) SpectacleCore::instance()->setVideoMode(true);
                auto menu = viewer->rootObject()->findChild<QObject *>(QStringLiteral("nkshotMoreMenu"));
                if (!menu || !QMetaObject::invokeMethod(menu, "open")) { application.exit(164); return; }
                QTest::qWait(400);
                const bool video = SpectacleCore::instance()->videoMode();
                const auto share = visualItem(viewer->contentItem(), QStringLiteral("nkshotTask_nkshotSharingMenu"));
                const auto text = visualItem(viewer->contentItem(), QStringLiteral("nkshotTask_nkshotOcrLanguageMenu"));
                const auto combine = visualItem(viewer->contentItem(), QStringLiteral("nkshotTask_nkshotCombineMenu"));
                if (!share || !text || !combine || !share->isVisible() || text->isVisible() == video || combine->isVisible() == video) {
                    application.exit(165); return;
                }
                if (!viewer->grabWindow().save(destination)) { application.exit(166); return; }
                qInfo() << "NkShot task-based More menu and recording-context filtering passed" << video;
                application.exit(0); return;
            }
            if (qEnvironmentVariableIsSet("NKSHOT_TEST_COLOR")) {
                auto fail = [&](const char *step) { qCritical() << "Color sampler failure:" << step; application.exit(210); };
                auto document = SpectacleCore::instance()->annotationDocument();
                const auto original = document->renderToImage();
                const auto expected = NkShot::Imaging::colorSampleSnapshot(original);
                const auto depth = document->undoStackDepth();
                QApplication::clipboard()->setText(QStringLiteral("untouched"));
                auto more=viewer->rootObject()->findChild<QObject *>(QStringLiteral("nkshotMoreMenu"));
                if (!more || !QMetaObject::invokeMethod(more,"open")) { fail("menu"); return; }
                QTest::qWait(150);
                auto action = visualItem(viewer->contentItem(), QStringLiteral("nkshotColorPickerAction"));
                if (!action || !action->isVisible()) { fail("action"); return; }
                QTest::mouseClick(viewer, Qt::LeftButton, Qt::NoModifier, action->mapToScene(QPointF(action->width()/2, action->height()/2)).toPoint());
                QTest::qWait(200);
                QPointer<ColorPickerWindow> picker;
                for (auto window : QGuiApplication::allWindows()) if (auto candidate = qobject_cast<ColorPickerWindow *>(window)) picker = candidate;
                if (!picker || picker->status() != QQuickView::Ready || picker->hasSample()
                    || QApplication::clipboard()->text() != QStringLiteral("untouched")) { fail("initial state"); return; }
                if (viewer->openColorPicker() != picker) { fail("duplicate window"); return; }
                if (qEnvironmentVariableIsSet("NKSHOT_TEST_COLOR_COMPACT")) picker->resize(400, 460);
                picker->requestActivate(); QTest::qWait(200);
                auto viewport = visualItem(picker->contentItem(), QStringLiteral("nkshotColorViewport"));
                auto hex = visualItem(picker->contentItem(), QStringLiteral("nkshotColorCopyHex"));
                auto rgb = visualItem(picker->contentItem(), QStringLiteral("nkshotColorCopyRgb"));
                if (!viewport || !hex || !rgb || hex->isEnabled() || rgb->isEnabled() || !viewport->hasActiveFocus()) { fail("focus / disabled controls"); return; }
                picker->copyHex(); picker->copyRgb();
                if (QApplication::clipboard()->text() != QStringLiteral("untouched")) { fail("empty copy"); return; }
                if (!picker->grabWindow().save(destination + QStringLiteral(".empty.png"))) { fail("initial capture"); return; }
                const QPoint scene = viewport->mapToScene(QPointF(viewport->width()*0.5, viewport->height()*0.5)).toPoint();
                const QPointF local = viewport->mapFromScene(scene);
                const auto pixel = NkShot::Imaging::colorSamplePixelAt(local, {viewport->width(), viewport->height()}, expected.size());
                QTest::mouseClick(picker, Qt::LeftButton, Qt::NoModifier, scene);
                if (!picker->hasSample() || picker->pixel() != pixel || picker->sampleColor().rgba() != expected.pixelColor(pixel).rgba()
                    || QApplication::clipboard()->text() != QStringLiteral("untouched")) { fail("mouse pixel / no implicit copy"); return; }
                picker->sampleAt(-1, -1, viewport->width(), viewport->height());
                if (picker->pixel() != pixel) { fail("letterbox leaves selection"); return; }
                QTest::keyClick(picker, Qt::Key_Right);
                if (picker->pixel() != pixel + QPoint(1, 0)) { fail("arrow key"); return; }
                auto click = [&](QQuickItem *item) { QTest::mouseClick(picker, Qt::LeftButton, Qt::NoModifier, item->mapToScene(QPointF(item->width()/2, item->height()/2)).toPoint()); };
                click(hex);
                if (QApplication::clipboard()->text() != picker->hexValue()) { fail("HEX copy"); return; }
                click(rgb);
                if (QApplication::clipboard()->text() != picker->rgbValue()) { fail("RGB copy"); return; }
                QTest::keyClick(picker, Qt::Key_C, Qt::ControlModifier);
                if (QApplication::clipboard()->text() != picker->hexValue()) { fail("copy shortcut"); return; }
                bool returnedToImage = false;
                for (int i = 0; i < 8 && !returnedToImage; ++i) {
                    QTest::keyClick(picker, Qt::Key_Tab);
                    returnedToImage = viewport->hasActiveFocus();
                }
                if (!returnedToImage) { fail("Tab returns to image"); return; }
                for (auto item : {viewport, hex, rgb}) {
                    if (!QRectF(0, 0, picker->width(), picker->height()).contains(item->mapRectToScene({0, 0, item->width(), item->height()}))) { fail("clipped controls"); return; }
                }
                QTest::qWait(100);
                if (!picker->grabWindow().save(destination)) { fail("sample capture"); return; }
                picker->setWindowStates(Qt::WindowMinimized); QTest::qWait(50);
                if (viewer->openColorPicker() != picker || picker->windowStates().testFlag(Qt::WindowMinimized)) { fail("restore minimized sampler"); return; }
                const auto selected = picker->pixel();
                picker->resize(picker->width() + 60, picker->height() + 40); QTest::qWait(100);
                if (picker->pixel() != selected) { fail("resize changes sample"); return; }
                QTest::mousePress(picker, Qt::LeftButton, Qt::NoModifier, viewport->mapToScene(QPointF(viewport->width()/2, viewport->height()/2)).toPoint());
                const QPoint dragEnd = viewport->mapToScene(QPointF(viewport->width()*0.7, viewport->height()/2)).toPoint();
                QTest::mouseMove(picker, dragEnd); QTest::mouseRelease(picker, Qt::LeftButton, Qt::NoModifier, dragEnd);
                const auto draggedPixel = NkShot::Imaging::colorSamplePixelAt(viewport->mapFromScene(dragEnd), {viewport->width(), viewport->height()}, expected.size());
                if (picker->pixel() != draggedPixel) { fail("drag"); return; }
                picker->moveSample(345 - picker->pixel().x(), 300 - picker->pixel().y());
                QTest::qWait(100);
                if (!picker->grabWindow().save(destination + QStringLiteral(".edge.png"))) { fail("edge capture"); return; }
                // Capture setup must not leave a floating image sampler in the frame.
                viewer->hide(); QTest::qWait(100);
                if (picker) { fail("viewer hide cleanup"); return; }
                viewer->show();
                picker = qobject_cast<ColorPickerWindow *>(viewer->openColorPicker());
                if (!picker || picker->hasSample()) { fail("reopen"); return; }
                picker->requestActivate(); QTest::qWait(100); QTest::keyClick(picker, Qt::Key_Escape); QTest::qWait(100);
                if (picker || !samePixels(original, document->renderToImage()) || depth != document->undoStackDepth()) { fail("close / source isolation"); return; }
                const auto beforeVideoCopy=QApplication::clipboard()->text();
                SpectacleCore::instance()->setVideoMode(true);
                picker=qobject_cast<ColorPickerWindow *>(viewer->openColorPicker());
                if(!picker||picker->property("hasImage").toBool()||!picker->property("screenMode").toBool()
                    ||picker->property("screenPending").toBool()||picker->hasSample()
                    ||QApplication::clipboard()->text()!=beforeVideoCopy){fail("independent Screen in video context");return;}
                picker->close();
                if(!QTest::qWaitFor([&]{return picker.isNull();},1000)){fail("video Screen closes");return;}
                SpectacleCore::instance()->setVideoMode(false);
                qInfo() << "Color sampler actual menu, pixels, keyboard, drag, explicit copy, resize, repeated open, hide/close and source isolation passed";
                application.exit(0); return;
            }
            if (qEnvironmentVariableIsSet("NKSHOT_TEST_CODE")) {
                auto document = SpectacleCore::instance()->annotationDocument();
                const auto original = document->renderToImage();
                const auto depth = document->undoStackDepth();
                QApplication::clipboard()->setText(QStringLiteral("leave clipboard unchanged"));
                InlineMessageModel::instance()->clear();
                if (!openTaskMenu(viewer, QStringLiteral("nkshotOcrLanguageMenu"))) { application.exit(185); return; }
                auto action = visualItem(viewer->contentItem(), QStringLiteral("nkshotScanCodeAction"));
                if (!action || !action->isVisible()) { application.exit(186); return; }
                QTest::mouseClick(viewer, Qt::LeftButton, Qt::NoModifier, action->mapToScene(QPointF(action->width()/2, action->height()/2)).toPoint());
                viewer->scanImageCode(); // A rapid second request must not create another job/result.
                if (qEnvironmentVariableIsSet("NKSHOT_TEST_CODE_STALE")) { viewer->hide(); viewer->show(); }
                if (qEnvironmentVariableIsSet("NKSHOT_TEST_CODE_MAXIMIZE")) viewer->setWindowStates(Qt::WindowMaximized);
                if (!QTest::qWaitFor([&] { return !viewer->scanningCode(); }, 5000)) { application.exit(187); return; }
                QTest::qWait(200);
                QPointer<OcrTextWindow> result;
                int count = 0;
                for (auto window : QGuiApplication::allWindows()) {
                    if (window->objectName() == QStringLiteral("nkshotCodeTextWindow")) {
                        result = qobject_cast<OcrTextWindow *>(window); ++count;
                    }
                }
                if (QApplication::clipboard()->text() != QStringLiteral("leave clipboard unchanged")) { application.exit(188); return; }
                if (qEnvironmentVariableIsSet("NKSHOT_TEST_CODE_STALE")) {
                    if (result || InlineMessageModel::instance()->rowCount()) { application.exit(199); return; }
                    viewer->grabWindow().save(destination);
                } else if (qEnvironmentVariableIsSet("NKSHOT_TEST_CODE_EMPTY")) {
                    if (result || InlineMessageModel::instance()->rowCount() != 1) { application.exit(189); return; }
                    viewer->grabWindow().save(destination);
                } else {
                    const auto expected = QStringLiteral("https://example.com/notes?text=<literal>\n中文备注");
                    if (!result || count != 1 || result->recognizedText() != expected) { application.exit(190); return; }
                    if (qEnvironmentVariableIsSet("NKSHOT_TEST_CODE_COMPACT")) result->resize(360, 300);
                    auto text = visualItem(result->contentItem(), QStringLiteral("nkshotOcrText"));
                    auto copy = visualItem(result->contentItem(), QStringLiteral("nkshotOcrCopy"));
                    if (!text || !copy || text->property("textFormat").toInt() != 0) { application.exit(191); return; }
                    result->requestActivate(); QTest::qWait(200);
                    result->grabWindow().save(destination);
                    QTest::mouseClick(result, Qt::LeftButton, Qt::NoModifier, copy->mapToScene(QPointF(copy->width()/2, copy->height()/2)).toPoint());
                    if (QApplication::clipboard()->text() != expected) { application.exit(192); return; }
                    QTemporaryDir folder;
                    const auto output = folder.filePath(QStringLiteral("code.txt"));
                    auto save = visualItem(result->contentItem(), QStringLiteral("nkshotOcrSave"));
                    if (!folder.isValid() || !save) { application.exit(195); return; }
                    QTest::mouseClick(result, Qt::LeftButton, Qt::NoModifier, save->mapToScene(QPointF(save->width()/2, save->height()/2)).toPoint());
                    QTest::qWait(200);
                    QPointer<QFileDialog> dialog;
                    for (auto widget : QApplication::topLevelWidgets())
                        if (widget->objectName() == QStringLiteral("nkshotOcrSaveDialog")) dialog = qobject_cast<QFileDialog *>(widget);
                    auto filename = dialog ? dialog->findChild<QLineEdit *>(QStringLiteral("fileNameEdit")) : nullptr;
                    if (!filename) { application.exit(196); return; }
                    filename->setFocus(); filename->selectAll(); QTest::keyClicks(filename, output);
                    if (!QTest::qWaitFor([&] { return dialog->selectedFiles() == QStringList{output}; }, 1000)
                        || !QMetaObject::invokeMethod(dialog, "accept")
                        || !QTest::qWaitFor([&] { return dialog.isNull(); }, 1000)) { application.exit(197); return; }
                    QFile file(output);
                    if (!file.open(QIODevice::ReadOnly) || file.readAll() != expected.toUtf8()) { application.exit(198); return; }
                    result->close(); QTest::qWait(100);
                    if (result) { application.exit(193); return; }
                    // A completed/closed request must not block the next one.
                    viewer->scanImageCode();
                    if (!QTest::qWaitFor([&] { return !viewer->scanningCode(); }, 5000)) { application.exit(200); return; }
                    for (auto window : QGuiApplication::allWindows())
                        if (window->objectName() == QStringLiteral("nkshotCodeTextWindow")) result = qobject_cast<OcrTextWindow *>(window);
                    if (!result || result->recognizedText() != expected) { application.exit(201); return; }
                    // Closing a result must also close its outstanding Save As dialog.
                    result->saveAs(); QTest::qWait(100);
                    for (auto widget : QApplication::topLevelWidgets())
                        if (widget->objectName() == QStringLiteral("nkshotOcrSaveDialog")) dialog = qobject_cast<QFileDialog *>(widget);
                    if (!dialog) { application.exit(202); return; }
                    result->close(); QTest::qWait(100);
                    if (result || dialog) { application.exit(203); return; }
                }
                if (!samePixels(document->renderToImage(), original) || depth != document->undoStackDepth()) { application.exit(194); return; }
                qInfo() << "NkShot actual code menu/decode/plain-text/copy/no-result/deduplication/source isolation passed";
                application.exit(0); return;
            }
            if(qEnvironmentVariableIsSet("NKSHOT_TEST_QUICK_NOTE")) {
                auto document=SpectacleCore::instance()->annotationDocument();const auto original=document->renderToImage();const auto depth=document->undoStackDepth();
                if(!openTaskMenu(viewer,QStringLiteral("nkshotPinsNotesMenu"))) {application.exit(170);return;}
                auto action=visualItem(viewer->contentItem(),QStringLiteral("nkshotQuickNoteAction"));
                if(!action){application.exit(171);return;}
                QTest::mouseClick(viewer,Qt::LeftButton,Qt::NoModifier,action->mapToScene(QPointF(action->width()/2,action->height()/2)).toPoint());QTest::qWait(200);
                QuickNoteWindow *note=nullptr;for(auto w:QGuiApplication::allWindows())if(auto n=qobject_cast<QuickNoteWindow *>(w))note=n;
                if(!note || note->canExport()){application.exit(172);return;}
                if(qEnvironmentVariableIsSet("NKSHOT_TEST_NOTE_COMPACT"))note->resize(400,560);
                auto editor=visualItem(note->contentItem(),QStringLiteral("nkshotNoteEditor"));
                auto copy=visualItem(note->contentItem(),QStringLiteral("nkshotNoteCopy"));
                auto pinAction=visualItem(note->contentItem(),QStringLiteral("nkshotNotePin"));
                if(!editor || !copy || !pinAction || copy->isEnabled() || pinAction->isEnabled()){application.exit(173);return;}
                QTest::qWait(150);note->grabWindow().save(destination+QStringLiteral(".empty.png"));
                const QString text=QStringLiteral("Tomorrow’s demo\n\n截图与录制检查\n• Review the image before sharing\n• Keep high-frequency actions visible\n• 中文备注，清晰表达。");
                if (!editor->hasActiveFocus()) { application.exit(183); return; }
                QInputMethodEvent commit;commit.setCommitString(text);QCoreApplication::sendEvent(note,&commit);
                if(!QTest::qWaitFor([&]{return note->canExport() && copy->isEnabled();},2000)){application.exit(174);return;}
                auto click=[&](QQuickItem *item){QTest::mouseClick(note,Qt::LeftButton,Qt::NoModifier,item->mapToScene(QPointF(item->width()/2,item->height()/2)).toPoint());QTest::qWait(150);};
                const auto card=note->snapshot();click(copy);
                if(!samePixels(QApplication::clipboard()->image(),card)){application.exit(175);return;}
                click(pinAction);QPointer<PinnedImageWindow> pin;
                for(auto w:QGuiApplication::allWindows())if(auto p=qobject_cast<PinnedImageWindow *>(w))pin=p;
                if(!pin || !samePixels(pin->snapshot(),card)){application.exit(176);return;}
                auto style=visualItem(note->contentItem(),QStringLiteral("nkshotNoteStyle"));
                if(!style){application.exit(177);return;}note->requestActivate();click(style);QTest::keyClick(note,Qt::Key_End);QTest::keyClick(note,Qt::Key_Return);QTest::qWait(250);
                if(note->snapshot().pixelColor(0,0)!=QColor("#20232c") || !samePixels(pin->snapshot(),card)){application.exit(178);return;}
                note->grabWindow().save(destination+QStringLiteral(".night.png"));
                note->close();QTest::qWait(100);if(note->isVisible() || viewer->openQuickNote()!=note || editor->property("text").toString()!=text){application.exit(179);return;}
                editor->setProperty("text",QString(2001,u'x'));QTest::qWait(250);
                if(note->canExport() || copy->isEnabled() || note->error().isEmpty()){application.exit(180);return;}
                note->grabWindow().save(destination+QStringLiteral(".limit.png"));
                editor->setProperty("text",text);QTest::qWait(200);click(style);QTest::keyClick(note,Qt::Key_Home);QTest::keyClick(note,Qt::Key_Return);QTest::qWait(250);
                for(auto item:{copy,pinAction}) {auto p=item->mapToScene(QPointF());if(p.x()<0 || p.x()+item->width()>note->width() || p.y()<0 || p.y()+item->height()>note->height()){application.exit(181);return;}}
                if(!samePixels(document->renderToImage(),original) || depth!=document->undoStackDepth() || !samePixels(pin->snapshot(),card)){application.exit(182);return;}
                note->grabWindow().save(destination);
                QuickNoteWindow::hideDraftsForCapture();
                if (note->isVisible() || viewer->openQuickNote()!=note || editor->property("text").toString()!=text) { application.exit(184); return; }
                pin->close();note->close();
                qInfo()<<"NkShot quick note actual menu/Chinese input/styles/copy/pin/reopen/limits and source isolation passed";
                application.exit(0);return;
            }
            if(qEnvironmentVariableIsSet("NKSHOT_TEST_PIN_ZOOM")) {
                auto document=SpectacleCore::instance()->annotationDocument();
                const auto original=document->renderToImage();const auto depth=document->undoStackDepth();
                QImage fixture(800,2600,QImage::Format_RGB32);fixture.fill(Qt::white);
                {QPainter p(&fixture);QFont f(QStringLiteral("DejaVu Sans"));f.setPixelSize(30);p.setFont(f);
                    for(int i=0;i<10;++i) {
                        p.fillRect(QRect(24,i*260+12,752,236),i%2?QColor("#eef1f7"):QColor("#f5f7fa"));p.setPen(QColor("#192433"));
                        p.drawText(QRect(24,i*260+20,752,200),Qt::AlignCenter,QStringLiteral("SECTION %1\nRead the full screenshot\nLocal image, original pixels").arg(i+1));
                    }
                }
                QPointer<PinnedImageWindow> pin=new PinnedImageWindow(fixture,viewer->engine());
                pin->resize(qEnvironmentVariableIsSet("NKSHOT_TEST_PIN_ZOOM_COMPACT")?QSize(260,300):QSize(480,640));pin->show();QTest::qWait(200);
                auto viewport=visualItem(pin->contentItem(),QStringLiteral("nkshotPinViewport"));
                auto click=[&](const char *name){auto item=visualItem(pin->contentItem(),QString::fromLatin1(name));if(!item || !item->isVisible()) return false;QTest::mouseClick(pin,Qt::LeftButton,Qt::NoModifier,item->mapToScene(QPointF(item->width()/2,item->height()/2)).toPoint());QTest::qWait(50);return true;};
                if(!viewport || !viewport->property("fitMode").toBool() || viewport->property("contentHeight").toReal()>viewport->height()+1) {application.exit(232);return;}
                pin->grabWindow().save(destination+QStringLiteral(".fit.png"));
                if(!click("nkshotPinFitWidth") || !viewport->property("widthMode").toBool() || viewport->property("contentWidth").toReal()>viewport->width()+1 || viewport->property("contentHeight").toReal()<=viewport->height()) {application.exit(243);return;}
                pin->grabWindow().save(destination+QStringLiteral(".width.png"));
                if(!click("nkshotPinActualSize") || qAbs(viewport->property("imageScale").toReal()-1)>0.001 || viewport->property("contentHeight").toReal()<2500) {application.exit(233);return;}
                const auto before=viewport->property("contentY").toReal();
                const auto center=viewport->mapToScene(QPointF(viewport->width()/2,viewport->height()/2)).toPoint();
                QTest::mousePress(pin,Qt::LeftButton,Qt::NoModifier,center);
                for(int offset=15;offset<=120;offset+=15) QTest::mouseMove(pin,center-QPoint(0,offset),30);
                QTest::mouseRelease(pin,Qt::LeftButton,Qt::NoModifier,center-QPoint(0,120));QTest::qWait(400);
                if(viewport->property("contentY").toReal()<=before+20) {application.exit(234);return;}
                pin->grabWindow().save(destination);
                if(!click("nkshotPinZoomIn") || qAbs(viewport->property("imageScale").toReal()-1.25)>0.001 || !click("nkshotPinZoomOut") || qAbs(viewport->property("imageScale").toReal()-1)>0.001) {application.exit(235);return;}
                for(int i=0;i<12;++i) click("nkshotPinZoomIn");
                if(qAbs(viewport->property("imageScale").toReal()-4)>0.001) {application.exit(236);return;}
                if(!click("nkshotPinCopy") || !samePixels(QApplication::clipboard()->image(),fixture)) {application.exit(237);return;}
                if(!click("nkshotPinFit") || !viewport->property("fitMode").toBool() || qAbs(viewport->property("contentY").toReal())>1) {application.exit(238);return;}
                pin->requestActivate();QTest::qWait(100);
                QTest::keyClick(pin,Qt::Key_1,Qt::ControlModifier);QTest::qWait(100);
                if(qAbs(viewport->property("imageScale").toReal()-1)>0.001) {application.exit(240);return;}
                QWheelEvent wheel(QPointF(center),pin->mapToGlobal(center),QPoint(),QPoint(0,120),Qt::NoButton,Qt::ControlModifier,Qt::NoScrollPhase,false);
                QCoreApplication::sendEvent(pin,&wheel);QTest::qWait(100);
                if(qAbs(viewport->property("imageScale").toReal()-1.2)>0.001) {application.exit(241);return;}
                QTest::keyClick(pin,Qt::Key_0,Qt::ControlModifier);QTest::qWait(100);
                if(!viewport->property("fitMode").toBool()) {application.exit(242);return;}
                pin->resize(300,400);QTest::qWait(100);
                if(viewport->property("contentHeight").toReal()>viewport->height()+1 || !samePixels(pin->snapshot(),fixture) || !samePixels(document->renderToImage(),original) || depth!=document->undoStackDepth()) {application.exit(239);return;}
                pin->close();qInfo()<<"NkShot pin actual Fit/width/100%/drag/keyboard/wheel/zoom bounds/full-resolution copy/resize/source isolation passed";
                application.exit(0);return;
            }
            if(qEnvironmentVariableIsSet("NKSHOT_TEST_PIN_TEXT")) {
                const auto configured=Settings::ocrLanguages();
                const auto restore=qScopeGuard([configured]{Settings::setOcrLanguages(configured);});
                const bool chinese=qEnvironmentVariableIsSet("NKSHOT_TEST_PIN_TEXT_CHINESE");
                Settings::setOcrLanguages({chinese?QStringLiteral("chi_sim"):QStringLiteral("eng")});
                if(qEnvironmentVariableIsSet("NKSHOT_TEST_PIN_TEXT_MISSING")) Settings::setOcrLanguages({QStringLiteral("nkshot_missing_model")});
                const auto selected=Settings::ocrLanguages();
                auto document=SpectacleCore::instance()->annotationDocument();
                const auto original=document->renderToImage();const auto depth=document->undoStackDepth();
                const bool region=qEnvironmentVariableIsSet("NKSHOT_TEST_PIN_AREA");
                QImage fixture(1000,region?440:220,QImage::Format_ARGB32);fixture.fill(Qt::white);
                if(region) fixture.setDevicePixelRatio(2.0);
                {QPainter p(&fixture);QFont f(chinese?QStringLiteral("Noto Sans CJK SC"):QStringLiteral("DejaVu Sans"));f.setPixelSize(64);p.setFont(f);p.setPen(Qt::black);p.scale(region?0.5:1,region?0.5:1);p.drawText(QRect(0,0,1000,220),Qt::AlignCenter,chinese?QStringLiteral("中文截图编辑 12345"):QStringLiteral("HELLO NKSHOT 12345"));}
                if(region) { QPainter p(&fixture);p.scale(0.5,0.5);QFont f(QStringLiteral("DejaVu Sans"));f.setPixelSize(64);p.setFont(f);p.setPen(Qt::black);p.drawText(QRect(0,240,1000,180),Qt::AlignCenter,QStringLiteral("EXCLUDED LINE")); }
                if(qEnvironmentVariableIsSet("NKSHOT_TEST_PIN_TEXT_BLANK")) fixture.fill(Qt::white);
                QPointer<PinnedImageWindow> pin=new PinnedImageWindow(fixture,viewer->engine());pin->show();
                QSignalSpy extractionStatus(pin,&PinnedImageWindow::textExtractionStatus);
                if(qEnvironmentVariableIsSet("NKSHOT_TEST_PIN_TEXT_COMPACT")) pin->resize(260,region?180:220);
                QTest::qWait(150);
                auto extract=visualItem(pin->contentItem(),QStringLiteral("nkshotPinExtractText"));
                if(!extract || !extract->isVisible()) {application.exit(210);return;}
                for(const auto &name:{"nkshotPinTop","nkshotPinCopy","nkshotPinExtractText","nkshotPinSave","nkshotPinClose"}) {
                    auto action=visualItem(pin->contentItem(),QString::fromLatin1(name));
                    const auto topLeft=action?action->mapToScene(QPointF()):QPointF(-1,-1);
                    if(!action || !action->isVisible() || topLeft.x()<0 || topLeft.x()+action->width()>pin->width() || topLeft.y()<0 || topLeft.y()+action->height()>pin->height()) {application.exit(231);return;}
                }
                QApplication::clipboard()->setText(QStringLiteral("keep clipboard"));
                QTest::mouseClick(pin,Qt::LeftButton,Qt::NoModifier,extract->mapToScene(QPointF(extract->width()/2,extract->height()/2)).toPoint());
                QTest::qWait(100);
                const auto clickPin = [&](const char *name) {
                    auto item=visualItem(pin->contentItem(),QString::fromLatin1(name));
                    if(!item || !item->isVisible()) return false;
                    QTest::mouseClick(pin,Qt::LeftButton,Qt::NoModifier,item->mapToScene(QPointF(item->width()/2,item->height()/2)).toPoint());
                    if(QByteArray(name)!="nkshotPinReadArea" && QByteArray(name)!="nkshotPinReadWhole") QTest::qWait(60);
                    return true;
                };
                if(region) {
                    pin->extractTextArea(QRect());pin->extractTextArea(QRect(-100,-100,20,20));pin->extractTextArea(QRect(10000,10000,20,20));pin->extractTextArea(QRect(0,0,1,100));
                    if(pin->extractingText()) {application.exit(248);return;}
                    if(!clickPin("nkshotPinChooseArea") || !pin->rootObject()->property("selectingArea").toBool() || pin->extractingText()) {application.exit(241);return;}
                    auto readArea=visualItem(pin->contentItem(),QStringLiteral("nkshotPinReadArea"));
                    if(!readArea || readArea->isEnabled()) {application.exit(242);return;}
                    pin->requestActivate();QTest::qWait(50);QTest::keyClick(pin,Qt::Key_Escape);QTest::qWait(60);
                    if(!pin || !pin->isVisible() || pin->rootObject()->property("selectingArea").toBool()) {application.exit(243);return;}
                    clickPin("nkshotPinExtractText");clickPin("nkshotPinChooseArea");clickPin("nkshotPinCancelArea");
                    if(pin->rootObject()->property("selectingArea").toBool()) {application.exit(244);return;}
                    // Zoom before selecting; DPR=2 must not divide physical crop bounds.
                    clickPin("nkshotPinActualSize");
                    clickPin("nkshotPinExtractText");clickPin("nkshotPinChooseArea");
                    auto viewportItem=visualItem(pin->contentItem(),QStringLiteral("nkshotPinViewport"));
                    auto zoomSnapshot=visualItem(pin->contentItem(),QStringLiteral("nkshotPinSnapshot"));
                    viewportItem->setProperty("contentX",100);viewportItem->setProperty("contentY",80);QTest::qWait(60);
                    const auto panStart=viewportItem->mapToScene(QPointF(10,10)).toPoint();
                    const auto panEnd=viewportItem->mapToScene(QPointF(70,35)).toPoint();
                    const auto physicalStart=zoomSnapshot->mapFromScene(panStart);
                    QTest::mousePress(pin,Qt::LeftButton,Qt::NoModifier,panStart);QTest::mouseMove(pin,panEnd,60);QTest::mouseRelease(pin,Qt::LeftButton,Qt::NoModifier,panEnd);QTest::qWait(60);
                    const auto pannedArea=pin->rootObject()->property("selectedArea").toRect();
                    if(viewportItem->property("contentX").toReal()<=0 || viewportItem->property("contentY").toReal()<=0 || qAbs(pannedArea.x()-physicalStart.x())>1 || qAbs(pannedArea.y()-physicalStart.y())>1 || pannedArea.width()!=60 || pannedArea.height()!=25) {qWarning()<<"pan mapping"<<pannedArea<<physicalStart;application.exit(253);return;}
                    pin->hide();QTest::qWait(50);pin->show();pin->requestActivate();QTest::qWait(60);
                    if(pin->rootObject()->property("selectingArea").toBool() || pin->extractingText()) {application.exit(254);return;}
                    clickPin("nkshotPinFit");clickPin("nkshotPinExtractText");clickPin("nkshotPinChooseArea");
                    auto snapshot=visualItem(pin->contentItem(),QStringLiteral("nkshotPinSnapshot"));
                    const qreal scale=snapshot->width()/1000.0;
                    const auto start=snapshot->mapToScene(QPointF(10*scale,10*scale)).toPoint();
                    const auto end=snapshot->mapToScene(QPointF(990*scale,210*scale)).toPoint();
                    const auto outside=snapshot->mapToScene(QPointF(snapshot->width()+100,snapshot->height()+100)).toPoint();
                    QTest::mousePress(pin,Qt::LeftButton,Qt::NoModifier,start);QTest::mouseMove(pin,outside,80);QTest::mouseRelease(pin,Qt::LeftButton,Qt::NoModifier,outside);QTest::qWait(60);
                    const auto clamped=pin->rootObject()->property("selectedArea").toRect();
                    if(clamped.right()!=999 || clamped.bottom()!=439) {application.exit(252);return;}
                    // Reverse-direction selection must produce the same physical bounds.
                    QTest::mousePress(pin,Qt::LeftButton,Qt::NoModifier,end);QTest::mouseMove(pin,start,80);QTest::mouseRelease(pin,Qt::LeftButton,Qt::NoModifier,start);QTest::qWait(100);
                    const auto area=pin->rootObject()->property("selectedArea").toRect();
                    if(qAbs(area.x()-10)>3 || qAbs(area.y()-10)>3 || qAbs(area.width()-980)>4 || qAbs(area.height()-200)>4 || !readArea->isEnabled() || pin->extractingText() || QApplication::clipboard()->text()!=QStringLiteral("keep clipboard")) {qWarning()<<"region mismatch"<<area;application.exit(245);return;}
                    QTest::keyClick(pin,Qt::Key_Plus,Qt::ControlModifier);QTest::qWait(80);
                    if(pin->rootObject()->property("selectedArea").toRect()!=area) {application.exit(249);return;}
                    QTest::keyClick(pin,Qt::Key_0,Qt::ControlModifier);QTest::qWait(80);
                    pin->grabWindow().save(destination+QStringLiteral(".selected.png"));
                    QTest::keyClick(pin,Qt::Key_C,Qt::ControlModifier);
                    if(!samePixels(QApplication::clipboard()->image(),fixture)) {application.exit(246);return;}
                    QApplication::clipboard()->setText(QStringLiteral("keep clipboard"));
                    clickPin("nkshotPinReadArea");
                } else if(!clickPin("nkshotPinReadWhole")) {application.exit(247);return;}
                if(!pin->extractingText()) {application.exit(211);return;}
                if(qEnvironmentVariableIsSet("NKSHOT_TEST_PIN_TEXT_MISSING") || qEnvironmentVariableIsSet("NKSHOT_TEST_PIN_TEXT_BLANK")) {
                    if(!QTest::qWaitFor([&]{return !pin->extractingText();},15000)) {application.exit(228);return;}
                    for(auto w:QGuiApplication::allWindows()) if(qobject_cast<OcrTextWindow *>(w)) {application.exit(229);return;}
                    const auto message=extractionStatus.isEmpty()?QString():extractionStatus.last().first().toString();
                    const bool missing=qEnvironmentVariableIsSet("NKSHOT_TEST_PIN_TEXT_MISSING");
                    if(!message.contains(missing?QStringLiteral("unavailable"):QStringLiteral("No text")) || QApplication::clipboard()->text()!=QStringLiteral("keep clipboard") || !samePixels(pin->snapshot(),fixture) || !samePixels(document->renderToImage(),original) || depth!=document->undoStackDepth() || Settings::ocrLanguages()!=selected) {application.exit(230);return;}
                    QTest::qWait(150);pin->grabWindow().save(destination);pin->close();
                    qInfo()<<"NkShot pin text empty/missing model UI boundary passed";
                    application.exit(0);return;
                }
                pin->extractText(); // Repeated action must not create a second job/result.
                QPointer<OcrTextWindow> result;
                if(!QTest::qWaitFor([&]{for(auto w:QGuiApplication::allWindows()) if(auto candidate=qobject_cast<OcrTextWindow *>(w)) result=candidate;return result && !pin->extractingText();},15000)) {application.exit(212);return;}
                auto normalized=result->recognizedText().simplified();if(chinese) normalized.remove(u' ');
                if(normalized!=(chinese?QStringLiteral("中文截图编辑12345"):QStringLiteral("HELLO NKSHOT 12345")) || QApplication::clipboard()->text()!=QStringLiteral("keep clipboard") || Settings::ocrLanguages()!=selected || !samePixels(pin->snapshot(),fixture) || !samePixels(document->renderToImage(),original) || depth!=document->undoStackDepth()) {application.exit(213);return;}
                int count=0;for(auto w:QGuiApplication::allWindows()) if(qobject_cast<OcrTextWindow *>(w)) ++count;
                if(count!=1) {application.exit(214);return;}
                QTest::qWait(150);pin->grabWindow().save(destination+QStringLiteral(".pin.png"));result->grabWindow().save(destination);
                auto copy=visualItem(result->contentItem(),QStringLiteral("nkshotOcrCopy"));
                auto save=visualItem(result->contentItem(),QStringLiteral("nkshotOcrSave"));
                if(!copy || !save) {application.exit(215);return;}
                QTest::mouseClick(result,Qt::LeftButton,Qt::NoModifier,copy->mapToScene(QPointF(copy->width()/2,copy->height()/2)).toPoint());
                if(QApplication::clipboard()->text()!=result->recognizedText()) {application.exit(216);return;}
                QTest::mouseClick(result,Qt::LeftButton,Qt::NoModifier,save->mapToScene(QPointF(save->width()/2,save->height()/2)).toPoint());QTest::qWait(150);
                QFileDialog *dialog=nullptr;for(auto w:QApplication::topLevelWidgets()) if(w->objectName()==QStringLiteral("nkshotOcrSaveDialog")) dialog=qobject_cast<QFileDialog *>(w);
                auto filename=dialog?dialog->findChild<QLineEdit *>(QStringLiteral("fileNameEdit")):nullptr;
                QTemporaryDir folder;const auto output=folder.filePath(QStringLiteral("pin-text.txt"));
                if(!filename || !folder.isValid()) {application.exit(217);return;}
                filename->setFocus();filename->selectAll();QTest::keyClicks(filename,output);
                if(!QMetaObject::invokeMethod(dialog,"accept") || !QTest::qWaitFor([&]{return QFileInfo::exists(output);},1500)) {application.exit(218);return;}
                QFile saved(output);if(!saved.open(QIODevice::ReadOnly) || saved.readAll()!=result->recognizedText().toUtf8()) {application.exit(219);return;}
                result->close();if(!QTest::qWaitFor([&]{return result.isNull();},1000)) {application.exit(220);return;}
                // Hidden pins must not open a late result over a capture selector.
                if(region) pin->extractTextArea(QRect(0,0,1000,220)); else pin->extractText();
                pin->hide();
                if(!QTest::qWaitFor([&]{return !pin->extractingText();},15000)) {application.exit(224);return;}
                for(auto w:QGuiApplication::allWindows()) if(qobject_cast<OcrTextWindow *>(w)) {application.exit(225);return;}
                pin->show();QTest::qWait(100);pin->extractText();
                if(region) {
                    // A whole-image request must never reuse pending text from a crop.
                    if(!pin->extractingText() || !QTest::qWaitFor([&]{return !pin->extractingText();},15000)) {application.exit(250);return;}
                }
                for(auto w:QGuiApplication::allWindows()) if(auto candidate=qobject_cast<OcrTextWindow *>(w)) result=candidate;
                if(!result || result->recognizedText().isEmpty() || pin->extractingText()) {application.exit(226);return;}
                if(region && !result->recognizedText().contains(QStringLiteral("EXCLUDED"))) {application.exit(251);return;}
                result->close();if(!QTest::qWaitFor([&]{return result.isNull();},1000)) {application.exit(227);return;}
                // An immediate pin close cancels cooperatively and suppresses late UI/clipboard writes.
                QApplication::clipboard()->setText(QStringLiteral("after close"));
                pin->extractText();pin->close();
                if(!QTest::qWaitFor([&]{return pin.isNull() && QThreadPool::globalInstance()->activeThreadCount()==0;},15000)) {application.exit(221);return;}
                QTest::qWait(150);
                for(auto w:QGuiApplication::allWindows()) if(qobject_cast<OcrTextWindow *>(w)) {application.exit(222);return;}
                if(QApplication::clipboard()->text()!=QStringLiteral("after close") || !samePixels(document->renderToImage(),original) || depth!=document->undoStackDepth() || Settings::ocrLanguages()!=selected) {application.exit(223);return;}
                qInfo()<<"NkShot pin local text actual action/repeat/result/copy/UTF8 save/hidden deferral/close suppression and source/clipboard/config isolation passed";
                application.exit(0);return;
            }
            if (qEnvironmentVariableIsSet("NKSHOT_TEST_SCROLL_PREVIEW")) {
                if(qEnvironmentVariableIsSet("NKSHOT_TEST_SCROLL_VIDEO")) SpectacleCore::instance()->setVideoMode(true);
                auto document=SpectacleCore::instance()->annotationDocument();
                const auto original=document->renderToImage(); const auto depth=document->undoStackDepth();
                auto more=viewer->rootObject()->findChild<QObject *>(QStringLiteral("nkshotMoreMenu"));
                if(!more || !QMetaObject::invokeMethod(more,"open")) {application.exit(189);return;}
                QTest::qWait(200);
                auto entry=visualItem(viewer->contentItem(),QStringLiteral("nkshotScrollPreviewEntry"));
                if(!entry) {application.exit(189);return;}
                if(SpectacleCore::instance()->videoMode()) {
                    if(entry->isVisible() || viewer->openScrollPreview()) {application.exit(180);return;}
                    qInfo() << "NkShot manual scroll preview remains image-only; video mode refuses opening";
                    application.exit(0);return;
                }
                if(QDBusConnection::sessionBus().isConnected()) {application.exit(181);return;}
                if(!entry->isVisible() || !entry->property("text").toString().contains(QStringLiteral("Manual"))) {application.exit(189);return;}
                if(!viewer->grabWindow().save(destination+QStringLiteral(".entry.png"))) {application.exit(189);return;}
                QApplication::clipboard()->setText(QStringLiteral("scroll preview untouched"));
                const auto pinCount=[] {int count=0;for(auto w:QGuiApplication::allWindows()) if(qobject_cast<PinnedImageWindow *>(w)) ++count;return count;};
                const int originalPins=pinCount();
                QTest::mouseClick(viewer,Qt::LeftButton,Qt::NoModifier,entry->mapToScene(QPointF(entry->width()/2,entry->height()/2)).toPoint());
                QPointer<ScrollCaptureWindow> preview;
                if(!QTest::qWaitFor([&]{for(auto w:QGuiApplication::allWindows()) if(auto candidate=qobject_cast<ScrollCaptureWindow *>(w)) preview=candidate;return !preview.isNull();},1000)) {application.exit(182);return;}
                if(!preview || preview->status()!=QQuickView::Ready || preview->available() || viewer->openScrollPreview()!=preview) {application.exit(182);return;}
                if(qEnvironmentVariableIsSet("NKSHOT_TEST_SCROLL_COMPACT")) preview->resize(360,260);
                QTest::qWait(250);
                auto primary=visualItem(preview->contentItem(),QStringLiteral("nkshotScrollPrimary"));
                if(!primary || primary->isEnabled() || !preview->statusText().contains(QStringLiteral("API 5"))
                    || !preview->statusText().contains(QStringLiteral("Unavailable"))) {application.exit(183);return;}
                QTest::mouseClick(preview,Qt::LeftButton,Qt::NoModifier,primary->mapToScene(QPointF(primary->width()/2,primary->height()/2)).toPoint());
                preview->selectRegion();
                preview->captureNext();
                preview->finishCapture();
                QTest::qWait(50);
                if(preview->phase()!=0 || preview->frameCount()!=0 || preview->busy() || preview->canFinish()
                    || pinCount()!=originalPins || QApplication::clipboard()->text()!=QStringLiteral("scroll preview untouched")
                    || !samePixels(original,document->renderToImage()) || depth!=document->undoStackDepth()) {application.exit(184);return;}
                if(!preview->grabWindow().save(destination)) {application.exit(184);return;}
                // Visual selector-only exercise: no capability bypass, C++ phase
                // stays Setup and reject paths cannot accept a region or frame.
                if(qEnvironmentVariableIsSet("NKSHOT_TEST_SCROLL_SELECTOR_VISUAL")) {
                    preview->resize(640,420);preview->rootObject()->setProperty("phase",1);QTest::qWait(150);
                    auto selector=visualItem(preview->contentItem(),QStringLiteral("nkshotScrollSelectionArea"));
                    auto confirm=visualItem(preview->contentItem(),QStringLiteral("nkshotScrollConfirmRegion"));
                    if(!selector || !confirm || confirm->isEnabled()) {application.exit(190);return;}
                    QTest::keyClick(preview,Qt::Key_Right);
                    if(selector->property("selectionRect").toRectF()!=QRectF(161,105,320,210)) {qWarning()<<"Selector keyboard seed"<<selector->property("selectionRect")<<selector->hasActiveFocus()<<preview->activeFocusItem();preview->grabWindow().save(destination+QStringLiteral(".keyboard-failure.png"));application.exit(191);return;}
                    QTest::keyClick(preview,Qt::Key_Down,Qt::AltModifier);
                    QTest::keyClick(preview,Qt::Key_Left,Qt::ShiftModifier);
                    if(selector->property("selectionRect").toRectF()!=QRectF(151,105,320,211)) {application.exit(192);return;}
                    QTest::mousePress(preview,Qt::LeftButton,Qt::NoModifier,QPoint(70,120));
                    QTest::mouseMove(preview,QPoint(530,340));QTest::qWait(150);
                    preview->grabWindow().save(destination+QStringLiteral(".selector-layout.png"));
                    QTest::mouseRelease(preview,Qt::LeftButton,Qt::NoModifier,QPoint(530,340));
                    if(preview->phase()!=0 || preview->frameCount()!=0) {application.exit(185);return;}
                    QTest::keyClick(preview,Qt::Key_Right);
                    QTest::keyClick(preview,Qt::Key_Down,Qt::ShiftModifier);
                    QTest::keyClick(preview,Qt::Key_Left,Qt::AltModifier);
                    QTest::keyClick(preview,Qt::Key_Up,Qt::AltModifier|Qt::ShiftModifier);
                    if(selector->property("selectionRect").toRectF()!=QRectF(71,130,459,210) || !confirm->isEnabled()) {application.exit(193);return;}
                    QTest::qWait(150);preview->grabWindow().save(destination+QStringLiteral(".keyboard-layout.png"));
                    auto cancelRegion=visualItem(preview->contentItem(),QStringLiteral("nkshotScrollSelectorCancel"));
                    QTest::keyClick(preview,Qt::Key_Tab);
                    if(!cancelRegion || !cancelRegion->hasActiveFocus()) {application.exit(200);return;}
                    QTest::keyClick(preview,Qt::Key_Tab);
                    if(!confirm->hasActiveFocus()) {application.exit(201);return;}
                    QSignalSpy confirmed(confirm,SIGNAL(clicked()));
                    QTest::keyClick(preview,Qt::Key_Return);
                    if(confirmed.count()!=1) {application.exit(202);return;}
                    QTest::mouseClick(preview,Qt::LeftButton,Qt::NoModifier,confirm->mapToScene(QPointF(confirm->width()/2,confirm->height()/2)).toPoint());
                    selector->forceActiveFocus();
                    for(int i=0;i<100;++i) QTest::keyClick(preview,Qt::Key_Right,Qt::ShiftModifier);
                    if(selector->property("selectionRect").toRectF().right()!=640) {application.exit(194);return;}
                    for(int i=0;i<100;++i) QTest::keyClick(preview,Qt::Key_Down,Qt::AltModifier|Qt::ShiftModifier);
                    if(selector->property("selectionRect").toRectF().bottom()!=420) {application.exit(195);return;}
                    for(int i=0;i<100;++i) QTest::keyClick(preview,Qt::Key_Left,Qt::AltModifier|Qt::ShiftModifier);
                    for(int i=0;i<100;++i) QTest::keyClick(preview,Qt::Key_Up,Qt::AltModifier|Qt::ShiftModifier);
                    if(selector->property("selectionRect").toRectF().size()!=QSizeF(1,25)) {application.exit(196);return;}
                    QTest::keyClick(preview,Qt::Key_Return);
                    if(preview->phase()!=0 || preview->frameCount()!=0) {application.exit(197);return;}
                    QTest::mousePress(preview,Qt::LeftButton,Qt::NoModifier,QPoint(70,120));
                    QTest::mouseMove(preview,QPoint(90,130));QTest::mouseRelease(preview,Qt::LeftButton,Qt::NoModifier,QPoint(90,130));
                    if(confirm->isEnabled()) {application.exit(198);return;}
                    QTest::keyClick(preview,Qt::Key_Return);QTest::qWait(150);
                    preview->grabWindow().save(destination+QStringLiteral(".invalid-region.png"));
                    if(preview->phase()!=0 || preview->frameCount()!=0) {application.exit(199);return;}
                    preview->resize(360,260);preview->rootObject()->setProperty("phase",2);QTest::qWait(150);
                    preview->grabWindow().save(destination+QStringLiteral(".session-layout.png"));
                }
                if(qEnvironmentVariableIsSet("NKSHOT_TEST_SCROLL_SELECTOR_VISUAL")) {
                    preview->rootObject()->setProperty("phase",1);QTest::qWait(100);
                    auto selector=visualItem(preview->contentItem(),QStringLiteral("nkshotScrollSelectionArea"));
                    selector->forceActiveFocus();QTest::keyClick(preview,Qt::Key_Tab);QTest::keyClick(preview,Qt::Key_Return);
                } else QTest::keyClick(preview,Qt::Key_Escape);
                if(!QTest::qWaitFor([&]{return preview.isNull();},1000) || !samePixels(original,document->renderToImage()) || depth!=document->undoStackDepth()
                    || pinCount()!=originalPins || QApplication::clipboard()->text()!=QStringLiteral("scroll preview untouched")) {application.exit(186);return;}
                preview=qobject_cast<ScrollCaptureWindow *>(viewer->openScrollPreview());
                if(!preview || preview->available()) {application.exit(187);return;}
                preview->cancelPreview();
                if(!QTest::qWaitFor([&]{return preview.isNull();},1000)) {application.exit(188);return;}
                qInfo() << "NkShot default manual scroll entry, unavailable-state/reuse/rejected capture and Finish/close/reopen/source and clipboard isolation passed; selector input diagnostics are not native capture";
                application.exit(0);return;
            }
            if (qEnvironmentVariableIsSet("NKSHOT_TEST_SCROLL_BACKEND_BOUNDARY")) {
                // This branch verifies unavailable-backend isolation, never successful capture.
                if (QDBusConnection::sessionBus().isConnected()) { application.exit(171); return; }
                auto document=SpectacleCore::instance()->annotationDocument();
                const auto before=document->renderToImage(); const auto depth=document->undoStackDepth();
                ScrollFrameKWin backend;
                if (backend.request(1,nullptr,QRect(0,0,40,100)) || backend.busy()
                    || backend.request(2,viewer->screen(),QRect(0,0,40,100)) || backend.busy()) {
                    application.exit(172); return;
                }
                backend.cancel(); backend.cancel();
                if (!samePixels(before,document->renderToImage()) || depth!=document->undoStackDepth()) {
                    application.exit(173); return;
                }
                viewer->grabWindow().save(destination);
                qInfo() << "NkShot unavailable scroll backend refused requests without editor/history mutation; no live capture claim";
                application.exit(0); return;
            }
            if (qEnvironmentVariableIsSet("NKSHOT_TEST_DROP_MEDIA")) {
                QString failure;
                const bool passed = MediaDropUiChecks::run(application,viewer,destination,&failure);
                qInfo() << "108 local media drop UI" << passed << failure;
                application.exit(passed ? 0 : 240); return;
            }
            if (qEnvironmentVariableIsSet("NKSHOT_TEST_IMAGE_EXPORT_DEMO")) {
                const auto input = SpectacleCore::instance()->annotationDocument()->renderToImage();
                auto editor = new ImageCopyEditor(input, viewer->engine());
                if (editor->status() != QQuickView::Ready) { delete editor; application.exit(239); return; }
                editor->resize(qEnvironmentVariableIsSet("NKSHOT_TEST_COMPACT") ? QSize(420,520) : QSize(900,640));
                editor->show(); editor->requestActivate(); QTest::qWait(200);
                auto document = editor->document();
                document->tool()->setType(AnnotationTool::RectangleTool);
                document->tool()->setStrokeColor(QColor(QStringLiteral("#007aff")));
                document->tool()->setStrokeWidth(3); document->tool()->setFillColor(Qt::transparent); document->tool()->setShadow(false);
                document->beginItem({75,75}); document->continueItem({285,165}); document->finishItem();
                QString failure;
                const bool passed = ImageExportUiChecks::run(application,editor,destination,&failure);
                qInfo() << "107 image sharing standalone UI" << passed << failure;
                application.exit(passed ? 0 : 239); return;
            }
            if (qEnvironmentVariableIsSet("NKSHOT_TEST_RESIZE") || qEnvironmentVariableIsSet("NKSHOT_TEST_BEAUTIFY")) {
                const bool beautify = qEnvironmentVariableIsSet("NKSHOT_TEST_BEAUTIFY");
                const bool orient = !beautify && qEnvironmentVariableIsSet("NKSHOT_TEST_ORIENTATION");
                const int presentationStyle = qEnvironmentVariableIntValue("NKSHOT_TEST_PRESENTATION_STYLE");
                const bool limitTest = qEnvironmentVariableIsSet("NKSHOT_TEST_BEAUTIFY_LIMIT");
                const int presentationPadding = limitTest ? 0 : 48;
                auto document = SpectacleCore::instance()->annotationDocument();
                document->tool()->setType(AnnotationTool::RectangleTool);
                document->tool()->setStrokeColor(QColor(QStringLiteral("#007aff")));
                document->tool()->setStrokeWidth(3); document->tool()->setFillColor(Qt::transparent); document->tool()->setShadow(false);
                document->beginItem({80, 80}); document->continueItem({220, 160}); document->finishItem();
                const auto original = document->renderToImage();
                const auto depth = document->undoStackDepth();
                auto popup = viewer->rootObject()->findChild<QObject *>(beautify ? QStringLiteral("nkshotBeautifyPopup") : QStringLiteral("nkshotResizePopup"));
                auto openResize = [&] {
                    if (!popup || !openTaskMenu(viewer, QStringLiteral("nkshotSharingMenu"))) return false;
                    auto entry = visualItem(viewer->contentItem(), beautify ? QStringLiteral("nkshotBeautifyImageAction") : QStringLiteral("nkshotResizeImageAction"));
                    if (!entry) return false;
                    QTest::mouseClick(viewer, Qt::LeftButton, Qt::NoModifier,
                        entry->mapToScene(QPointF(entry->width()/2, entry->height()/2)).toPoint());
                    return QTest::qWaitFor([&] { return popup->property("opened").toBool(); }, 1000);
                };
                QObject *presentation = beautify ? viewer->property("presentationPreview").value<QObject *>() : nullptr;
                auto ensurePresentationVisible = [&](QQuickItem *item) {
                    if (!item || !item->isVisible()) return false;
                    auto scroll = visualItem(viewer->contentItem(), QStringLiteral("nkshotPresentationScroll"));
                    if (!scroll) return false;
                    for (int attempt = 0; attempt < 12; ++attempt) {
                        const QRectF bounds(scroll->mapToScene(QPointF()), scroll->size());
                        const QRectF target(item->mapToScene(QPointF()), item->size());
                        if (bounds.adjusted(0, 1, 0, -1).contains(target)) { QTest::qWait(100); return true; }
                        const bool down = target.bottom() > bounds.bottom();
                        const auto at = scroll->mapToScene(QPointF(scroll->width()/2, scroll->height()/2));
                        QWheelEvent wheel(at, viewer->mapToGlobal(at.toPoint()), {}, QPoint(0, down ? -120 : 120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
                        QCoreApplication::sendEvent(viewer, &wheel); QTest::qWait(100);
                        const QRectF after(item->mapToScene(QPointF()), item->size());
                        if (after == target) {
                            auto bar = visualItem(viewer->contentItem(), QStringLiteral("nkshotPresentationScrollbar"));
                            if (!bar || !bar->isVisible()) return false;
                            const auto from = bar->mapToScene(QPointF(bar->width()/2, (bar->property("position").toReal()+bar->property("size").toReal()/2)*bar->height())).toPoint();
                            const auto to = bar->mapToScene(QPointF(bar->width()/2, down ? bar->height()-2 : 2)).toPoint();
                            QTest::mousePress(viewer, Qt::LeftButton, Qt::NoModifier, from);
                            QTest::mouseMove(viewer, to, 100); QTest::mouseRelease(viewer, Qt::LeftButton, Qt::NoModifier, to); QTest::qWait(150);
                        }
                    }
                    return false;
                };
                auto presentationReady = [&] {
                    auto image = visualItem(viewer->contentItem(), QStringLiteral("nkshotPresentationPreview"));
                    return presentation && presentation->property("ready").toBool() && !presentation->property("imageSource").toUrl().isEmpty()
                        && image && image->isVisible() && image->property("status").toInt() == 1;
                };
                auto presentationPixels = [&](const NkShot::Imaging::PresentationOptions &options) {
                    if (!presentationReady()) return false;
                    const auto url = presentation->property("imageSource").toUrl();
                    auto provider = dynamic_cast<QQuickImageProvider *>(viewer->engine()->imageProvider(url.host()));
                    if (!provider) return false;
                    QSize size;
                    const auto actual = provider->requestImage(url.path().mid(1), &size, QSize());
                    const auto expectedPreview = NkShot::Imaging::previewPresentation(original, options);
                    return actual.devicePixelRatio() == 1 && size == expectedPreview.size() && samePixels(actual, expectedPreview);
                };
                if (!openResize()) { application.exit(140); return; }
                if (beautify && (!presentation || (!limitTest && !QTest::qWaitFor(presentationReady, 1500)))) {
                    qWarning() << "106 initial presentation preview not ready"; application.exit(167); return;
                }
                if (beautify && !limitTest && !presentationPixels({48, NkShot::Imaging::PresentationStyle(2), QString(), true})) {
                    qWarning() << "106 initial preview pixels differ"; application.exit(170); return;
                }
                if(orient) {
                    for(const auto &name:{QStringLiteral("nkshotCopyRotate"),QStringLiteral("nkshotCopyMirror")}) {
                        auto item=visualItem(viewer->contentItem(),name);
                        if(!item) {application.exit(238);return;}
                        QTest::mouseClick(viewer,Qt::LeftButton,Qt::NoModifier,item->mapToScene(QPointF(item->width()/2,item->height()/2)).toPoint());
                    }
                }
                viewer->grabWindow().save(destination + QStringLiteral(".dialog.png"));
                QTest::keyClick(viewer, Qt::Key_Escape);
                if (!QTest::qWaitFor([&] { return !popup->property("visible").toBool(); }, 1000)
                    || !samePixels(document->renderToImage(), original) || document->undoStackDepth() != depth
                    || (beautify && !presentation->property("sourceSize").toSize().isEmpty()) || !openResize()
                    || (beautify && presentation->property("sourceSize").toSize() != original.size())) {
                    application.exit(141); return;
                }
                auto create = visualItem(viewer->contentItem(), beautify ? QStringLiteral("nkshotBeautifyCreate") : QStringLiteral("nkshotResizeCreate"));
                if (!create) { application.exit(142); return; }
                if (beautify) {
                    if (limitTest) {
                        if (create->isEnabled()) { application.exit(164); return; }
                        viewer->grabWindow().save(destination + QStringLiteral(".limit.png"));
                        auto padding = visualItem(viewer->contentItem(), QStringLiteral("nkshotPresentationPadding"));
                        auto field = padding ? padding->property("contentItem").value<QQuickItem *>() : nullptr;
                        if (!field || !ensurePresentationVisible(padding)) { application.exit(165); return; }
                        QTest::mouseClick(viewer, Qt::LeftButton, Qt::NoModifier, field->mapToScene(QPointF(field->width()/2, field->height()/2)).toPoint());
                        QTest::keyClick(viewer, Qt::Key_A, Qt::ControlModifier);
                        QTest::keyClick(viewer, Qt::Key_0); QTest::keyClick(viewer, Qt::Key_Tab);
                        if (!QTest::qWaitFor([&] { return create->isEnabled() && presentationReady(); }, 1500) || padding->property("value").toInt() != 0) { application.exit(166); return; }
                    }
                    auto style = visualItem(viewer->contentItem(), QStringLiteral("nkshotPresentationStyle") + QString::number(presentationStyle));
                    auto watermark = visualItem(viewer->contentItem(), QStringLiteral("nkshotPresentationWatermark"));
                    if (!style || !watermark) { application.exit(162); return; }
                    for (int choice : { 0, 1, presentationStyle }) {
                        if (popup->property("styleIndex").toInt() == choice) continue;
                        auto option = visualItem(viewer->contentItem(), QStringLiteral("nkshotPresentationStyle") + QString::number(choice));
                        const auto beforeUrl = presentation->property("imageSource").toUrl();
                        if (!ensurePresentationVisible(option)) { qWarning() << "106 style option not reachable"; application.exit(168); return; }
                        QTest::mouseClick(viewer, Qt::LeftButton, Qt::NoModifier, option->mapToScene(QPointF(option->width()/2,option->height()/2)).toPoint());
                        if (!QTest::qWaitFor([&] { return presentationReady() && presentation->property("imageSource").toUrl() != beforeUrl; }, 1500)
                            || !samePixels(document->renderToImage(), original) || document->undoStackDepth() != depth) {
                            qWarning() << "106 style refresh or source isolation failed"; application.exit(169); return;
                        }
                    }
                    if (!ensurePresentationVisible(style)) { application.exit(168); return; }
                    QTest::mouseClick(viewer, Qt::LeftButton, Qt::NoModifier, style->mapToScene(QPointF(style->width()/2,style->height()/2)).toPoint());
                    if (!ensurePresentationVisible(watermark)) { qWarning() << "106 watermark not reachable"; application.exit(168); return; }
                    QTest::mouseClick(viewer, Qt::LeftButton, Qt::NoModifier, watermark->mapToScene(QPointF(watermark->width()/2,watermark->height()/2)).toPoint());
                    QTest::keyClick(viewer, Qt::Key_A, Qt::ControlModifier);
                    for (char c : QByteArray("NkShot demo")) QTest::keyClick(viewer, c);
                    if (watermark->property("text").toString() != QStringLiteral("NkShot demo") || !style->property("checked").toBool()) { application.exit(163); return; }
                    if (!QTest::qWaitFor([&] { return presentationPixels({presentationPadding, NkShot::Imaging::PresentationStyle(presentationStyle), QStringLiteral("NkShot demo"), true}); }, 1500)) {
                        qWarning() << "106 configured preview/provider pixels differ"; application.exit(170); return;
                    }
                    viewer->grabWindow().save(destination + QStringLiteral(".configured.png"));
                    auto previewImage = visualItem(viewer->contentItem(), QStringLiteral("nkshotPresentationPreview"));
                    if (!ensurePresentationVisible(previewImage)) { qWarning() << "106 preview not reachable"; application.exit(168); return; }
                    viewer->grabWindow().save(destination + QStringLiteral(".preview.png"));
                } else {
                    if(orient) {
                        auto rotate=visualItem(viewer->contentItem(),QStringLiteral("nkshotCopyRotate"));
                        auto mirror=visualItem(viewer->contentItem(),QStringLiteral("nkshotCopyMirror"));
                        if(!rotate||!mirror||popup->property("quarterTurns").toInt()!=0||popup->property("horizontalMirror").toBool()) {application.exit(236);return;}
                        for(int n=0;n<5;++n)QTest::mouseClick(viewer,Qt::LeftButton,Qt::NoModifier,rotate->mapToScene(QPointF(rotate->width()/2,rotate->height()/2)).toPoint());
                        for(int n=0;n<3;++n)QTest::mouseClick(viewer,Qt::LeftButton,Qt::NoModifier,mirror->mapToScene(QPointF(mirror->width()/2,mirror->height()/2)).toPoint());
                        if(popup->property("quarterTurns").toInt()!=1||!popup->property("horizontalMirror").toBool()) {application.exit(237);return;}
                    }
                    auto widthInput = visualItem(viewer->contentItem(), QStringLiteral("nkshotResizeWidth"));
                    auto field = widthInput ? widthInput->property("contentItem").value<QQuickItem *>() : nullptr;
                    if (!field) { application.exit(142); return; }
                    QTest::mouseClick(viewer, Qt::LeftButton, Qt::NoModifier, field->mapToScene(QPointF(field->width()/2, field->height()/2)).toPoint());
                    QTest::keyClick(viewer, Qt::Key_A, Qt::ControlModifier);
                    for (char c : QByteArray("320")) QTest::keyClick(viewer, c);
                    QTest::keyClick(viewer, Qt::Key_Tab);
                    if (widthInput->property("value").toInt() != 320) { application.exit(143); return; }
                    QTest::qWait(100);viewer->grabWindow().save(destination+QStringLiteral(".configured.png"));
                }
                if (beautify && !QRectF(QPointF(), viewer->size()).contains(QRectF(create->mapToScene(QPointF()), create->size()))) {
                    qWarning() << "106 fixed Create footer clipped"; application.exit(171); return;
                }
                QTest::mouseClick(viewer, Qt::LeftButton, Qt::NoModifier,
                    create->mapToScene(QPointF(create->width()/2, create->height()/2)).toPoint());
                QPointer<PinnedImageWindow> preview;
                if (!QTest::qWaitFor([&] {
                    for (auto window : QGuiApplication::allWindows()) if (auto pin = qobject_cast<PinnedImageWindow *>(window)) preview = pin;
                    return preview && !viewer->resizingImage();
                }, 5000)) { application.exit(144); return; }
                const auto expected = beautify ? NkShot::Imaging::composePresentation(original,
                    {presentationPadding, NkShot::Imaging::PresentationStyle(presentationStyle), QStringLiteral("NkShot demo"), true}) : orient ? NkShot::Imaging::orientedCopy(original,1,true,320)
                    : original.scaled(320, qMax(1, qRound(double(original.height()) * 320 / original.width())), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                if (!samePixels(preview->snapshot(), expected) || !samePixels(document->renderToImage(), original)
                    || document->undoStackDepth() != depth) { application.exit(145); return; }
                preview->resize(beautify ? QSize(720, 540) : QSize(440, 340)); QTest::qWait(200);
                auto copy = visualItem(preview->contentItem(), QStringLiteral("nkshotPinCopy"));
                auto save = visualItem(preview->contentItem(), QStringLiteral("nkshotPinSave"));
                if (!copy || !save) { application.exit(146); return; }
                QTest::mouseClick(preview, Qt::LeftButton, Qt::NoModifier, copy->mapToScene(QPointF(copy->width()/2, copy->height()/2)).toPoint());
                if (!samePixels(QApplication::clipboard()->image(), expected)) { application.exit(147); return; }
                QTest::mouseClick(preview, Qt::LeftButton, Qt::NoModifier, save->mapToScene(QPointF(save->width()/2, save->height()/2)).toPoint());
                QTest::qWait(200);
                QFileDialog *dialog = nullptr;
                for (auto widget : QApplication::topLevelWidgets()) if (widget->objectName() == QStringLiteral("nkshotPinSaveDialog")) dialog = qobject_cast<QFileDialog *>(widget);
                auto filename = dialog ? dialog->findChild<QLineEdit *>(QStringLiteral("fileNameEdit")) : nullptr;
                QTemporaryDir folder; const auto output = folder.filePath(QStringLiteral("resized.png"));
                if (!filename || !folder.isValid()) { application.exit(148); return; }
                filename->setFocus(); filename->selectAll(); QTest::keyClicks(filename, output);
                if (!QTest::qWaitFor([&] { return dialog->selectedFiles() == QStringList{output}; }, 1000)
                    || !QMetaObject::invokeMethod(dialog, "accept")
                    || !QTest::qWaitFor([&] { return QFileInfo::exists(output); }, 2000)
                    || !samePixels(QImage(output), expected)) { application.exit(149); return; }
                QTest::qWait(200);
                if (!preview->grabWindow().save(destination)) { application.exit(150); return; }
                qInfo() << "NkShot sharing-copy actual menu/cancel/options/preview/copy/PNG save and source-history isolation passed; styled:" << beautify;
                application.exit(0); return;
            }
            if (qEnvironmentVariableIsSet("NKSHOT_TEST_VIDEO_CLIP")) { runVideoClipChecks(application, viewer, destination); return; }
            if (qEnvironmentVariableIsSet("NKSHOT_TEST_PLAYBACK")) {
                const auto path = qEnvironmentVariable("NKSHOT_TEST_PLAYBACK");
                QFile input(path);
                if (!input.open(QIODevice::ReadOnly)) { application.exit(111); return; }
                const auto original = input.readAll(); input.close();
                SpectacleCore::instance()->setVideoMode(true);
                QQuickItem *recording = nullptr;
                if (!QTest::qWaitFor([&] {
                    recording = visualItem(viewer->contentItem(), QStringLiteral("nkshotRecordingView"));
                    return recording != nullptr;
                }, 2000)) { application.exit(112); return; }
                recording->setProperty("sourceUrl", QUrl::fromLocalFile(path));
                const bool expectError = qEnvironmentVariableIsSet("NKSHOT_TEST_PLAYBACK_ERROR");
                if (!QTest::qWaitFor([&] { return recording->property(expectError ? "playbackFailed" : "hasContent").toBool(); }, 15000)) {
                    qCritical() << "Playback state timeout" << recording->property("playbackFailed");
                    application.exit(113); return;
                }
                if (expectError) {
                    auto retry = visualItem(viewer->contentItem(), QStringLiteral("nkshotPlaybackRetry"));
                    if (!retry || !retry->isVisible()) { application.exit(114); return; }
                    QTest::mouseClick(viewer, Qt::LeftButton, Qt::NoModifier,
                        retry->mapToScene(QPointF(retry->width()/2, retry->height()/2)).toPoint());
                    QTest::qWait(400);
                    if (!QTest::qWaitFor([&] { return recording->property("playbackFailed").toBool(); }, 5000)) {
                        application.exit(115); return;
                    }
                } else if (qEnvironmentVariableIsSet("NKSHOT_TEST_ANIMATION")) {
                    auto animated = visualItem(viewer->contentItem(), QStringLiteral("nkshotAnimatedImage"));
                    auto toggle = visualItem(viewer->contentItem(), QStringLiteral("nkshotPlaybackToggle"));
                    auto seek = visualItem(viewer->contentItem(), QStringLiteral("nkshotPlaybackSeek"));
                    auto player = recording->findChild<QObject *>(QStringLiteral("nkshotMediaPlayer"));
                    if (!animated || !toggle || !seek || !player || seek->isVisible()
                        || !recording->property("hasAnimatedImage").toBool()
                        || !player->property("source").toUrl().isEmpty()) { application.exit(122); return; }
                    if (recording->property("playing").toBool()) {
                        QTest::mouseClick(viewer, Qt::LeftButton, Qt::NoModifier,
                            recording->mapToScene(QPointF(recording->width()/2, recording->height()/2)).toPoint());
                    }
                    if (!QTest::qWaitFor([&] { return !recording->property("playing").toBool() && toggle->isVisible(); }, 2000)) {
                        application.exit(123); return;
                    }
                    QTest::qWait(300);
                    const auto startFrame = animated->property("currentFrame").toInt();
                    QTest::mouseClick(viewer, Qt::LeftButton, Qt::NoModifier,
                        toggle->mapToScene(QPointF(toggle->width()/2, toggle->height()/2)).toPoint());
                    if (!QTest::qWaitFor([&] { return recording->property("playing").toBool()
                        && animated->property("currentFrame").toInt() != startFrame; }, 3000)) { application.exit(124); return; }
                    QTest::mouseClick(viewer, Qt::LeftButton, Qt::NoModifier,
                        toggle->mapToScene(QPointF(toggle->width()/2, toggle->height()/2)).toPoint());
                    if (!QTest::qWaitFor([&] { return !recording->property("playing").toBool(); }, 1000)) { application.exit(125); return; }
                    const auto pausedFrame = animated->property("currentFrame").toInt();
                    QTest::qWait(400);
                    if (animated->property("currentFrame").toInt() != pausedFrame) { application.exit(126); return; }
                    qInfo() << "NkShot real animated-image frames, pause/resume and separate decoder routing passed";
                } else {
                    auto toggle = visualItem(viewer->contentItem(), QStringLiteral("nkshotPlaybackToggle"));
                    auto controls = visualItem(viewer->contentItem(), QStringLiteral("nkshotPlaybackControls"));
                    auto seek = visualItem(viewer->contentItem(), QStringLiteral("nkshotPlaybackSeek"));
                    auto player = recording->findChild<QObject *>(QStringLiteral("nkshotMediaPlayer"));
                    auto softwareVideo = visualItem(viewer->contentItem(), QStringLiteral("nkshotSoftwareVideoPreview"));
                    if (!toggle || !controls || !seek || !player
                        || !QTest::qWaitFor([&] { return controls->isVisible() && controls->opacity() > 0.9; }, 2000)) {
                        application.exit(116); return;
                    }
                    QTest::mouseClick(viewer, Qt::LeftButton, Qt::NoModifier,
                        toggle->mapToScene(QPointF(toggle->width()/2, toggle->height()/2)).toPoint());
                    if (!QTest::qWaitFor([&] { return recording->property("playing").toBool() && player->property("position").toLongLong() > 100; }, 3000)) {
                        application.exit(117); return;
                    }
                    QTest::mouseClick(viewer, Qt::LeftButton, Qt::NoModifier,
                        toggle->mapToScene(QPointF(toggle->width()/2, toggle->height()/2)).toPoint());
                    if (!QTest::qWaitFor([&] { return !recording->property("playing").toBool(); }, 1000)) { application.exit(118); return; }
                    if (!softwareVideo || !QTest::qWaitFor([&] { return softwareVideo->property("hasFrame").toBool(); }, 2000)) {
                        application.exit(121); return;
                    }
                    QTest::mouseClick(viewer, Qt::LeftButton, Qt::NoModifier,
                        seek->mapToScene(QPointF(seek->width()*0.6, seek->height()/2)).toPoint());
                    if (!QTest::qWaitFor([&] { return player->property("position").toLongLong() > 1500; }, 2000)) {
                        application.exit(119); return;
                    }
                }
                if(qEnvironmentVariableIsSet("NKSHOT_TEST_PLAYBACK_SETTINGS")) {
                    auto click=[&](const char *name){auto item=visualItem(viewer->contentItem(),QString::fromLatin1(name));if(!item || !item->isVisible()) return false;QTest::mouseClick(viewer,Qt::LeftButton,Qt::NoModifier,item->mapToScene(QPointF(item->width()/2,item->height()/2)).toPoint());QTest::qWait(120);return true;};
                    auto player=recording->findChild<QObject *>(QStringLiteral("nkshotMediaPlayer"));
                    auto audio=recording->findChild<QObject *>(QStringLiteral("nkshotPlaybackAudio"));
                    auto popup=recording->findChild<QObject *>(QStringLiteral("nkshotPlaybackSettingsPopup"));
                    if(!player || !audio || !popup || !click("nkshotPlaybackSettings") || !popup->property("opened").toBool()) {application.exit(244);return;}
                    auto mute=visualItem(viewer->contentItem(),QStringLiteral("nkshotPlaybackMute"));
                    auto volume=visualItem(viewer->contentItem(),QStringLiteral("nkshotPlaybackVolume"));
                    const bool hasAudio=player->property("hasAudio").toBool();
                    if(!mute || !volume || hasAudio==qEnvironmentVariableIsSet("NKSHOT_TEST_PLAYBACK_SILENT") || mute->isEnabled()!=hasAudio || volume->isEnabled()!=hasAudio) {application.exit(245);return;}
                    if(hasAudio) {
                        if(!click("nkshotPlaybackMute") || !audio->property("muted").toBool() || volume->isEnabled()) {application.exit(246);return;}
                        if(!click("nkshotPlaybackMute") || audio->property("muted").toBool() || !volume->isEnabled()) {application.exit(247);return;}
                        QTest::mouseClick(viewer,Qt::LeftButton,Qt::NoModifier,volume->mapToScene(QPointF(volume->width()*0.35,volume->height()/2)).toPoint());QTest::qWait(100);
                        if(audio->property("volume").toReal()<0.2 || audio->property("volume").toReal()>0.5) {application.exit(248);return;}
                    }
                    if(!click("nkshotPlaybackSpeed")) {application.exit(249);return;}
                    QTest::keyClick(viewer,Qt::Key_End);QTest::keyClick(viewer,Qt::Key_Return);QTest::qWait(150);
                    if(qAbs(player->property("playbackRate").toReal()-2)>0.01) {application.exit(250);return;}
                    viewer->grabWindow().save(destination+QStringLiteral(".settings.png"));
                    QTest::keyClick(viewer,Qt::Key_Escape);QTest::qWait(150);
                    if(popup->property("opened").toBool() || !click("nkshotPlaybackSettings") || !popup->property("opened").toBool()) {application.exit(251);return;}
                    QTest::mouseClick(viewer,Qt::LeftButton,Qt::NoModifier,recording->mapToScene(QPointF(12,12)).toPoint());QTest::qWait(150);
                    if(popup->property("opened").toBool()) {application.exit(252);return;}
                    qInfo()<<"NkShot playback speed/mute/volume/silent-track gating/Escape/reopen/outside-dismiss UI passed; audible output not verified";
                }
                if (qEnvironmentVariableIsSet("NKSHOT_TEST_MEDIA_SWITCH")) {
                    QTemporaryDir repairFolder;
                    const auto repairPath = repairFolder.filePath(QStringLiteral("repair.mp4"));
                    if (!repairFolder.isValid() || !QFile::copy(qEnvironmentVariable("NKSHOT_TEST_SWITCH_BROKEN"), repairPath)) {
                        application.exit(135); return;
                    }
                    const QList<QPair<QString, int>> sources{
                        {qEnvironmentVariable("NKSHOT_TEST_SWITCH_GIF"), 1},
                        {qEnvironmentVariable("NKSHOT_TEST_SWITCH_WEBP"), 1},
                        {repairPath, 2}, {path, 0}};
                    auto softwareVideo = visualItem(viewer->contentItem(), QStringLiteral("nkshotSoftwareVideoPreview"));
                    auto animated = visualItem(viewer->contentItem(), QStringLiteral("nkshotAnimatedImage"));
                    auto player = recording->findChild<QObject *>(QStringLiteral("nkshotMediaPlayer"));
                    if (!softwareVideo || !animated || !player) { application.exit(127); return; }
                    for (const auto &[source, kind] : sources) {
                        QFile nextFile(source);
                        if (!nextFile.open(QIODevice::ReadOnly)) { application.exit(128); return; }
                        const auto bytes = nextFile.readAll(); nextFile.close();
                        recording->setProperty("sourceUrl", QUrl::fromLocalFile(source));
                        if (kind == 2) {
                            if (!QTest::qWaitFor([&] { return recording->property("playbackFailed").toBool()
                                && !recording->property("hasContent").toBool() && !softwareVideo->property("hasFrame").toBool(); }, 5000)) {
                                application.exit(129); return;
                            }
                            QFile repaired(source);
                            if (!repaired.open(QIODevice::WriteOnly | QIODevice::Truncate)
                                || repaired.write(original) != original.size()) { application.exit(136); return; }
                            repaired.close(); // Only the test-owned temporary file is changed.
                            auto retry = visualItem(viewer->contentItem(), QStringLiteral("nkshotPlaybackRetry"));
                            if (!retry) { application.exit(137); return; }
                            QTest::mouseClick(viewer, Qt::LeftButton, Qt::NoModifier,
                                retry->mapToScene(QPointF(retry->width()/2, retry->height()/2)).toPoint());
                            if (!QTest::qWaitFor([&] { return recording->property("hasContent").toBool()
                                && !recording->property("playbackFailed").toBool(); }, 5000)) { application.exit(138); return; }
                            QMetaObject::invokeMethod(recording, "togglePlay");
                            if (!QTest::qWaitFor([&] { return softwareVideo->property("hasFrame").toBool()
                                && player->property("position").toLongLong() > 100; }, 5000)) { application.exit(139); return; }
                            QMetaObject::invokeMethod(recording, "togglePlay");
                        } else if (kind == 1) {
                            if (!QTest::qWaitFor([&] { return recording->property("hasAnimatedImage").toBool()
                                && !recording->property("playbackFailed").toBool() && player->property("source").toUrl().isEmpty()
                                && !softwareVideo->property("active").toBool() && !softwareVideo->property("hasFrame").toBool(); }, 5000)) {
                                application.exit(130); return;
                            }
                            if (!recording->property("playing").toBool()) QMetaObject::invokeMethod(recording, "togglePlay");
                            const auto frame = animated->property("currentFrame").toInt();
                            if (!QTest::qWaitFor([&] { return animated->property("currentFrame").toInt() != frame; }, 2000)) {
                                application.exit(131); return;
                            }
                        } else {
                            if (!QTest::qWaitFor([&] { return recording->property("hasContent").toBool()
                                && !recording->property("playbackFailed").toBool() && !recording->property("hasAnimatedImage").toBool(); }, 5000)) {
                                application.exit(132); return;
                            }
                            QMetaObject::invokeMethod(recording, "togglePlay");
                            if (!QTest::qWaitFor([&] { return softwareVideo->property("hasFrame").toBool()
                                && player->property("position").toLongLong() > 100; }, 5000)) { application.exit(133); return; }
                            QMetaObject::invokeMethod(recording, "togglePlay");
                        }
                        if (!nextFile.open(QIODevice::ReadOnly) || nextFile.readAll() != (kind == 2 ? original : bytes)) { application.exit(134); return; }
                    }
                    qInfo() << "NkShot MP4 GIF WebP invalid-file MP4 switch, decoder isolation, stale-frame clearing and recovery passed";
                }
                QTest::qWait(300);
                if (!viewer->grabWindow().save(destination) || !input.open(QIODevice::ReadOnly) || input.readAll() != original) {
                    application.exit(120); return;
                }
                qInfo() << "NkShot real local playback/error-retry and source immutability passed, error scenario:" << expectError;
                application.exit(0); return;
            }
            if (qEnvironmentVariableIsSet("NKSHOT_TEST_OCR_RESULT")) {
                const bool chinese = qEnvironmentVariableIsSet("NKSHOT_TEST_OCR_CHINESE");
                const QString expected = chinese ? QStringLiteral("中文截图编辑12345") : QStringLiteral("HELLO NKSHOT 12345");
                QImage source(1000, 220, QImage::Format_RGB32); source.fill(Qt::white);
                {
                    QPainter painter(&source);
                    QFont font(chinese ? QStringLiteral("Noto Sans CJK SC") : QStringLiteral("DejaVu Sans"));
                    font.setPixelSize(chinese ? 72 : 64); painter.setFont(font); painter.setPen(Qt::black);
                    painter.drawText(source.rect(), Qt::AlignCenter,
                        chinese ? QStringLiteral("中文截图编辑 12345") : expected);
                }
                auto document = SpectacleCore::instance()->annotationDocument();
                document->setBaseImage(source);
                const auto original = document->renderToImage();
                if (!QTest::qWaitFor([] { return SpectacleCore::instance()->ocrAvailable(); }, 10000)) {
                    application.exit(92); return;
                }
                if (qEnvironmentVariableIsSet("NKSHOT_TEST_OCR_LANGUAGE")) {
                    auto menu = viewer->rootObject()->findChild<QObject *>(QStringLiteral("nkshotOcrLanguageMenu"));
                    if (!menu || !openTaskMenu(viewer, QStringLiteral("nkshotOcrLanguageMenu"))) { application.exit(108); return; }
                    QTest::qWait(300);
                    auto entry = visualItem(viewer->contentItem(), QStringLiteral("nkshotOcrLanguage_")
                        + (chinese ? QStringLiteral("chi_sim") : QStringLiteral("eng")));
                    if (!entry || !entry->isEnabled() || entry->width() < 50) {
                        qWarning() << "OCR language menu missing" << menu->property("count") << menu->property("languages") << entry;
                        viewer->grabWindow().save(destination + QStringLiteral(".failed.png"));
                        application.exit(109); return;
                    }
                    viewer->grabWindow().save(destination + QStringLiteral(".menu.png"));
                    QTest::mouseClick(viewer, Qt::LeftButton, Qt::NoModifier,
                        entry->mapToScene(QPointF(entry->width()/2, entry->height()/2)).toPoint());
                } else if (!SpectacleCore::instance()->startOcrExtraction(chinese ? QStringLiteral("chi_sim") : QStringLiteral("eng"))) {
                    application.exit(92); return;
                }
                QPointer<OcrTextWindow> result;
                if (!QTest::qWaitFor([&] {
                    for (auto window : QGuiApplication::allWindows())
                        if (auto text = qobject_cast<OcrTextWindow *>(window)) result = text;
                    return !result.isNull();
                }, 15000)) { application.exit(93); return; }
                auto normalized = result->recognizedText().simplified();
                if (chinese) normalized.remove(QLatin1Char(' '));
                auto textArea = visualItem(result->contentItem(), QStringLiteral("nkshotOcrText"));
                auto copy = visualItem(result->contentItem(), QStringLiteral("nkshotOcrCopy"));
                auto close = visualItem(result->contentItem(), QStringLiteral("nkshotOcrClose"));
                if (normalized != expected || !textArea || !copy || !close || !textArea->property("readOnly").toBool()
                    || !samePixels(document->renderToImage(), original)) { application.exit(94); return; }
                const bool compact = qEnvironmentVariableIsSet("NKSHOT_TEST_OCR_COMPACT");
                result->resize(compact ? QSize(360, 260) : QSize(520, 360));
                result->requestActivate(); QTest::qWait(300);
                QTest::mouseClick(result, Qt::LeftButton, Qt::NoModifier, textArea->mapToScene(QPointF(20, 20)).toPoint());
                QTest::keyClick(result, Qt::Key_A, Qt::ControlModifier);
                QTest::keyClick(result, Qt::Key_C, Qt::ControlModifier);
                if (textArea->property("selectedText").toString() != result->recognizedText()
                    || QApplication::clipboard()->text() != result->recognizedText()) { application.exit(98); return; }
                QMetaObject::invokeMethod(textArea, "deselect");
                QTest::mouseClick(result, Qt::LeftButton, Qt::NoModifier,
                    copy->mapToScene(QPointF(copy->width()/2, copy->height()/2)).toPoint());
                if (QApplication::clipboard()->text() != result->recognizedText() || !copy->property("copied").toBool()) {
                    application.exit(95); return;
                }
                if (qEnvironmentVariableIsSet("NKSHOT_TEST_OCR_SAVE")) {
                    QTemporaryDir folder;
                    const auto output = folder.filePath(QStringLiteral("recognized.txt"));
                    auto save = visualItem(result->contentItem(), QStringLiteral("nkshotOcrSave"));
                    auto status = visualItem(result->contentItem(), QStringLiteral("nkshotOcrSaveStatus"));
                    if (!folder.isValid() || !save || !status) { application.exit(100); return; }
                    auto openDialog = [&]() -> QFileDialog * {
                        QTest::mouseClick(result, Qt::LeftButton, Qt::NoModifier,
                            save->mapToScene(QPointF(save->width()/2, save->height()/2)).toPoint());
                        QTest::qWait(200);
                        for (auto widget : QApplication::topLevelWidgets())
                            if (widget->objectName() == QStringLiteral("nkshotOcrSaveDialog")) return qobject_cast<QFileDialog *>(widget);
                        return nullptr;
                    };
                    QPointer<QFileDialog> dialog = openDialog();
                    if (!dialog) { application.exit(101); return; }
                    result->saveAs();
                    int count = 0;
                    for (auto widget : QApplication::topLevelWidgets())
                        if (widget->objectName() == QStringLiteral("nkshotOcrSaveDialog")) ++count;
                    if (count != 1) { application.exit(102); return; }
                    dialog->reject();
                    if (!QTest::qWaitFor([&] { return dialog.isNull(); }, 1000) || QFileInfo::exists(output)) {
                        application.exit(103); return;
                    }
                    for (const auto &path : {folder.filePath(QStringLiteral("rejected.png")), output}) {
                        dialog = openDialog();
                        auto filename = dialog ? dialog->findChild<QLineEdit *>(QStringLiteral("fileNameEdit")) : nullptr;
                        if (!filename) { application.exit(104); return; }
                        filename->setFocus(); filename->selectAll(); QTest::keyClicks(filename, path);
                        if (!QTest::qWaitFor([&] { return dialog->selectedFiles() == QStringList{path}; }, 1000)
                            || !QMetaObject::invokeMethod(dialog, "accept")
                            || !QTest::qWaitFor([&] { return dialog.isNull(); }, 1000)) { application.exit(105); return; }
                        if (path.endsWith(QStringLiteral(".png"))
                            && (QFileInfo::exists(path) || !status->property("text").toString().contains(QStringLiteral(".txt")))) {
                            application.exit(106); return;
                        }
                    }
                    QFile textFile(output);
                    if (!textFile.open(QIODevice::ReadOnly) || textFile.readAll() != result->recognizedText().toUtf8()
                        || status->property("text").toString() != QStringLiteral("Text saved")
                        || !samePixels(document->renderToImage(), original)) { application.exit(107); return; }
                    QTest::qWait(200);
                    qInfo() << "NkShot OCR UTF-8 save, cancel/reopen/reuse, wrong extension and source isolation passed";
                }
                if (!result->grabWindow().save(destination)) { application.exit(96); return; }
                QTest::mouseClick(result, Qt::LeftButton, Qt::NoModifier,
                    close->mapToScene(QPointF(close->width()/2, close->height()/2)).toPoint());
                if (!QTest::qWaitFor([&] { return result.isNull(); }, 1000)) { application.exit(97); return; }
                qInfo() << "NkShot real OCR result, copy feedback, source isolation and close passed" << expected;
                application.exit(0); return;
            }
            if (qEnvironmentVariableIsSet("NKSHOT_TEST_COMBINE")) {
                const bool stacking = qEnvironmentVariableIsSet("NKSHOT_TEST_STACK");
                const bool horizontal = qEnvironmentVariableIsSet("NKSHOT_TEST_HORIZONTAL");
                auto document = SpectacleCore::instance()->annotationDocument();
                const auto original = document->renderToImage();
                QTemporaryDir files;
                QImage expected(320, 600, QImage::Format_RGBA8888);
                expected.fill(QColor(QStringLiteral("#f5f5f7")));
                {
                    QPainter painter(&expected);
                    painter.fillRect(QRect(12, 12, 296, 576), Qt::white);
                    QFont heading(QStringLiteral("DejaVu Sans"), 15, QFont::Bold);
                    painter.setFont(heading); painter.setPen(QColor(QStringLiteral("#202126")));
                    painter.drawText(QRect(28, 28, 264, 58), Qt::TextWordWrap, QStringLiteral("Design notes"));
                    painter.setFont(QFont(QStringLiteral("DejaVu Sans"), 9));
                    painter.setPen(QColor(QStringLiteral("#747680")));
                    painter.drawText(28, 77, QStringLiteral("Synthetic local stitching fixture"));
                    for (int section = 0; section < 5; ++section) {
                        const int y = 110 + section * 90;
                        painter.setPen(QColor(QStringLiteral("#202126")));
                        painter.setFont(QFont(QStringLiteral("DejaVu Sans"), 11, QFont::DemiBold));
                        painter.drawText(28, y, QStringLiteral("%1. Keep the essentials").arg(section + 1));
                        for (int line = 0; line < 3; ++line)
                            painter.fillRect(QRect(28, y + 12 + line * 13, 238 - line * 26, 4), QColor(QStringLiteral("#dfe1e7")));
                    }
                }
                // A one-pixel row marker makes exact overlap unambiguous without
                // replacing the document-like visual fixture with random noise.
                for (int y = 0; y < expected.height(); ++y)
                    expected.setPixelColor(0, y, QColor(y % 256, y / 256, 240));
                if (horizontal) {
                    expected = QImage(600, 320, QImage::Format_RGBA8888);
                    expected.fill(QColor("#f5f5f7"));
                    QPainter painter(&expected);
                    painter.setPen(QColor("#202126"));
                    painter.setFont(QFont(QStringLiteral("DejaVu Sans"), 16, QFont::Bold));
                    painter.drawText(24, 40, QStringLiteral("A wider view"));
                    painter.setFont(QFont(QStringLiteral("DejaVu Sans"), 9));
                    painter.setPen(QColor("#747680"));
                    painter.drawText(24, 64, QStringLiteral("Synthetic local image composition"));
                    for (int column = 0; column < 3; ++column) {
                        const int x = 24 + column*190;
                        painter.fillRect(QRect(x, 86, 172, 208), Qt::white);
                        painter.setPen(QColor("#202126"));
                        painter.setFont(QFont(QStringLiteral("DejaVu Sans"), 12, QFont::DemiBold));
                        painter.drawText(x+12, 116, QStringLiteral("%1. %2").arg(column+1).arg(QStringList{QStringLiteral("Capture"), QStringLiteral("Refine"), QStringLiteral("Share")}[column]));
                        painter.fillRect(QRect(x+12, 134, 148, 76), QColor(column==0 ? "#dcecff" : column==1 ? "#e8e2ff" : "#dff4e9"));
                        for (int line=0; line<4; ++line) painter.fillRect(QRect(x+12, 228+line*12, 138-line*12, 4), QColor("#dfe1e7"));
                    }
                    painter.end();
                    for (int x=0; x<expected.width(); ++x) expected.setPixelColor(x,0,QColor(x%256,x/256,240));
                }
                const auto slice = [&](int offset, int length, int trimCross=0) {
                    return horizontal ? expected.copy(offset,0,length,320-trimCross) : expected.copy(0,offset,320-trimCross,length);
                };
                const auto top = files.filePath(QStringLiteral("frame2.png"));
                const auto bottom = files.filePath(QStringLiteral("frame10.png"));
                if (!files.isValid() || !slice(0, stacking ? 300 : 360).save(top)
                    || !slice(stacking ? 300 : 240, stacking ? 300 : 360).save(bottom)) { application.exit(72); return; }
                auto clickCombine = [&](const QString &name) {
                    auto item = visualItem(viewer->contentItem(), name);
                    if (!item || !item->isVisible() || !item->isEnabled()) return false;
                    QTest::mouseClick(viewer, Qt::LeftButton, Qt::NoModifier, item->mapToScene(QPointF(item->width()/2,item->height()/2)).toPoint());
                    QTest::qWait(120); return true;
                };
                auto openConfig = [&] {
                    viewer->requestActivate(); QTest::qWait(100);
                    auto more = viewer->rootObject()->findChild<QObject *>(QStringLiteral("nkshotMoreMenu"));
                    if (!more || !QMetaObject::invokeMethod(more,"open")) return false;
                    QTest::qWait(200);
                    return clickCombine(QStringLiteral("nkshotCombineImagesAction"));
                };
                if (!openConfig()
                    || !clickCombine(horizontal ? QStringLiteral("nkshotCombineHorizontal") : QStringLiteral("nkshotCombineVertical"))
                    || !clickCombine(stacking ? QStringLiteral("nkshotCombineStack") : QStringLiteral("nkshotCombineMatch"))
                    || !viewer->grabWindow().save(destination+QStringLiteral(".setup.png"))
                    || !clickCombine(QStringLiteral("nkshotCombineCancel")) || viewer->combiningImages()
                    || !samePixels(document->renderToImage(),original) || !openConfig()) { application.exit(73); return; }
                auto direction = visualItem(viewer->contentItem(), horizontal ? QStringLiteral("nkshotCombineHorizontal") : QStringLiteral("nkshotCombineVertical"));
                auto method = visualItem(viewer->contentItem(), stacking ? QStringLiteral("nkshotCombineStack") : QStringLiteral("nkshotCombineMatch"));
                if (!clickCombine(horizontal ? QStringLiteral("nkshotCombineHorizontal") : QStringLiteral("nkshotCombineVertical"))
                    || !clickCombine(stacking ? QStringLiteral("nkshotCombineStack") : QStringLiteral("nkshotCombineMatch"))) { application.exit(73); return; }
                if (!direction || !method || !direction->property("checked").toBool() || !method->property("checked").toBool()
                    || !clickCombine(QStringLiteral("nkshotCombineChoose"))) { application.exit(73); return; }
                QTest::qWait(200);
                auto currentDialog = []() -> QFileDialog * {
                    for (auto widget : QApplication::topLevelWidgets())
                        if (widget->objectName() == QStringLiteral("nkshotCombineDialog")) return qobject_cast<QFileDialog *>(widget);
                    return nullptr;
                };
                QPointer<QFileDialog> dialog = currentDialog();
                if (!dialog) { application.exit(74); return; }
                viewer->combineImages(stacking, horizontal);
                if (currentDialog() != dialog) { application.exit(75); return; }
                dialog->reject();
                if (!QTest::qWaitFor([&] { return dialog.isNull(); }, 1000) || viewer->combiningImages()) { application.exit(76); return; }
                viewer->combineImages(stacking, horizontal); QTest::qWait(200);
                dialog = currentDialog();
                auto filename = dialog ? dialog->findChild<QLineEdit *>(QStringLiteral("fileNameEdit")) : nullptr;
                if (!filename) { application.exit(77); return; }
                filename->setFocus(); filename->selectAll();
                QTest::keyClicks(filename, QStringLiteral("\"%1\" \"%2\"").arg(bottom, top));
                QMetaObject::invokeMethod(dialog, "accept");
                if (!QTest::qWaitFor([&] { return !viewer->combiningImages() && dialog.isNull(); }, 5000)) { application.exit(78); return; }
                QPointer<PinnedImageWindow> combined;
                for (auto window : QGuiApplication::allWindows())
                    if (auto pin = qobject_cast<PinnedImageWindow *>(window)) combined = pin;
                if (!combined || !samePixels(combined->snapshot(), expected) || !samePixels(document->renderToImage(), original)) {
                    qCritical() << "Combined image mismatch" << bool(combined);
                    application.exit(79); return;
                }
                combined->resize(horizontal ? QSize(720, 440) : QSize(380, 520)); QTest::qWait(250);
                if (!combined->grabWindow().save(destination)) { application.exit(80); return; }
                combined->close();
                if (!QTest::qWaitFor([&] { return combined.isNull(); }, 1000)) { application.exit(81); return; }
                if (stacking) {
                    qInfo() << "NkShot explicit stack exact pixels, sorted order, cancel/reopen/reuse and source isolation passed";
                    application.exit(0); return;
                }
                // Duplicate frames must reject rather than fabricate a long image.
                const auto duplicate = files.filePath(QStringLiteral("frame3.png"));
                if (!slice(0, 360).save(duplicate)) { application.exit(82); return; }
                const auto beforeWarning = InlineMessageModel::instance()->rowCount();
                viewer->combineImages(false, horizontal); QTest::qWait(200);
                dialog = currentDialog();
                filename = dialog ? dialog->findChild<QLineEdit *>(QStringLiteral("fileNameEdit")) : nullptr;
                if (!filename) { application.exit(83); return; }
                filename->setFocus(); filename->selectAll();
                QTest::keyClicks(filename, QStringLiteral("\"%1\" \"%2\"").arg(top, duplicate));
                QMetaObject::invokeMethod(dialog, "accept");
                if (!QTest::qWaitFor([&] { return !viewer->combiningImages() && dialog.isNull(); }, 5000)
                    || InlineMessageModel::instance()->rowCount() != beforeWarning + 1
                    || !samePixels(document->renderToImage(), original)) { application.exit(84); return; }
                for (auto window : QGuiApplication::allWindows())
                    if (qobject_cast<PinnedImageWindow *>(window)) { application.exit(85); return; }
                const auto warningText = InlineMessageModel::instance()->data(
                    InlineMessageModel::instance()->index(beforeWarning, 0), Qt::DisplayRole).toString();
                if (!warningText.contains(QStringLiteral("repeated image"))) { application.exit(86); return; }
                if (qEnvironmentVariableIsSet("NKSHOT_TEST_COMBINE_ERRORS")) {
                    const auto wrongWidth = files.filePath(QStringLiteral("frame20-width.png"));
                    const auto missingOverlap = files.filePath(QStringLiteral("frame20-gap.png"));
                    const auto broken = files.filePath(QStringLiteral("frame20-broken.png"));
                    QImage gap(horizontal ? 360 : 320, horizontal ? 320 : 360, QImage::Format_RGBA8888); gap.fill(QColor(222, 222, 222));
                    QFile damaged(broken);
                    if (!slice(240, 360, 1).save(wrongWidth) || !gap.save(missingOverlap)
                        || !damaged.open(QIODevice::WriteOnly)) { application.exit(87); return; }
                    damaged.write("not an image"); damaged.close();
                    const QList<QPair<QStringList, QString>> errors = {
                        {{top, wrongWidth}, horizontal ? QStringLiteral("same height") : QStringLiteral("same width")},
                        {{top, broken}, QStringLiteral("could not be read")},
                        {{top}, QStringLiteral("Choose 2")},
                        {{top, missingOverlap}, QStringLiteral("No unchanged overlap")},
                    };
                    for (const auto &test : errors) {
                        auto messages = InlineMessageModel::instance(); messages->clear();
                        viewer->combineImages(false, horizontal); QTest::qWait(200); dialog = currentDialog();
                        filename = dialog ? dialog->findChild<QLineEdit *>(QStringLiteral("fileNameEdit")) : nullptr;
                        if (!filename) { application.exit(88); return; }
                        QStringList quoted;
                        for (const auto &path : test.first) quoted.append(QStringLiteral("\"%1\"").arg(path));
                        filename->setFocus(); filename->selectAll(); QTest::keyClicks(filename, quoted.join(QLatin1Char(' ')));
                        QMetaObject::invokeMethod(dialog, "accept");
                        if (!QTest::qWaitFor([&] { return !viewer->combiningImages() && dialog.isNull(); }, 5000)
                            || messages->rowCount() != 1 || !messages->data(messages->index(0, 0), Qt::DisplayRole).toString().contains(test.second)
                            || !samePixels(document->renderToImage(), original)) { application.exit(89); return; }
                        for (auto window : QGuiApplication::allWindows())
                            if (qobject_cast<PinnedImageWindow *>(window)) { application.exit(90); return; }
                    }
                    QTest::qWait(400);
                    if (!viewer->grabWindow().save(destination)) { application.exit(91); return; }
                    qInfo() << "NkShot specific mismatch, damaged-image, count and missing-overlap errors passed";
                }
                qInfo() << "NkShot combine actual menu, cancel/reopen, numeric filename order, exact pixels and source isolation and duplicate rejection passed";
                application.exit(0); return;
            }
            if (qEnvironmentVariableIsSet("NKSHOT_TEST_SECONDARY_TOOLS")) {
                auto document = SpectacleCore::instance()->annotationDocument();
                auto menu = viewer->rootObject()->findChild<QObject *>(QStringLiteral("nkshotExtraToolsMenu"));
                const auto original = document->renderToImage();
                const auto depth = document->undoStackDepth();
                const QList<QPair<QString, AnnotationTool::Tool>> cases = {
                    {QStringLiteral("Highlighter"), AnnotationTool::HighlighterTool},
                    {QStringLiteral("Line"), AnnotationTool::LineTool},
                    {QStringLiteral("Ellipse"), AnnotationTool::EllipseTool},
                    {QStringLiteral("Blur"), AnnotationTool::BlurTool},
                    {QStringLiteral("Number"), AnnotationTool::NumberTool},
                    {QStringLiteral("Pan"), AnnotationTool::NoTool},
                };
                for (const auto &test : cases) {
                    if (!menu || !QMetaObject::invokeMethod(menu, "open")) { application.exit(67); return; }
                    QTest::qWait(200);
                    auto item = visualItem(viewer->contentItem(), QStringLiteral("nkshotExtra") + test.first);
                    if (!item) { application.exit(68); return; }
                    QTest::mouseClick(viewer, Qt::LeftButton, Qt::NoModifier,
                        item->mapToScene(QPointF(item->width()/2, item->height()/2)).toPoint());
                    if (document->tool()->type() != test.second || !samePixels(document->renderToImage(), original)
                        || document->undoStackDepth() != depth) { application.exit(69); return; }
                }
                QMetaObject::invokeMethod(menu, "open");
                QTest::qWait(200);
                qInfo() << "NkShot actual secondary-tool menu clicks, pan and unchanged document passed";
            }
            if (qEnvironmentVariableIsSet("NKSHOT_TEST_PIN")) {
                auto document = SpectacleCore::instance()->annotationDocument();
                const auto original = document->renderToImage();
                if (!openTaskMenu(viewer, QStringLiteral("nkshotPinsNotesMenu"))) { application.exit(41); return; }
                auto pinAction = visualItem(viewer->contentItem(), QStringLiteral("nkshotPinImageAction"));
                if (!pinAction) { application.exit(41); return; }
                QTest::mouseClick(viewer, Qt::LeftButton, Qt::NoModifier,
                    pinAction->mapToScene(QPointF(pinAction->width()/2, pinAction->height()/2)).toPoint());
                QPointer<PinnedImageWindow> first;
                for (auto window : QGuiApplication::allWindows()) {
                    if (auto pin = qobject_cast<PinnedImageWindow *>(window)) first = pin;
                }
                if (!first) { application.exit(41); return; }
                document->tool()->setType(AnnotationTool::RectangleTool);
                document->beginItem({100,100}); document->continueItem({350,250}); document->finishItem();
                const auto edited = document->renderToImage();
                QPointer<PinnedImageWindow> second = qobject_cast<PinnedImageWindow *>(viewer->pinImage());
                if (!second || !samePixels(first->snapshot(), original) || !samePixels(second->snapshot(), edited)
                    || samePixels(first->snapshot(), second->snapshot())) { application.exit(42); return; }
                const auto pinSize = qEnvironmentVariable("NKSHOT_TEST_PIN_SIZE", QStringLiteral("480x360")).split(u'x');
                if (pinSize.size() == 2 && pinSize[0].toInt() >= 260 && pinSize[1].toInt() >= 180)
                    first->resize(pinSize[0].toInt(), pinSize[1].toInt());
                else first->resize(480, 360);
                first->requestActivate();
                QTest::qWait(400);
                auto click = [](QQuickWindow *window, const QString &name) {
                    auto button = visualItem(window->contentItem(), name);
                    if (!button) return false;
                    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                        button->mapToScene(QPointF(button->width()/2, button->height()/2)).toPoint());
                    return true;
                };
                if (!click(first, QStringLiteral("nkshotPinCopy")) || !samePixels(QApplication::clipboard()->image(), original)) {
                    application.exit(43); return;
                }
                auto copyButton = visualItem(first->contentItem(), QStringLiteral("nkshotPinCopy"));
                if (!copyButton || !copyButton->property("copied").toBool()) { application.exit(51); return; }
                if (qEnvironmentVariableIsSet("NKSHOT_TEST_PIN_FEEDBACK")) {
                    if (!QTest::qWaitFor([copyButton] { return !copyButton->property("copied").toBool(); }, 2500)) { application.exit(52); return; }
                }
                if (qEnvironmentVariableIsSet("NKSHOT_TEST_PIN_SAVE")) {
                    QTemporaryDir saveDirectory;
                    if (!saveDirectory.isValid()) { application.exit(53); return; }
                    const QString output = saveDirectory.filePath(QStringLiteral("NkShot.png"));
                    if (QFileInfo::exists(output) || !click(first, QStringLiteral("nkshotPinSave"))) { application.exit(53); return; }
                    QTest::qWait(200);
                    QPointer<QFileDialog> dialog;
                    for (auto widget : QApplication::topLevelWidgets())
                        if (widget->objectName() == QStringLiteral("nkshotPinSaveDialog")) dialog = qobject_cast<QFileDialog *>(widget);
                    if (!dialog) { application.exit(54); return; }
                    first->saveAs(); // Repeated action must reuse the one dialog.
                    int count = 0;
                    for (auto widget : QApplication::topLevelWidgets())
                        if (widget->objectName() == QStringLiteral("nkshotPinSaveDialog")) ++count;
                    if (count != 1) { application.exit(55); return; }
                    dialog->reject();
                    if (!QTest::qWaitFor([&dialog] { return dialog.isNull(); }, 1000) || QFileInfo::exists(output)) { application.exit(56); return; }
                    click(first, QStringLiteral("nkshotPinSave"));
                    QTest::qWait(200);
                    for (auto widget : QApplication::topLevelWidgets())
                        if (widget->objectName() == QStringLiteral("nkshotPinSaveDialog")) dialog = qobject_cast<QFileDialog *>(widget);
                    if (!dialog) { application.exit(57); return; }
                    // The widget dialog loads directory contents asynchronously. Drive its
                    // filename editor as a user would instead of assuming selectFile()
                    // has replaced the already-visible default selection immediately.
                    auto filename = dialog->findChild<QLineEdit *>(QStringLiteral("fileNameEdit"));
                    if (!filename) { application.exit(58); return; }
                    filename->setFocus();
                    filename->selectAll();
                    QTest::keyClicks(filename, output);
                    if (!QTest::qWaitFor([&] { return dialog && dialog->selectedFiles() == QStringList{output}; }, 1000)) {
                        application.exit(58); return;
                    }
                    if (!QMetaObject::invokeMethod(dialog, "accept")) { application.exit(58); return; }
                    if (!QTest::qWaitFor([&] { return QFileInfo::exists(output); }, 2000)) { application.exit(59); return; }
                    if (!samePixels(QImage(output), original) || !samePixels(document->renderToImage(), edited)) { qWarning() << "pin save mismatch" << output << QFileInfo::exists(output) << QImage(output).size() << samePixels(document->renderToImage(), edited) << (dialog ? dialog->selectedFiles() : QStringList()); application.exit(59); return; }
                    auto status = visualItem(first->contentItem(), QStringLiteral("nkshotPinSaveStatus"));
                    if (!status || status->property("text").toString().isEmpty()) { application.exit(60); return; }
                    qInfo() << "NkShot pin Save As cancel/reopen/repeated-dialog/PNG/source-isolation passed" << output;
                }
                if (!first->stayOnTop() || !click(first, QStringLiteral("nkshotPinTop")) || first->stayOnTop()) {
                    application.exit(44); return;
                }
                click(first, QStringLiteral("nkshotPinTop"));
                if (!first->stayOnTop()) { application.exit(45); return; }
                if (!click(second, QStringLiteral("nkshotPinClose")) || !QTest::qWaitFor([&second] { return second.isNull(); }, 1000)) {
                    application.exit(46); return;
                }
                document->undo();
                if (!samePixels(document->renderToImage(), original) || !samePixels(first->snapshot(), original)) { application.exit(47); return; }
                viewer->close();
                QTest::qWait(200);
                if (!first || !first->isVisible() || !samePixels(first->snapshot(), original)) { application.exit(50); return; }
                viewer->show();
                if (!first->grabWindow().save(destination)) { application.exit(48); return; }
                if (!click(first, QStringLiteral("nkshotPinClose")) || !QTest::qWaitFor([&first] { return first.isNull(); }, 1000)) {
                    application.exit(49); return;
                }
                qInfo() << "NkShot actual menu entry, two pin snapshots, copy/top-toggle/close, source isolation and editor-close survival passed";
                qInfo() << "NkShot actual pinned window capture" << destination;
                application.exit(0);
                return;
            }
            if (qEnvironmentVariableIsSet("NKSHOT_TEST_CROP_GESTURE")) {
                auto document = SpectacleCore::instance()->annotationDocument();
                auto viewport = visualItem(viewer->contentItem(), QStringLiteral("nkshotAnnotationViewport"));
                if (!viewport) { application.exit(35); return; }
                const auto before = document->renderToImage();
                const auto originalRect = document->canvasRect();
                document->tool()->setType(AnnotationTool::CropTool);
                QTest::qWait(200);
                const auto start = viewport->mapToScene(QPointF(viewport->width()*0.18, viewport->height()*0.18)).toPoint();
                const auto end = viewport->mapToScene(QPointF(viewport->width()*0.72, viewport->height()*0.72)).toPoint();
                const auto middle = (start + end) / 2;
                auto drag = [viewer, start, end] {
                    QTest::mousePress(viewer, Qt::LeftButton, Qt::NoModifier, start);
                    QTest::mouseMove(viewer, (start + end) / 2, 20);
                    QTest::mouseMove(viewer, end, 20);
                    QTest::mouseRelease(viewer, Qt::LeftButton, Qt::NoModifier, end);
                };
                drag();
                QTest::mouseClick(viewer, Qt::RightButton, Qt::NoModifier, middle);
                QTest::qWait(600); // TapHandler single-tap waits for the double-tap interval.
                if (!samePixels(document->renderToImage(), before) || document->undoStackDepth() != 0 || !document->tool()->geometry().isEmpty()) { application.exit(36); return; }
                drag();
                QTest::qWait(100);
                QTest::mouseClick(viewer, Qt::LeftButton, Qt::NoModifier, middle, 20);
                QTest::qWait(80);
                QTest::mouseClick(viewer, Qt::LeftButton, Qt::NoModifier, middle, 20);
                if (!QTest::qWaitFor([document] { return document->undoStackDepth() == 1; }, 1000)) {
                    qCritical() << "NkShot crop double click failed" << document->tool()->geometry();
                    viewer->grabWindow().save(QStringLiteral("/build/screenshots/030-crop-failed.png"));
                    application.exit(37); return;
                }
                const auto cropped = document->renderToImage();
                const auto rect = document->canvasRect().toRect();
                if (rect.width() >= originalRect.width() || rect.height() >= originalRect.height()
                    || !samePixels(cropped, before.copy(rect))) { qCritical() << "crop pixel comparison" << document->canvasRect() << cropped.size() << rect; application.exit(38); return; }
                QTest::keyClick(viewer, Qt::Key_Z, Qt::ControlModifier);
                if (!QTest::qWaitFor([document, before] { return samePixels(document->renderToImage(), before); }, 1000)) {
                    qCritical() << "NkShot actual crop undo shortcut failed"; application.exit(39); return;
                }
                QTest::keyClick(viewer, Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
                if (!QTest::qWaitFor([document, cropped] { return samePixels(document->renderToImage(), cropped); }, 1000)) {
                    qCritical() << "NkShot actual crop redo shortcut failed"; application.exit(40); return;
                }
                document->tool()->setType(AnnotationTool::NoTool);
                QTest::qWait(300);
                qInfo() << "NkShot actual crop cancel/accept and keyboard undo/redo passed" << rect;
            }
            if (qEnvironmentVariableIsSet("NKSHOT_TEST_TOOL_KEYS")) {
                auto document = SpectacleCore::instance()->annotationDocument();
                if (document->undoStackDepth() == 0) {
                    document->tool()->setType(AnnotationTool::RectangleTool);
                    document->beginItem({100, 100}); document->continueItem({200, 160}); document->finishItem();
                }
                const auto pixels = document->renderToImage();
                const auto depth = document->undoStackDepth();
                const QList<AnnotationTool::Tool> types{AnnotationTool::CropTool, AnnotationTool::SelectTool,
                    AnnotationTool::FreehandTool, AnnotationTool::ArrowTool, AnnotationTool::RectangleTool,
                    AnnotationTool::PixelateTool, AnnotationTool::TextTool};
                viewer->requestActivate();
                QTest::qWait(100);
                for (int i = 0; i < types.size(); ++i) {
                    if (i == 1 && depth == 0) continue;
                    QTest::keyClick(viewer, Qt::Key(Qt::Key_1 + i), Qt::AltModifier);
                    if (!QTest::qWaitFor([&] { return document->tool()->type() == types[i]; }, 1000)
                        || !samePixels(document->renderToImage(), pixels) || document->undoStackDepth() != depth) {
                        qCritical() << "Tool shortcut failed" << i << document->tool()->type();
                        application.exit(110); return;
                    }
                }
                QTest::keyClick(viewer, Qt::Key_3, Qt::AltModifier);
                auto freehand = visualItem(viewer->contentItem(), QStringLiteral("nkshotFreehandTool"));
                if (freehand) {
                    QTest::mouseMove(viewer, freehand->mapToScene(QPointF(freehand->width()/2, freehand->height()/2)).toPoint());
                    QTest::qWait(1200);
                }
                qInfo() << "NkShot actual Alt tool shortcuts preserve pixels and undo history";
            }
            if (qEnvironmentVariableIsSet("NKSHOT_TEST_TEXT_INPUT")) {
                auto document = SpectacleCore::instance()->annotationDocument();
                auto viewport = visualItem(viewer->contentItem(), QStringLiteral("nkshotAnnotationViewport"));
                if (!viewport) { application.exit(31); return; }
                const auto before = document->renderToImage();
                document->tool()->setType(AnnotationTool::TextTool);
                QFont font(QStringLiteral("Noto Sans CJK SC")); font.setPixelSize(32);
                document->tool()->setFont(font);
                document->tool()->setFontColor(Qt::black);
                const auto point = viewport->mapToScene(QPointF(viewport->width()*0.25, viewport->height()*0.3)).toPoint();
                QTest::mouseClick(viewer, Qt::LeftButton, Qt::NoModifier, point);
                QTest::qWait(200);
                for (char key : QByteArray("NkShot ")) QTest::keyClick(viewer, key);
                auto focus = viewer->focusObject();
                if (!focus) { application.exit(34); return; }
                QInputMethodEvent preedit(QStringLiteral("zhongwen"), {});
                QApplication::sendEvent(focus, &preedit);
                QInputMethodEvent commit;
                commit.setCommitString(QStringLiteral("中文"));
                QApplication::sendEvent(focus, &commit);
                QTest::qWait(500);
                auto selected = document->property("selectedItem").value<QObject *>();
                if (!selected || selected->property("text").toString() != QStringLiteral("NkShot 中文")) {
                    qCritical() << "NkShot keyboard/IME text mismatch" << (selected ? selected->property("text") : QVariant());
                    application.exit(32); return;
                }
                document->tool()->setType(AnnotationTool::NoTool);
                document->deselectItem();
                const auto changed = document->renderToImage();
                const auto depth = document->undoStackDepth();
                for (int i = 0; i < depth; ++i) document->undo();
                const bool restored = samePixels(document->renderToImage(), before);
                for (int i = 0; i < depth; ++i) document->redo();
                if (!depth || samePixels(changed, before) || !restored || !samePixels(document->renderToImage(), changed)) {
                    application.exit(33); return;
                }
                QTest::qWait(200);
                qInfo() << "NkShot actual keyboard and Chinese IME commit plus undo/redo passed";
            }
            const auto redaction = qEnvironmentVariable("NKSHOT_TEST_REDACTION");
            if (redaction == QStringLiteral("blur") || redaction == QStringLiteral("pixelate")) {
                auto document = SpectacleCore::instance()->annotationDocument();
                auto viewport = visualItem(viewer->contentItem(), QStringLiteral("nkshotAnnotationViewport"));
                if (!viewport) { application.exit(28); return; }
                const auto before = document->renderToImage();
                document->tool()->setType(redaction == QStringLiteral("blur") ? AnnotationTool::BlurTool : AnnotationTool::PixelateTool);
                document->tool()->setStrength(0.6);
                const auto start = viewport->mapToScene(QPointF(viewport->width()*0.12, viewport->height()*0.2)).toPoint();
                const auto end = viewport->mapToScene(QPointF(viewport->width()*0.65, viewport->height()*0.7)).toPoint();
                QTest::mousePress(viewer, Qt::LeftButton, Qt::NoModifier, start);
                QTest::mouseMove(viewer, end);
                QTest::mouseRelease(viewer, Qt::LeftButton, Qt::NoModifier, end);
                if (!QTest::qWaitFor([document] { return document->undoStackDepth() == 1; }, 1000)) { application.exit(29); return; }
                const auto changed = document->renderToImage();
                document->undo();
                const bool restored = samePixels(document->renderToImage(), before);
                document->redo();
                if (samePixels(changed, before) || !restored || !samePixels(document->renderToImage(), changed)) {
                    qCritical() << "NkShot actual redaction gesture regression failed" << "unchanged" << samePixels(changed, before) << "undo" << restored << "redo" << samePixels(document->renderToImage(), changed);
                    application.exit(30); return;
                }
                QTest::qWait(200);
                qInfo() << "NkShot actual redaction gesture and undo/redo passed" << redaction;
            }
            if (qEnvironmentVariableIsSet("NKSHOT_TEST_SECOND_DOCUMENT")) {
                auto original = SpectacleCore::instance()->annotationDocument();
                const auto originalPixels = original->renderToImage();
                const auto originalTool = original->tool()->type();
                const auto originalDepth = original->undoStackDepth();
                AnnotationDocument alternate;
                QImage blank(100, 80, QImage::Format_ARGB32_Premultiplied);
                blank.fill(Qt::white);
                alternate.setBaseImage(blank);
                alternate.tool()->setType(AnnotationTool::RectangleTool);
                alternate.tool()->setStrokeWidth(4);
                alternate.beginItem({20,20}); alternate.continueItem({70,60}); alternate.finishItem();
                auto palette = visualItem(viewer->contentItem(), QStringLiteral("nkshotToolPalette"));
                auto history = visualItem(viewer->contentItem(), QStringLiteral("nkshotHistoryControls"));
                auto properties = viewer->rootObject()->findChild<QObject *>(QStringLiteral("nkshotToolProperties"));
                if (!palette || !history || !properties) { application.exit(26); return; }
                for (QObject *control : {static_cast<QObject *>(palette), static_cast<QObject *>(history), properties})
                    control->setProperty("editorDocument", QVariant::fromValue(&alternate));
                QTest::qWait(100);
                auto click = [viewer](QQuickItem *item) {
                    QTest::mouseClick(viewer, Qt::LeftButton, Qt::NoModifier, item->mapToScene(QPointF(item->width()/2, item->height()/2)).toPoint());
                };
                auto freehand = visualItem(viewer->contentItem(), QStringLiteral("nkshotFreehandTool"));
                auto undo = visualItem(viewer->contentItem(), QStringLiteral("nkshotUndo"));
                auto redo = visualItem(viewer->contentItem(), QStringLiteral("nkshotRedo"));
                bool passed = freehand && undo && redo;
                if (passed) {
                    click(freehand);
                    passed &= alternate.tool()->type() == AnnotationTool::FreehandTool;
                    click(undo);
                    passed &= alternate.undoStackDepth() == 0 && samePixels(alternate.renderToImage(), blank);
                    click(redo);
                    passed &= alternate.undoStackDepth() == 1;
                }
                // Exercise the actual property editor against the alternate document.
                passed &= properties->property("document").value<AnnotationDocument *>() == &alternate;
                alternate.tool()->setType(AnnotationTool::SelectTool);
                alternate.selectItem({15,15,60,50});
                const auto beforeProperties = alternate.renderToImage();
                auto popup = viewer->rootObject()->findChild<QObject *>(QStringLiteral("nkshotPropertiesPopup"));
                if (popup) {
                    QMetaObject::invokeMethod(popup, "open");
                    QTest::qWait(100);
                    // Changing tool/document may recreate the Loader's spin box.
                    auto spin = viewer->rootObject()->findChild<QObject *>(QStringLiteral("nkshotStrokeWidth"));
                    if (spin) spin->setProperty("value", 12);
                    else passed = false;
                    QMetaObject::invokeMethod(popup, "close");
                    passed &= QTest::qWaitFor([&alternate] { return alternate.undoStackDepth() == 2; }, 1000);
                    passed &= !samePixels(alternate.renderToImage(), beforeProperties);
                } else { passed = false; }
                passed &= original->tool()->type() == originalTool && original->undoStackDepth() == originalDepth
                    && samePixels(original->renderToImage(), originalPixels);
                for (QObject *control : {static_cast<QObject *>(palette), static_cast<QObject *>(history), properties})
                    control->setProperty("editorDocument", QVariant::fromValue(original));
                QTest::qWait(200); // settle bindings before the alternate document is destroyed
                if (!passed) { qCritical() << "NkShot second-document control isolation failed"; application.exit(27); return; }
                qInfo() << "NkShot second-document actual tool/undo/redo clicks and property edit isolated from screenshot document";
            }
            if (qEnvironmentVariableIsSet("NKSHOT_TEST_PROPERTY_TRANSACTION")) {
                auto document = SpectacleCore::instance()->annotationDocument();
                auto spin = viewer->rootObject()->findChild<QObject *>(QStringLiteral("nkshotStrokeWidth"));
                auto popup = viewer->rootObject()->findChild<QObject *>(QStringLiteral("nkshotPropertiesPopup"));
                const auto before = document->renderToImage();
                if (!spin || !popup || !spin->setProperty("value", 12)) {
                    application.exit(8);
                    return;
                }
                // Close through the visible control before the delayed commit settles.
                auto closeProperties = visualItem(viewer->contentItem(), QStringLiteral("nkshotCloseProperties"));
                if (!closeProperties) { application.exit(65); return; }
                QTest::mouseClick(viewer, Qt::LeftButton, Qt::NoModifier,
                    closeProperties->mapToScene(QPointF(closeProperties->width()/2, closeProperties->height()/2)).toPoint());
                if (popup->property("visible").toBool()) { application.exit(66); return; }
                QTest::qWait(300);
                if (!QTest::qWaitFor([document] { return document->undoStackDepth() == 2; }, 1000)) {
                    qCritical() << "NkShot property close test: missing undo transaction";
                    application.exit(9);
                    return;
                }
                const auto changed = document->renderToImage();
                document->undo();
                if (changed == before || document->renderToImage() != before) {
                    qCritical() << "NkShot property close test: undo pixels differ";
                    application.exit(10);
                    return;
                }
                document->redo();
                if (document->renderToImage() != changed) {
                    application.exit(11);
                    return;
                }
                QMetaObject::invokeMethod(popup, "open");
                qInfo() << "NkShot actual QML property-change/close/undo/redo regression passed";
            }
            if (qEnvironmentVariableIsSet("NKSHOT_TEST_SOLID_COVER")) {
                auto document = SpectacleCore::instance()->annotationDocument();
                document->tool()->setType(AnnotationTool::RectangleTool);
                document->tool()->setStrokeWidth(4); document->tool()->setStrokeColor(Qt::red);
                document->tool()->setFillColor(Qt::transparent); document->tool()->setShadow(false);
                document->beginItem({80, 80}); document->continueItem({220, 160}); document->finishItem();
                document->selectItem({75, 75, 150, 90});
                auto selected = document->property("selectedItem").value<QObject *>();
                if (!selected || !selected->property("hasSelection").toBool()) { application.exit(151); return; }
                const auto before = document->renderToImage(); const auto depth = document->undoStackDepth();
                auto menu = viewer->rootObject()->findChild<QObject *>(QStringLiteral("nkshotExtraToolsMenu"));
                if (!menu || !QMetaObject::invokeMethod(menu, "open")) { application.exit(152); return; }
                QTest::qWait(200);
                auto preset = visualItem(viewer->contentItem(), QStringLiteral("nkshotSolidCoverPreset"));
                if (!preset) { application.exit(153); return; }
                QTest::mouseClick(viewer, Qt::LeftButton, Qt::NoModifier,
                    preset->mapToScene(QPointF(preset->width()/2, preset->height()/2)).toPoint());
                QTest::qWait(150);
                if (!samePixels(before, document->renderToImage()) || document->undoStackDepth() != depth
                    || selected->property("hasSelection").toBool() || document->tool()->fillColor() != QColor(Qt::black)
                    || document->tool()->hasShadow()) { application.exit(154); return; }
                auto viewport = visualItem(viewer->contentItem(), QStringLiteral("nkshotAnnotationViewport"));
                if (!viewport) { application.exit(155); return; }
                const auto start = viewport->mapToScene(QPointF(viewport->width()*0.15, viewport->height()*0.25)).toPoint();
                const auto end = viewport->mapToScene(QPointF(viewport->width()*0.65, viewport->height()*0.65)).toPoint();
                QTest::mousePress(viewer, Qt::LeftButton, Qt::NoModifier, start);
                QTest::mouseMove(viewer, end);
                QTest::mouseRelease(viewer, Qt::LeftButton, Qt::NoModifier, end);
                if (!QTest::qWaitFor([&] { return document->undoStackDepth() == depth + 1; }, 1000)) { application.exit(156); return; }
                const auto covered = document->renderToImage();
                QRect blackBounds; int count = 0;
                for (int y=0; y<covered.height(); ++y) for (int x=0; x<covered.width(); ++x) {
                    if (covered.pixelColor(x,y) == QColor(Qt::black) && before.pixelColor(x,y) != QColor(Qt::black)) {
                        blackBounds |= QRect(x,y,1,1); ++count;
                    }
                }
                if (count < 1000 || blackBounds.width() < 20 || blackBounds.height() < 20) { application.exit(157); return; }
                const auto inside = blackBounds.adjusted(3,3,-3,-3);
                for (int y=inside.top(); y<=inside.bottom(); ++y) for (int x=inside.left(); x<=inside.right(); ++x)
                    if (covered.pixelColor(x,y) != QColor(Qt::black)) { application.exit(158); return; }
                const auto margin = blackBounds.adjusted(-3,-3,3,3);
                for (int y=0; y<covered.height(); ++y) for (int x=0; x<covered.width(); ++x)
                    if (!margin.contains(x,y) && covered.pixelColor(x,y) != before.pixelColor(x,y)) { application.exit(159); return; }
                document->undo();
                if (!samePixels(document->renderToImage(), before)) { application.exit(160); return; }
                document->redo();
                if (!samePixels(document->renderToImage(), covered)) { application.exit(161); return; }
                QTest::qWait(400); // Let the guidance notification finish its layout transition before capture.
                qInfo() << "NkShot solid-cover preset preserves selected mark; actual drag opaque interior, outside pixels, undo/redo passed" << blackBounds;
            }
            if (qEnvironmentVariableIsSet("NKSHOT_TEST_COPY_SAVE")) {
                const auto output = SpectacleCore::instance()->outputUrl();
                if (!output.isLocalFile() || !QFileInfo(output.toLocalFile()).fileName().startsWith(QStringLiteral("nkshot-ui-test-"))) {
                    qCritical() << "NkShot export test needs explicit nkshot-ui-test-* output file";
                    application.exit(12);
                    return;
                }
                if (QFileInfo::exists(output.toLocalFile())) {
                    qCritical() << "NkShot export test requires a fresh output path";
                    application.exit(15);
                    return;
                }
                const auto expected = SpectacleCore::instance()->annotationDocument()->renderToImage();
                auto workbench = viewer->rootObject()->findChild<QQuickItem *>(QStringLiteral("nkshotWorkbench"));
                const QRectF before = workbench ? QRectF(workbench->position(), workbench->size()) : QRectF();
                viewer->copyImage();
                if (!QTest::qWaitFor([expected] {
                    return samePixels(QApplication::clipboard()->image(), expected);
                }, 1000)) {
                    qCritical() << "NkShot Copy action pixels differ";
                    application.exit(13);
                    return;
                }
                viewer->save();
                if (!QTest::qWaitFor([expected, output] {
                    return samePixels(QImage(output.toLocalFile()), expected);
                }, 1000)) {
                    qCritical() << "NkShot Save action pixels differ";
                    application.exit(14);
                    return;
                }
                qInfo() << "NkShot actual Copy and Save action pixel regressions passed";
                QTest::qWait(800);
                if (!workbench || QRectF(workbench->position(), workbench->size()) != before) {
                    qCritical() << "NkShot notifications changed workbench geometry";
                    application.exit(19);
                    return;
                }
                qInfo() << "NkShot notification workbench geometry remained stable" << before;
            }
            if (qEnvironmentVariableIsSet("NKSHOT_TEST_NOTIFICATIONS")) {
                auto model = InlineMessageModel::instance();
                model->clear();
                auto list = viewer->rootObject()->findChild<QQuickItem *>(QStringLiteral("nkshotNotifications"));
                auto workbench = viewer->rootObject()->findChild<QQuickItem *>(QStringLiteral("nkshotWorkbench"));
                const QRectF before(workbench->position(), workbench->size());
                const QUrl savedFile(QStringLiteral("file:///tmp/nkshot-ui-test-notification.png"));
                model->push(InlineMessageModel::Saved, QStringLiteral("Saved synthetic test image"), savedFile);
                QTest::qWait(400);
                auto copyLink = visualItem(viewer->contentItem(), QStringLiteral("nkshotNotificationCopy0"));
                auto folderLink = visualItem(viewer->contentItem(), QStringLiteral("nkshotNotificationFolder0"));
                if (!copyLink || !folderLink || !folderLink->isVisible()) { application.exit(63); return; }
                QTest::mouseClick(viewer, Qt::LeftButton, Qt::NoModifier,
                    copyLink->mapToScene(QPointF(copyLink->width()/2, copyLink->height()/2)).toPoint());
                if (QApplication::clipboard()->mimeData()->urls() != QList<QUrl>{savedFile}) { application.exit(64); return; }
                model->clear();
                QTest::qWait(400);
                for (int i = 0; i < 12; ++i) {
                    model->push(InlineMessageModel::Warning, QStringLiteral("Test warning %1: this message must remain available until dismissed.").arg(i));
                }
                QTest::qWait(700);
                if (!list || model->rowCount() != 12 || list->height() > list->property("maximumHeight").toReal() + 1
                    || list->property("contentHeight").toReal() <= list->height()) {
                    qCritical() << "NkShot notification overflow bounds failed";
                    application.exit(20);
                    return;
                }
                list->setProperty("contentY", list->property("contentHeight").toReal() - list->height());
                QTest::qWait(400);
                auto close = visualItem(viewer->contentItem(), QStringLiteral("nkshotCloseNotification11"));
                if (!close) {
                    qCritical() << "NkShot notification close target missing" << list->property("contentY") << list->property("contentHeight") << list->height();
                    for (auto item : viewer->rootObject()->findChildren<QQuickItem *>()) {
                        if (item->objectName().startsWith(QStringLiteral("nkshotCloseNotification"))) qInfo() << item->objectName() << item->mapToScene(QPointF());
                    }
                    application.exit(21); return;
                }
                QTest::mouseClick(viewer, Qt::LeftButton, Qt::NoModifier, close->mapToScene(QPointF(close->width()/2, close->height()/2)).toPoint());
                if (!QTest::qWaitFor([model] { return model->rowCount() == 11; }, 1000)) {
                    qCritical() << "NkShot notification actual close click failed";
                    application.exit(22);
                    return;
                }
                model->push(InlineMessageModel::Copied, QStringLiteral("Copied once"));
                model->push(InlineMessageModel::Copied, QStringLiteral("Copied again"));
                QTest::qWait(500);
                if (model->rowCount() != 12 || QRectF(workbench->position(), workbench->size()) != before) {
                    application.exit(23); return;
                }
                qInfo() << "NkShot notification overflow, actual dismiss, deduplication and stable geometry passed";
                if (qEnvironmentVariableIsSet("NKSHOT_TEST_NOTIFICATION_TIMEOUT")) {
                    list->setProperty("contentY", 0);
                    // The transient last row is outside the visible list area.
                    if (!QTest::qWaitFor([model] { return model->rowCount() == 11; }, 11000)) {
                        qCritical() << "NkShot offscreen transient notification failed to expire";
                        application.exit(24); return;
                    }
                    if (QRectF(workbench->position(), workbench->size()) != before) { application.exit(25); return; }
                    qInfo() << "NkShot offscreen transient timeout preserved all eleven persistent warnings";
                }

            }
            if (qEnvironmentVariableIsSet("NKSHOT_TEST_CONTEXT_MENU")) {
                auto spin = viewer->rootObject()->findChild<QObject *>(QStringLiteral("nkshotStrokeWidth"));
                auto editor = spin ? spin->property("contentItem").value<QQuickItem *>() : nullptr;
                if (!editor) {
                    qCritical() << "NkShot context menu test: missing text input";
                    application.exit(6);
                    return;
                }
                spin->setProperty("editable", true);
                auto eventWindow = editor->window();
                const QPointF local = editor->mapToScene(QPointF(editor->width() / 2, editor->height() / 2));
                QTest::mouseClick(eventWindow, Qt::RightButton, Qt::NoModifier, local.toPoint());
                if (!QTest::qWaitFor([] { return TextContextMenu::instance()->isVisible(); }, 1000)) {
                    qCritical() << "NkShot context menu test: right click did not open native menu";
                    application.exit(7);
                    return;
                }
                TextContextMenu::instance()->close();
                qInfo() << "NkShot actual right-button context-menu regression passed";
            }
            const QImage pixels = viewer->grabWindow();
            const bool saved = !pixels.isNull() && pixels.save(destination);
            if (saved) {
                qInfo() << "NkShot actual viewer capture:" << destination << pixels.size();
            } else {
                qCritical() << "NkShot UI capture failed:" << destination;
            }
            application.exit(saved ? 0 : 4);
        });
    });
}
