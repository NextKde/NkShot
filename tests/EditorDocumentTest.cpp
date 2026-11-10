// SPDX-License-Identifier: GPL-3.0-only
#include <KQuickImageEditor/AnnotationDocument>
#include <KQuickImageEditor/AnnotationTool>
#include "Gui/PortableAnnotationViewport.h"
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTemporaryDir>
#include <QtTest>

class EditorDocumentTest : public QObject
{
    Q_OBJECT
    static void drawRectangle(AnnotationDocument &document)
    {
        document.tool()->setType(AnnotationTool::RectangleTool);
        document.tool()->setStrokeColor(Qt::blue);
        document.tool()->setStrokeWidth(4);
        document.beginItem({20, 20});
        document.continueItem({70, 60});
        document.finishItem();
    }
private Q_SLOTS:
    void editUndoRedoCropSave()
    {
        AnnotationDocument document;
        QImage source(100, 80, QImage::Format_ARGB32_Premultiplied);
        source.fill(Qt::white);
        document.setBaseImage(source);
        const auto before = document.renderToImage();
        drawRectangle(document);
        const auto edited = document.renderToImage();
        QVERIFY(edited != before);
        QCOMPARE(document.undoStackDepth(), 1);
        document.undo();
        QCOMPARE(document.renderToImage(), before);
        document.redo();
        QCOMPARE(document.renderToImage(), edited);
        document.cropCanvas({10, 10, 70, 55});
        const auto cropped = document.renderToImage();
        QCOMPARE(cropped.size(), QSize(70, 55));
        QCOMPARE(cropped, edited.copy(10, 10, 70, 55));
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath(QStringLiteral("edited.png"));
        QVERIFY(document.saveImage(path));
        QCOMPARE(QImage(path).convertToFormat(cropped.format()), cropped);
        document.undo();
        QCOMPARE(document.renderToImage(), edited);
    }
    void mouseGestureAtScale_data()
    {
        QTest::addColumn<int>("tool");
        QTest::addColumn<qreal>("zoom");
        QTest::addColumn<qreal>("imageDpr");
        for (qreal zoom : {0.5, 1.0, 2.0}) {
            for (qreal dpr : {1.0, 1.25, 1.5, 2.0}) {
                for (auto tool : {AnnotationTool::RectangleTool, AnnotationTool::BlurTool, AnnotationTool::PixelateTool}) {
                    const auto name = QStringLiteral("tool-%1-zoom-%2-imageDpr-%3").arg(int(tool)).arg(zoom).arg(dpr).toLatin1();
                    QTest::newRow(name.constData()) << int(tool) << zoom << dpr;
                }
            }
        }
    }
    void mouseGestureAtScale()
    {
        QFETCH(int, tool);
        QFETCH(qreal, zoom);
        QFETCH(qreal, imageDpr);
        AnnotationDocument document;
        QImage source(QSize(100 * imageDpr, 80 * imageDpr), QImage::Format_ARGB32_Premultiplied);
        source.setDevicePixelRatio(imageDpr);
        for (int y = 0; y < source.height(); ++y) {
            for (int x = 0; x < source.width(); ++x) {
                source.setPixelColor(x, y, QColor((x * 37 + y * 13) % 256, (x * 17) % 256, (y * 29) % 256));
            }
        }
        document.setBaseImage(source);
        const auto before = document.renderToImage();
        QQuickWindow window;
        window.resize(260, 220);
        PortableAnnotationViewport viewport(window.contentItem());
        viewport.setTransformOrigin(QQuickItem::TopLeft);
        viewport.setPosition({20, 20});
        viewport.setSize({100, 80});
        viewport.setScale(zoom);
        viewport.setViewportRect({0, 0, 100, 80});
        viewport.setDocument(&document);
        document.tool()->setType(static_cast<AnnotationTool::Tool>(tool));
        document.tool()->setStrokeColor(Qt::blue);
        document.tool()->setStrokeWidth(4);
        document.tool()->setStrength(0.6);
        document.tool()->setShadow(false); // Isolate geometry from shadow rasterization.
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        const auto start = viewport.mapToScene({20, 20}).toPoint();
        const auto end = viewport.mapToScene({70, 60}).toPoint();
        QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(&window, end);
        QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, end);
        QTRY_COMPARE(document.undoStackDepth(), 1);
        const auto edited = document.renderToImage();
        QVERIFY(edited != before);
        // Independent document-level gesture checks scene-to-image coordinates,
        // not merely that some pixels changed after a mouse event.
        AnnotationDocument expected;
        expected.setBaseImage(source);
        expected.tool()->setType(static_cast<AnnotationTool::Tool>(tool));
        expected.tool()->setStrokeColor(Qt::blue);
        expected.tool()->setStrokeWidth(4);
        expected.tool()->setStrength(0.6);
        expected.tool()->setShadow(false);
        // AnnotationViewport rounds local input to the window's device pixels.
        const auto snapped = [&window](QPointF point) {
            const auto dpr = window.devicePixelRatio();
            return QPointF(qRound(point.x() * dpr) / dpr, qRound(point.y() * dpr) / dpr);
        };
        expected.beginItem(snapped({20, 20}));
        expected.continueItem(snapped({70, 60}));
        expected.finishItem();
        expected.deselectItem(); // Mouse release deselects creation tools too.
        QCOMPARE(edited, expected.renderToImage());
        document.undo();
        QCOMPARE(document.renderToImage(), before);
        document.redo();
        QCOMPARE(document.renderToImage(), edited);
    }
    void basicToolsUndoRedo_data()
    {
        QTest::addColumn<int>("tool");
        QTest::newRow("freehand") << int(AnnotationTool::FreehandTool);
        QTest::newRow("highlighter") << int(AnnotationTool::HighlighterTool);
        QTest::newRow("line") << int(AnnotationTool::LineTool);
        QTest::newRow("arrow") << int(AnnotationTool::ArrowTool);
        QTest::newRow("ellipse") << int(AnnotationTool::EllipseTool);
    }
    void basicToolsUndoRedo()
    {
        QFETCH(int, tool);
        AnnotationDocument document;
        QImage source(100, 80, QImage::Format_ARGB32_Premultiplied);
        source.fill(Qt::white);
        document.setBaseImage(source);
        const auto before = document.renderToImage();
        document.tool()->setType(static_cast<AnnotationTool::Tool>(tool));
        document.tool()->setStrokeColor(Qt::blue);
        document.tool()->setStrokeWidth(6);
        document.beginItem({20, 20});
        document.continueItem({40, 30});
        document.continueItem({70, 60});
        document.finishItem();
        const auto edited = document.renderToImage();
        QVERIFY(edited != before);
        QCOMPARE(document.undoStackDepth(), 1);
        document.undo();
        QCOMPARE(document.renderToImage(), before);
        document.redo();
        QCOMPARE(document.renderToImage(), edited);
    }
    void viewportDocumentReplacement()
    {
        // Documents outlive the viewport; upstream setDocument(nullptr) is not supported.
        AnnotationDocument first;
        AnnotationDocument second;
        QImage white(100, 80, QImage::Format_ARGB32_Premultiplied);
        white.fill(Qt::white);
        QImage red = white;
        red.fill(Qt::red);
        first.setBaseImage(white);
        second.setBaseImage(red);
        QQuickWindow window;
        window.resize(100, 80);
        PortableAnnotationViewport viewport(window.contentItem());
        viewport.setSize({100, 80});
        viewport.setViewportRect({0, 0, 100, 80});
        viewport.setDocument(&first);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTRY_VERIFY(!window.grabWindow().isNull());
        QTRY_COMPARE(window.grabWindow().pixelColor(10, 10), QColor(Qt::white));
        viewport.setDocument(&second);
        QTRY_COMPARE(window.grabWindow().pixelColor(10, 10), QColor(Qt::red));
        const auto secondFrame = window.grabWindow();
        drawRectangle(first);
        QCoreApplication::processEvents();
        QCOMPARE(window.grabWindow(), secondFrame);
        drawRectangle(second);
        QTRY_VERIFY(window.grabWindow() != secondFrame);
        second.undo();
        QTRY_COMPARE(window.grabWindow(), secondFrame);
        viewport.setDocument(&first);
        QTRY_VERIFY(window.grabWindow() != secondFrame);
        first.undo();
        QTRY_COMPARE(window.grabWindow().pixelColor(10, 10), QColor(Qt::white));
    }
    void softwareViewportRepaints()
    {
        QQuickWindow window;
        window.resize(100, 80);
        AnnotationDocument document;
        QImage source(100, 80, QImage::Format_ARGB32_Premultiplied);
        source.fill(Qt::white);
        document.setBaseImage(source);
        PortableAnnotationViewport viewport(window.contentItem());
        viewport.setSize({100, 80});
        viewport.setViewportRect({0, 0, 100, 80});
        viewport.setDocument(&document);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTRY_VERIFY(!window.grabWindow().isNull());
        QTRY_COMPARE(window.grabWindow().pixelColor(10, 10), QColor(Qt::white));
        const auto before = window.grabWindow();
        QVERIFY(!before.isNull());
        drawRectangle(document);
        QTRY_VERIFY(window.grabWindow() != before);
        const auto edited = window.grabWindow();
        QVERIFY(edited != before);
        document.undo();
        QTRY_COMPARE(window.grabWindow(), before);
        document.redo();
        QTRY_COMPARE(window.grabWindow(), edited);
    }
};
QTEST_MAIN(EditorDocumentTest)
#include "EditorDocumentTest.moc"
