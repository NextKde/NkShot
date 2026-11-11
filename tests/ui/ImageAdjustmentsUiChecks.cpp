// SPDX-License-Identifier: GPL-3.0-only
#include "ImageAdjustmentsUiChecks.h"
#include "Gui/ImageCopyEditor.h"
#include "Gui/ImageAdjustmentsPreview.h"
#include "Gui/PinnedImageWindow.h"
#include "Imaging/ImageAdjustments.h"
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QPointer>
#include <QQuickItem>
#include <QtTest/QTest>
namespace {
QQuickItem *find(QQuickItem *root,const QString &name) {
    if(!root)return nullptr;
    if(root->objectName()==name)return root;
    for(auto child:root->childItems())if(auto found=find(child,name))return found;
    return nullptr;
}
}
bool ImageAdjustmentsUiChecks::run(QApplication &app,ImageCopyEditor *raw,const QString &prefix,QString *failure) {
    QPointer<ImageCopyEditor> editor=raw;
    auto fail=[&](const QString &message){if(failure)*failure=message;if(editor)editor->grabWindow().save(prefix+QStringLiteral(".adjust-failed.png"));return false;};
    if(!editor||editor->annotationMode())return fail(QStringLiteral("Image adjustment requires a live image editor"));
    auto document=editor->document(); const auto before=document->renderToImage().copy(),source=document->baseImage().copy();
    const auto history=document->undoStackDepth();const bool dirty=editor->hasUnsavedChanges();
    auto unchanged=[&]{return editor&&document->renderToImage()==before&&document->baseImage()==source&&document->undoStackDepth()==history&&editor->hasUnsavedChanges()==dirty;};
    auto preview=qobject_cast<ImageAdjustmentsPreview *>(editor->adjustmentPreviewObject());
    if(!preview)return fail(QStringLiteral("Adjustment preview unavailable"));
    auto item=[&](const QString &name){return editor?find(editor->contentItem(),name):nullptr;};
    auto visible=[&](const QString &name){auto p=item(name);return p&&p->isVisible();};
    auto expose=[&](QQuickItem *target){
        auto scroll=item(QStringLiteral("nkshotImageAdjustmentsScroll"));
        auto flick=scroll?qvariant_cast<QQuickItem *>(scroll->property("contentItem")):nullptr;
        if(flick&&target) {
            const auto relative=target->mapToItem(flick,QPointF(0,0));
            const double y=flick->property("contentY").toDouble();
            if(relative.y()<0||relative.y()+target->height()>flick->height())
                flick->setProperty("contentY",qBound(0.0,y+relative.y()-flick->height()/2,qMax(0.0,flick->property("contentHeight").toDouble()-flick->height())));
        }
        QTest::qWait(40);
    };
    auto click=[&](const QString &name){auto target=item(name);if(!target||!target->isVisible()||!target->isEnabled())return false;expose(target);
        const auto point=target->mapToScene(QPointF(target->width()/2,target->height()/2)).toPoint();
        if(!QRect(QPoint(),editor->size()).contains(point))return false;
        QTest::mouseClick(editor,Qt::LeftButton,Qt::NoModifier,point);QTest::qWait(80);return true;};
    auto open=[&]{editor->requestActivate();QTest::qWait(80);
        if(!click(QStringLiteral("nkshotCopyEditorMore"))||!click(QStringLiteral("nkshotImageAdjustmentsAction")))return false;
        return QTest::qWaitFor([&]{return editor->adjustmentBlocked()&&visible(QStringLiteral("nkshotImageAdjustmentsCreate"))&&preview->ready();},2000);};
    auto zero=[&]{const auto a=preview->options();return a.brightness==0&&a.contrast==0&&a.saturation==0;};
    auto change=[&](const QString &name){auto slider=item(name);if(!slider)return false;expose(slider);
        const auto old=slider->property("value").toInt();
        QTest::mouseClick(editor,Qt::LeftButton,Qt::NoModifier,slider->mapToScene(QPointF(slider->width()*(name.contains(QStringLiteral("Brightness"))?.42:name.contains(QStringLiteral("Contrast"))?.56:.62),slider->height()/2)).toPoint());
        slider->forceActiveFocus();const auto clicked=slider->property("value").toInt();
        QTest::keyClick(editor,Qt::Key_Right);QTest::qWait(180);
        return clicked!=old&&slider->property("value").toInt()>clicked;};
    if(!open()||!zero())return fail(QStringLiteral("Actual adjustments menu did not open/reset"));
    if(!change(QStringLiteral("nkshotImageAdjustmentBrightness")))return fail(QStringLiteral("Brightness mouse/keyboard adjustment failed"));
    app.clipboard()->setText(QStringLiteral("adjustment-shortcut-sentinel"));
    QTest::keyClick(editor,Qt::Key_C,Qt::ControlModifier);QTest::keyClick(editor,Qt::Key_S,Qt::ControlModifier);QTest::qWait(100);
    if(app.clipboard()->text()!=QStringLiteral("adjustment-shortcut-sentinel")||!unchanged())return fail(QStringLiteral("Popup Copy/Save shortcut escaped"));
    for(auto window:app.topLevelWidgets())if(qobject_cast<QFileDialog *>(window)&&window->isVisible())return fail(QStringLiteral("Popup Save opened a file dialog"));
    if(!click(QStringLiteral("nkshotImageAdjustmentsReset"))||!zero()||!unchanged())return fail(QStringLiteral("Adjustment Reset changed source or retained values"));
    if(!change(QStringLiteral("nkshotImageAdjustmentContrast"))||!click(QStringLiteral("nkshotImageAdjustmentsCancel")))return fail(QStringLiteral("Adjustment Cancel workflow failed"));
    if(!QTest::qWaitFor([&]{return !editor->adjustmentBlocked();},1000)||!unchanged()||!open()||!zero())return fail(QStringLiteral("Cancel/reopen did not restore clean preview"));
    QTest::keyClick(editor,Qt::Key_Escape);QTest::qWait(120);
    if(editor->adjustmentBlocked()||!unchanged()||!open()||!zero())return fail(QStringLiteral("Escape/reopen failed"));
    for(const auto &name:{QStringLiteral("nkshotImageAdjustmentBrightness"),QStringLiteral("nkshotImageAdjustmentContrast"),QStringLiteral("nkshotImageAdjustmentSaturation")})
        if(!change(name))return fail(QStringLiteral("Slider input failed: ")+name);
    const auto expected=NkShot::Imaging::adjustedCopy(before,preview->options());
    if(expected.isNull()||expected==before)return fail(QStringLiteral("Adjusted expected image is invalid/unchanged"));
    editor->grabWindow().save(prefix+QStringLiteral(".adjustments.png"));
    if (auto scroll=item(QStringLiteral("nkshotImageAdjustmentsScroll"))) {
        if(auto flick=qvariant_cast<QQuickItem *>(scroll->property("contentItem"))) {
            flick->setProperty("contentY",0);QTest::qWait(100);
            editor->grabWindow().save(prefix+QStringLiteral(".adjustments-preview.png"));
        }
    }
    bool busyObserved=false,duplicateRejected=false;
    const auto connection=QObject::connect(editor,&ImageCopyEditor::adjustmentStateChanged,editor,[&]{
        if(!busyObserved&&preview->ready()&&editor->adjustmentBlocked()) {busyObserved=true;duplicateRejected=!editor->createAdjustedCopy();}
    });
    const bool clicked=click(QStringLiteral("nkshotImageAdjustmentsCreate"));QObject::disconnect(connection);
    if(!clicked||!busyObserved||!duplicateRejected)return fail(QStringLiteral("Create copy did not reject duplicate busy submission"));
    QPointer<PinnedImageWindow> pin;
    if(!QTest::qWaitFor([&]{for(auto window:app.topLevelWindows())if(window->objectName()==QStringLiteral("nkshotAdjustedCopyPin")) {
        pin=qobject_cast<PinnedImageWindow *>(window);if(pin&&pin->isVisible())return true;}return false;},15000))return fail(QStringLiteral("Adjusted copy pin did not appear"));
    if(pin->snapshot()!=expected||pin->snapshot().devicePixelRatio()!=expected.devicePixelRatio()||!unchanged())return fail(QStringLiteral("Adjusted pin differs from full-resolution snapshot or mutated editor"));
    pin->grabWindow().save(prefix+QStringLiteral(".adjusted-copy.png"));pin->close();
    if(!QTest::qWaitFor([&]{return pin.isNull();},1500))return fail(QStringLiteral("Adjusted pin failed to close"));
    if(editor->adjustmentBlocked()||!unchanged())return fail(QStringLiteral("Adjustment session remained blocked or changed source"));
    qInfo()<<"109 actual menu/sliders/reset/cancel/Escape/shortcut protection/async duplicate guard/exact copy passed";
    return true;
}
