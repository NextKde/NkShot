// SPDX-License-Identifier: GPL-3.0-only
#include "UiCapture.h"
#include "Gui/ViewerWindow.h"
#include "Gui/TextContextMenu.h"
#include <QtTest/QTest>
#include "SpectacleCore.h"
#include <KQuickImageEditor/AnnotationDocument>
#include <KQuickImageEditor/AnnotationTool>

#include <QApplication>
#include <QDebug>
#include <QImage>
#include <QIcon>
#include <QDir>
#include <QStandardPaths>
#include <QPointer>
#include <QPalette>
#include <QQuickItem>
#include <QTimer>

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
        viewer->setAnnotating(qEnvironmentVariableIsSet("NKSHOT_TEST_EDITING"));
        const auto testTool = qEnvironmentVariable("NKSHOT_TEST_TOOL");
        if (testTool == QStringLiteral("text")) {
            SpectacleCore::instance()->annotationDocument()->tool()->setType(AnnotationTool::TextTool);
        } else if (testTool == QStringLiteral("rectangle")) {
            SpectacleCore::instance()->annotationDocument()->tool()->setType(AnnotationTool::RectangleTool);
        } else if (testTool == QStringLiteral("blur")) {
            SpectacleCore::instance()->annotationDocument()->tool()->setType(AnnotationTool::BlurTool);
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
