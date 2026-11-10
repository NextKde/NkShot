// SPDX-License-Identifier: GPL-3.0-only
#include <KQuickImageEditor/AnnotationDocument>
#include <KQuickImageEditor/AnnotationTool>
#include <QtTest>

class ShadowRepaintTest : public QObject
{
    Q_OBJECT
    static QImage render(qreal dpr, const QRectF &rect, bool intermediate)
    {
        AnnotationDocument document;
        QImage source(QSize(160 * dpr, 120 * dpr), QImage::Format_ARGB32_Premultiplied);
        source.setDevicePixelRatio(dpr);
        source.fill(Qt::white);
        document.setBaseImage(source);
        if (intermediate) {
            document.renderToImage();
        }
        document.tool()->setType(AnnotationTool::RectangleTool);
        document.tool()->setStrokeColor(Qt::blue);
        document.tool()->setStrokeWidth(4);
        document.tool()->setShadow(true);
        document.beginItem(rect.topLeft());
        document.continueItem(rect.bottomRight());
        document.finishItem();
        document.deselectItem();
        return document.renderToImage();
    }
private Q_SLOTS:
    void incrementalEqualsComplete()
    {
        for (qreal dpr : {1., 1.25, 1.5, 2.}) {
            for (const QPointF origin : {QPointF(20, 20), QPointF(20.25, 20.75), QPointF(19.5, 21.5)}) {
                for (const QSizeF size : {QSizeF(50, 40), QSizeF(47, 33), QSizeF(10.5, 11.25), QSizeF(51.75, 39.5)}) {
                    const QRectF rect(origin, size);
                    QCOMPARE(render(dpr, rect, true), render(dpr, rect, false));
                }
            }
        }
    }
};
QTEST_MAIN(ShadowRepaintTest)
#include "ShadowRepaintTest.moc"
