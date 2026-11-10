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
        QTest::qWait(50);
        const auto before = window.grabWindow();
        QVERIFY(!before.isNull());
        drawRectangle(document);
        QTest::qWait(50);
        const auto edited = window.grabWindow();
        QVERIFY(edited != before);
        document.undo();
        QTest::qWait(50);
        QCOMPARE(window.grabWindow(), before);
        document.redo();
        QTest::qWait(50);
        QCOMPARE(window.grabWindow(), edited);
    }
};
QTEST_MAIN(EditorDocumentTest)
#include "EditorDocumentTest.moc"
