// SPDX-License-Identifier: GPL-3.0-only
#include "UiCapture.h"
#include "Gui/ViewerWindow.h"
#include "Gui/PinnedImageWindow.h"
#include "Gui/OcrTextWindow.h"
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

    if (qEnvironmentVariableIsSet("NKSHOT_TEST_OCR_SAVE") || qEnvironmentVariableIsSet("NKSHOT_TEST_PIN_SAVE") || qEnvironmentVariableIsSet("NKSHOT_TEST_COMBINE")) {
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
                    auto parentMenu = viewer->rootObject()->findChild<QObject *>(QStringLiteral("nkshotMoreMenu"));
                    if (parentMenu) QMetaObject::invokeMethod(parentMenu, "open");
                    QTest::qWait(200);
                    auto menu = viewer->rootObject()->findChild<QObject *>(QStringLiteral("nkshotOcrLanguageMenu"));
                    if (!menu || !QMetaObject::invokeMethod(menu, "open")) { application.exit(108); return; }
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
                const auto top = files.filePath(QStringLiteral("frame2.png"));
                const auto bottom = files.filePath(QStringLiteral("frame10.png"));
                if (!files.isValid() || !expected.copy(0, 0, 320, stacking ? 300 : 360).save(top)
                    || !expected.copy(0, stacking ? 300 : 240, 320, stacking ? 300 : 360).save(bottom)) { application.exit(72); return; }
                auto menu = viewer->rootObject()->findChild<QObject *>(QStringLiteral("nkshotMoreMenu"));
                QMetaObject::invokeMethod(menu, "open"); QTest::qWait(200);
                auto entry = visualItem(viewer->contentItem(), stacking ? QStringLiteral("nkshotStackImagesAction") : QStringLiteral("nkshotCombineImagesAction"));
                if (!entry) { application.exit(73); return; }
                QTest::mouseClick(viewer, Qt::LeftButton, Qt::NoModifier, entry->mapToScene(QPointF(entry->width()/2, entry->height()/2)).toPoint());
                QTest::qWait(200);
                auto currentDialog = []() -> QFileDialog * {
                    for (auto widget : QApplication::topLevelWidgets())
                        if (widget->objectName() == QStringLiteral("nkshotCombineDialog")) return qobject_cast<QFileDialog *>(widget);
                    return nullptr;
                };
                QPointer<QFileDialog> dialog = currentDialog();
                if (!dialog) { application.exit(74); return; }
                viewer->combineImages(stacking);
                if (currentDialog() != dialog) { application.exit(75); return; }
                dialog->reject();
                if (!QTest::qWaitFor([&] { return dialog.isNull(); }, 1000) || viewer->combiningImages()) { application.exit(76); return; }
                viewer->combineImages(stacking); QTest::qWait(200);
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
                combined->resize(380, 520); QTest::qWait(250);
                if (!combined->grabWindow().save(destination)) { application.exit(80); return; }
                combined->close();
                if (!QTest::qWaitFor([&] { return combined.isNull(); }, 1000)) { application.exit(81); return; }
                if (stacking) {
                    qInfo() << "NkShot explicit stack exact pixels, sorted order, cancel/reopen/reuse and source isolation passed";
                    application.exit(0); return;
                }
                // Duplicate frames must reject rather than fabricate a long image.
                const auto duplicate = files.filePath(QStringLiteral("frame3.png"));
                if (!expected.copy(0, 0, 320, 360).save(duplicate)) { application.exit(82); return; }
                const auto beforeWarning = InlineMessageModel::instance()->rowCount();
                viewer->combineImages(); QTest::qWait(200);
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
                    QImage gap(320, 360, QImage::Format_RGBA8888); gap.fill(QColor(222, 222, 222));
                    QFile damaged(broken);
                    if (!expected.copy(0, 240, 319, 360).save(wrongWidth) || !gap.save(missingOverlap)
                        || !damaged.open(QIODevice::WriteOnly)) { application.exit(87); return; }
                    damaged.write("not an image"); damaged.close();
                    const QList<QPair<QStringList, QString>> errors = {
                        {{top, wrongWidth}, QStringLiteral("same width")},
                        {{top, broken}, QStringLiteral("could not be read")},
                        {{top}, QStringLiteral("Choose 2")},
                        {{top, missingOverlap}, QStringLiteral("No unchanged overlap")},
                    };
                    for (const auto &test : errors) {
                        auto messages = InlineMessageModel::instance(); messages->clear();
                        viewer->combineImages(); QTest::qWait(200); dialog = currentDialog();
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
