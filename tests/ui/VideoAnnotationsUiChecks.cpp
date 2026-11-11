// SPDX-License-Identifier: GPL-3.0-only
#include "VideoAnnotationsUiChecks.h"
#include "Gui/ViewerWindow.h"
#include "Gui/ImageCopyEditor.h"
#include "Media/VideoClipJob.h"
#include "SpectacleCore.h"
#include <KQuickImageEditor/AnnotationTool>
#include <QApplication>
#include <QClipboard>
#include <QQuickItem>
#include <QWheelEvent>
#include <QSignalSpy>
#include <QtTest/QTest>

namespace {
QQuickItem *find(QQuickItem *root,const QString &name) {
    if (!root) return nullptr;
    if (root->objectName() == name) return root;
    for(auto item:root->childItems())if(auto result=find(item,name))return result;
    return nullptr;
}
bool click(QQuickWindow *window,const QString &name) {
    auto item=find(window->contentItem(),name);
    if(!item||!item->isVisible()||!item->isEnabled())return false;
    const auto p=item->mapToScene(QPointF(item->width()/2,item->height()/2));
    if(!QRectF(QPointF(),window->size()).contains(p))return false;
    // Wheel/scrollbar gestures may still be decelerating. A press while a
    // Flickable is moving intentionally stops scrolling instead of clicking.
    for(auto parent=item->parentItem();parent;parent=parent->parentItem()) {
        if(parent->property("moving").isValid()
            &&!QTest::qWaitFor([&]{return !parent->property("moving").toBool();},4000))return false;
    }
    window->requestActivate();
    if(!QTest::qWaitFor([&]{return QGuiApplication::focusWindow()==window;},1000))return false;
    QSignalSpy activated(item,SIGNAL(clicked()));
    const auto target=item->mapToScene(QPointF(item->width()/2,item->height()/2)).toPoint();
    QTest::mouseMove(window,target);QTest::qWait(100);
    QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,target,50);
    return QTest::qWaitFor([&]{return !activated.isEmpty();},500);
}
bool detailAction(ViewerWindow *viewer,const QString &name) {
    viewer->requestActivate();QTest::qWait(80);
    auto popup=viewer->findChild<QObject *>(QStringLiteral("nkshotClipPopup"));
    // Popup QObject ownership is under the recording page, not necessarily the window.
    if(!popup){auto details=find(viewer->contentItem(),QStringLiteral("nkshotClipDetails"));if(!details)return false;
        if(!details->property("checked").toBool()&&!click(viewer,QStringLiteral("nkshotClipDetails")))return false;
    } else if(!popup->property("detailsVisible").toBool()&&!click(viewer,QStringLiteral("nkshotClipDetails")))return false;
    QTest::qWait(150);
    auto scroll=find(viewer->contentItem(),QStringLiteral("nkshotClipDetailScroll"));
    auto target=find(viewer->contentItem(),name);
    if(!scroll||!target)return false;
    const auto top=scroll->mapToScene(QPointF());const QRectF bounds(top,scroll->size());
    const auto wheelAt=scroll->mapToScene(QPointF(scroll->width()/2,scroll->height()/2));
    for(int attempt=0;attempt<8;++attempt){
        const auto point=target->mapToScene(QPointF(target->width()/2,target->height()/2));
        if(bounds.adjusted(0,1,0,-1).contains(QRectF(target->mapToScene(QPointF()),target->size())))return click(viewer,name);
        QWheelEvent wheel(wheelAt,viewer->mapToGlobal(wheelAt.toPoint()),QPoint(),QPoint(0,point.y() + target->height()/2 > bounds.bottom()?-120:120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
        QCoreApplication::sendEvent(viewer,&wheel);QTest::qWait(100);
    }
    // Mouse-based offscreen ScrollView can leave wheel events unhandled. Exercise
    // the visible scrollbar itself, just as a pointer user can.
    if (auto bar = find(viewer->contentItem(), QStringLiteral("nkshotClipDetailScrollbar"))) {
        const qreal position = bar->property("position").toReal();
        const qreal size = bar->property("size").toReal();
        const auto from = bar->mapToScene(QPointF(bar->width()/2, (position + size/2) * bar->height())).toPoint();
        const auto to = bar->mapToScene(QPointF(bar->width()/2, bar->height()-2)).toPoint();
        QTest::mousePress(viewer, Qt::LeftButton, Qt::NoModifier, from);
        QTest::mouseMove(viewer, to, 100);
        QTest::mouseRelease(viewer, Qt::LeftButton, Qt::NoModifier, to);
        QTest::qWait(150);
        if (bounds.adjusted(0,1,0,-1).contains(QRectF(target->mapToScene(QPointF()), target->size())))
            return click(viewer, name);
    }
    return false;
}
}
QString runVideoAnnotationsUiChecks(ViewerWindow *viewer,QVideoSink *sink,const QString &evidence) {
    const auto core=SpectacleCore::instance();const auto source=core->currentVideo();auto job=viewer->clipJob();
    const auto original=core->annotationDocument()->renderToImage();const auto depth=core->annotationDocument()->undoStackDepth();
    if(!job->annotationAvailable()||job->hasAnnotationOverlay()||!detailAction(viewer,QStringLiteral("nkshotClipAnnotations"))){ viewer->grabWindow().save(evidence+QStringLiteral(".annotation-failed.png")); return QStringLiteral("Annotation menu unavailable"); }
    QPointer<ImageCopyEditor> editor;
    if(!QTest::qWaitFor([&]{for(auto window:QGuiApplication::allWindows())if(window->objectName()==QStringLiteral("nkshotVideoAnnotationEditor"))editor=qobject_cast<ImageCopyEditor *>(window);return editor&&editor->isVisible()&&editor->status()==QQuickView::Ready;},2000))return QStringLiteral("Video editor did not open");
    editor->resize(qEnvironmentVariableIsSet("NKSHOT_TEST_COMPACT")?QSize(420,520):QSize(900,640));QTest::qWait(150);
    if(!editor->annotationMode()||editor->document()==core->annotationDocument()||viewer->editVideoAnnotations(sink,source)!=editor)return QStringLiteral("Video editor ownership/session mismatch");
    if(viewer->editVideoAnnotations(nullptr,source)||viewer->editVideoAnnotations(sink,QUrl::fromLocalFile(QStringLiteral("/retired.mp4"))))return QStringLiteral("Retired video editor accepted");
    for(const auto name:{"nkshotCopyEditorCopy","nkshotCopyEditorSave","nkshotCropTool","nkshotPixelateTool"}){
        auto item=find(editor->contentItem(),QString::fromLatin1(name));if(item&&item->isVisible())return QStringLiteral("Unsafe image action visible in video editor");
    }
    for(const auto tool:{AnnotationTool::BlurTool,AnnotationTool::PixelateTool,AnnotationTool::CropTool}){
        editor->document()->tool()->setType(tool);if(editor->document()->tool()->type()!=AnnotationTool::SelectTool)return QStringLiteral("Forbidden video tool not guarded");
    }
    if(!click(editor,QStringLiteral("nkshotFreehandTool")))return QStringLiteral("Video freehand unavailable");
    editor->document()->tool()->setStrokeColor(Qt::white);editor->document()->tool()->setStrokeWidth(10);
    auto viewport=find(editor->contentItem(),QStringLiteral("nkshotAnnotationViewport"));if(!viewport)return QStringLiteral("Video viewport missing");
    const auto size=job->sourceSize();const QPointF a(size.width()*.15,size.height()*.15),b(size.width()*.8,size.height()*.25);
    QTest::mousePress(editor,Qt::LeftButton,Qt::NoModifier,viewport->mapToScene(a).toPoint());
    for(int i=1;i<=12;++i)QTest::mouseMove(editor,viewport->mapToScene(a+(b-a)*(i/12.)).toPoint(),15);
    QTest::mouseRelease(editor,Qt::LeftButton,Qt::NoModifier,viewport->mapToScene(b).toPoint());
    if(!QTest::qWaitFor([&]{return editor->document()->undoStackDepth()>0;},1000))return QStringLiteral("Actual annotation gesture created no history");
    const auto history=editor->document()->undoStackDepth();
    if(!editor->grabWindow().save(evidence+QStringLiteral(".annotation-editor.png"))||!click(editor,QStringLiteral("nkshotCopyEditorDone")))return QStringLiteral("Annotation Done unavailable");
    if(!QTest::qWaitFor([&]{return editor&&!editor->isVisible()&&job->hasAnnotationOverlay();},1000))return QStringLiteral("Annotation commit did not preserve hidden editor");
    const auto overlay=job->annotationOverlay();const auto revision=job->annotationRevision();
    if(overlay.size()!=size||!overlay.hasAlphaChannel()||qAlpha(overlay.pixel(0,0))!=0)return QStringLiteral("Video frame leaked into annotation layer");
    if(viewer->editVideoAnnotations(sink,source)!=editor||editor->document()->undoStackDepth()!=history)return QStringLiteral("Annotation reopen lost editable history");
    QTest::qWait(100);
    if(!click(editor,QStringLiteral("nkshotUndo"))||!click(editor,QStringLiteral("nkshotCopyEditorDone"))||job->hasAnnotationOverlay())return QStringLiteral("Undo to empty did not clear overlay");
    if(viewer->editVideoAnnotations(sink,source)!=editor)return QStringLiteral("Annotation redo session missing");
    QTest::qWait(100);
    if(!click(editor,QStringLiteral("nkshotRedo")))return QStringLiteral("Annotation redo unavailable");
    editor->close();
    if(!QTest::qWaitFor([&]{return editor&&!editor->isVisible()&&job->hasAnnotationOverlay();},1000)||job->annotationOverlay()!=overlay||job->annotationRevision()<=revision)return QStringLiteral("Window close did not commit/reuse annotations");
    viewer->editVideoAnnotations(sink,source);QTest::qWait(100);QSignalSpy commits(editor,&ImageCopyEditor::annotationsCommitted);
    editor->document()->cropCanvas(QRectF(0,0,size.width()/2,size.height()/2));editor->finishCopy();
    if(!commits.isEmpty()||!editor->isVisible()||job->annotationOverlay()!=overlay)return QStringLiteral("Invalid frame transform reached video export");
    editor->document()->undo();editor->finishCopy();
    if(editor->isVisible()||job->annotationOverlay()!=overlay||core->currentVideo()!=source||core->annotationDocument()->renderToImage()!=original||core->annotationDocument()->undoStackDepth()!=depth)return QStringLiteral("Video annotation isolation/invalid edit recovery failed");
    if(!detailAction(viewer,QStringLiteral("nkshotClipClearAnnotations"))||editor||job->hasAnnotationOverlay())return QStringLiteral("Clear annotations did not end editable session");
    editor=qobject_cast<ImageCopyEditor *>(viewer->editVideoAnnotations(sink,source));
    if(!editor||editor->document()->undoStackDepth()!=0)return QStringLiteral("Fresh annotation editor retained old history");
    QTest::qWait(150);editor->document()->tool()->setType(AnnotationTool::FreehandTool);
    editor->document()->tool()->setStrokeColor(Qt::white);editor->document()->tool()->setStrokeWidth(10);
    viewport=find(editor->contentItem(),QStringLiteral("nkshotAnnotationViewport"));if(!viewport)return QStringLiteral("Fresh viewport missing");
    QTest::mousePress(editor,Qt::LeftButton,Qt::NoModifier,viewport->mapToScene(a).toPoint());
    for(int i=1;i<=12;++i)QTest::mouseMove(editor,viewport->mapToScene(a+(b-a)*(i/12.)).toPoint(),15);
    QTest::mouseRelease(editor,Qt::LeftButton,Qt::NoModifier,viewport->mapToScene(b).toPoint());editor->finishCopy();
    if(editor->isVisible()||!job->hasAnnotationOverlay())return QStringLiteral("Fresh annotation session did not commit");
    auto details=find(viewer->contentItem(),QStringLiteral("nkshotClipDetails"));
    if(details&&details->property("checked").toBool()&&!click(viewer,QStringLiteral("nkshotClipDetails")))return QStringLiteral("Annotation details failed to collapse");
    viewer->requestActivate();QTest::qWait(150);
    auto preview=find(viewer->contentItem(),QStringLiteral("nkshotVideoAnnotationOverlay"));
    if(!preview||!preview->isVisible()||preview->property("image").value<QImage>()!=job->annotationOverlay())return QStringLiteral("Main video annotation preview missing");
    QTest::qWait(150);if(!viewer->grabWindow().save(evidence+QStringLiteral(".annotations.png")))return QStringLiteral("Annotation preview evidence failed");
    return {};
}
