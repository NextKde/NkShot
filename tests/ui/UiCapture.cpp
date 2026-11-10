// SPDX-License-Identifier: GPL-3.0-only
#include "UiCapture.h"
#include "Gui/ViewerWindow.h"
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
#include <QClipboard>
#include <QFileInfo>
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
