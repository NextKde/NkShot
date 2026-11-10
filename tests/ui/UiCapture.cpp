// SPDX-License-Identifier: GPL-3.0-only
#include "UiCapture.h"
#include "Gui/ViewerWindow.h"
#include "Gui/PinnedImageWindow.h"
#include "Gui/InlineMessageModel.h"
#include "Gui/CaptureWindow.h"
#include "Gui/SelectionEditor.h"
#include "Gui/Selection.h"
#include <QScreen>
#include <memory>
#include "Gui/TextContextMenu.h"
#include <QtTest/QTest>
#include "SpectacleCore.h"
#include <KQuickImageEditor/AnnotationDocument>
#include <KQuickImageEditor/AnnotationTool>

#include <QApplication>
#include <QDebug>
#include <QImage>
#include <QInputMethodEvent>
#include <QClipboard>
#include <QMimeData>
#include <QFileInfo>
#include <QFileDialog>
#include <QTemporaryDir>
#include <QLineEdit>
#include <QIcon>
#include <QDir>
#include <QStandardPaths>
#include <QPointer>
#include <QPalette>
#include <QQuickItem>
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


void installUiCaptureHook(QApplication &application)
{
    const QString destination = qEnvironmentVariable("NKSHOT_TEST_UI_CAPTURE");
    if (destination.isEmpty()) {
        return;
    }

    if (qEnvironmentVariableIsSet("NKSHOT_TEST_PIN_SAVE")) {
        // The harness drives widget filenames; production retains native dialogs.
        application.setAttribute(Qt::AA_DontUseNativeDialogs);
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
        const auto dimensions = qEnvironmentVariable("NKSHOT_TEST_UI_SIZE", QStringLiteral("960x660")).split(u'x');
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
            if (qEnvironmentVariableIsSet("NKSHOT_TEST_PIN")) {
                auto document = SpectacleCore::instance()->annotationDocument();
                const auto original = document->renderToImage();
                auto menu = viewer->rootObject()->findChild<QObject *>(QStringLiteral("nkshotMoreMenu"));
                if (!menu || !QMetaObject::invokeMethod(menu, "open")) { application.exit(41); return; }
                QTest::qWait(200);
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
                    QTest::qWaitFor([&] { return QFileInfo::exists(output); }, 2000);
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
                // Close immediately, including before Qt.callLater starts the commit timer.
                QMetaObject::invokeMethod(popup, "close");
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
