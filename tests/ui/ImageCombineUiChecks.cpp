// SPDX-License-Identifier: GPL-3.0-only
#include "ImageCombineUiChecks.h"
#include "Gui/ImageCombineWindow.h"
#include "Gui/ImageCombineSession.h"
#include "Gui/PinnedImageWindow.h"
#include "Gui/ViewerWindow.h"
#include "SpectacleCore.h"
#include "Imaging/VerticalStitcher.h"
#include <KQuickImageEditor/AnnotationDocument>
#include <QApplication>
#include <QClipboard>
#include <QCryptographicHash>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QDir>
#include <QLineEdit>
#include <QPointer>
#include <QQuickItem>
#include <QWheelEvent>
#include <QtTest/QTest>
namespace {
QQuickItem *find(QQuickItem *root,const QString &name){
    if(!root)return nullptr;
    if(root->objectName()==name)return root;
    for(auto child:root->childItems())if(auto found=find(child,name))return found;
    return nullptr;
}
ImageCombineWindow *review(){
    for(auto window:QGuiApplication::allWindows())if(auto result=qobject_cast<ImageCombineWindow *>(window))return result;
    return nullptr;
}
bool click(QQuickWindow *window,const char *name){
    QPointer<QQuickItem> target;
    if(!QTest::qWaitFor([&]{target=find(window->contentItem(),QString::fromLatin1(name));return target&&target->isVisible()&&target->isEnabled();},1500)) {
        qWarning()<<"Combine click unavailable"<<name<<target.data();return false;
    }
    QRectF previous;int stable=0;
    for(int attempt=0;attempt<30&&stable<3;++attempt){
        target=find(window->contentItem(),QString::fromLatin1(name));
        if(!target)return false;
        const QRectF current(target->mapToScene(QPointF()),target->size());
        stable=current==previous&&current.width()>0&&current.height()>0?stable+1:0;
        previous=current;QTest::qWait(30);
    }
    if(stable<3){qWarning()<<"Combine click layout did not settle"<<name<<previous;return false;}
    for(auto p=target->parentItem();p;p=p->parentItem()){
        if(!p->property("contentY").isValid())continue;
        const QRectF bounds(p->mapToScene(QPointF()),p->size());
        for(int attempt=0;attempt<16&&!bounds.contains(QRectF(target->mapToScene(QPointF()),target->size()));++attempt){
            const auto at=p->mapToScene(QPointF(p->width()/2,p->height()/2));
            QWheelEvent wheel(at,window->mapToGlobal(at.toPoint()),QPoint(),QPoint(0,target->mapToScene(QPointF()).y()<bounds.top()?120:-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
            QCoreApplication::sendEvent(window,&wheel);QTest::qWait(80);
        }
        if(!bounds.contains(QRectF(target->mapToScene(QPointF()),target->size()))){
            qWarning()<<"Combine click clipped"<<name<<QRectF(target->mapToScene(QPointF()),target->size())<<bounds;return false;
        }
        if(p->property("moving").isValid()&&!QTest::qWaitFor([&]{return !p->property("moving").toBool();},2000))return false;
    }
    const auto point=target->mapToScene(QPointF(target->width()/2,target->height()/2));
    if(!QRectF(QPointF(),window->size()).contains(point))return false;
    window->requestActivate();QTest::qWait(40);
    QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,point.toPoint());QTest::qWait(60);return true;
}
bool choose(QQuickWindow *window,const char *name,int index){
    if(!click(window,name))return false;
    QTest::keyClick(window,Qt::Key_Home);
    for(int i=0;i<index;++i)QTest::keyClick(window,Qt::Key_Down);
    QTest::keyClick(window,Qt::Key_Return);return true;
}
QList<QByteArray> hashes(const QStringList &paths){
    QList<QByteArray> result;
    for(const auto &path:paths){QFile file(path);if(!file.open(QIODevice::ReadOnly))return {};QCryptographicHash hash(QCryptographicHash::Sha256);if(!hash.addData(&file))return {};result.append(hash.result());}
    return result;
}
bool equal(const QImage &a,const QImage &b){return !a.isNull()&&!b.isNull()&&a.convertToFormat(QImage::Format_RGBA8888)==b.convertToFormat(QImage::Format_RGBA8888);}
ImageCombineSession *session(ImageCombineWindow *window){return window?qobject_cast<ImageCombineSession *>(window->sessionObject()):nullptr;}
bool settled(ImageCombineWindow *window){auto model=session(window);return model&&QTest::qWaitFor([&]{return !model->busy();},10000);}
QString idAt(ImageCombineSession *model,int index){return model->items().value(index).toMap().value(QStringLiteral("id")).toString();}
bool selectFilesAndAccept(QFileDialog *dialog,const QStringList &paths){
    if(!dialog||paths.isEmpty())return false;
    const QString directory=QFileInfo(paths.first()).absolutePath();
    QStringList quoted,expected;
    for(const auto &path:paths){
        const QFileInfo info(path);
        if(info.absolutePath()!=directory)return false;
        quoted.append(QStringLiteral("\"%1\"").arg(info.fileName()));
        expected.append(info.absoluteFilePath());
    }
    expected.sort();
    // Load the common directory first. Typing absolute paths while the file
    // model changes roots lets completion overwrite the in-flight key sequence.
    dialog->setDirectory(directory);
    auto model=dialog->findChild<QFileSystemModel *>();
    if(!model||!QTest::qWaitFor([&]{
        const auto root=model->index(directory);
        if(!root.isValid()||model->rowCount(root)<paths.size())return false;
        for(const auto &path:paths)if(!model->index(path).isValid())return false;
        return dialog->directory().absolutePath()==directory;
    },3000)){qWarning()<<"Combine picker directory did not load"<<directory;return false;}
    auto edit=dialog->findChild<QLineEdit *>(QStringLiteral("fileNameEdit"));
    if(!edit)return false;
    QTest::qWait(200);
    for(int attempt=0;attempt<3;++attempt){
        dialog->activateWindow();edit->setFocus();
        QTest::keyClick(edit,Qt::Key_A,Qt::ControlModifier);
        QTest::keyClicks(edit,quoted.join(u' '));QTest::qWait(150);
        auto selected=dialog->selectedFiles();selected.sort();
        if(edit->text()==quoted.join(u' ')&&selected==expected)
            return QMetaObject::invokeMethod(dialog,"accept");
        qWarning()<<"Combine picker input unsettled"<<attempt<<edit->text()<<selected<<"expected"<<expected;
    }
    return false;
}
bool reopen(ViewerWindow *owner,const QStringList &paths,bool keepAll,bool horizontal){
    owner->show();owner->requestActivate();
    if(!QTest::qWaitFor([&]{return QGuiApplication::focusWindow()==owner;},1000)) {
        qWarning()<<"Combine reopen owner focus"<<QGuiApplication::focusWindow()<<owner;
    }
    owner->combineImages(keepAll,horizontal);QPointer<QFileDialog> dialog;
    if(!QTest::qWaitFor([&]{for(auto widget:QApplication::topLevelWidgets())if(widget->objectName()==QStringLiteral("nkshotCombineDialog"))dialog=qobject_cast<QFileDialog *>(widget);return dialog&&dialog->isVisible();},1500)) {
        qWarning()<<"Combine reopen picker absent"<<"busy"<<owner->combiningImages()<<"review"<<review()<<"dialog"<<dialog.data();return false;
    }
    if(!selectFilesAndAccept(dialog,paths)){qWarning()<<"Combine reopen verified selection failed";return false;}
    const bool opened=QTest::qWaitFor([&]{return dialog.isNull()&&review();},3000);
    if(!opened)qWarning()<<"Combine reopen did not settle"<<"dialog"<<dialog.data()<<"visible"<<(dialog&&dialog->isVisible())
        <<"selection"<<(dialog?dialog->selectedFiles():QStringList{})<<"review"<<review()<<"busy"<<owner->combiningImages();
    return opened;
}
bool closeReview(QPointer<ImageCombineWindow> window){return click(window,"nkshotCombineReviewCancel")&&QTest::qWaitFor([&]{return window.isNull();},1500);}
}
bool ImageCombineUiChecks::run(QApplication &application,ViewerWindow *owner,const QStringList &paths,const QImage &expected,bool keepAll,bool horizontal,const QString &evidence,QString *failure){
    const auto fail=[&](const QString &text){if(failure)*failure=text;if(review())review()->grabWindow().save(evidence+QStringLiteral(".review-failed.png"));return false;};
    auto document=SpectacleCore::instance()->annotationDocument();const auto before=document->renderToImage();const auto base=document->baseImage();const auto depth=document->undoStackDepth();const auto inputHashes=hashes(paths);const auto clipboard=application.clipboard()->text();
    const auto unchanged=[&]{return document->renderToImage()==before&&document->baseImage()==base&&document->undoStackDepth()==depth&&hashes(paths)==inputHashes&&application.clipboard()->text()==clipboard;};
    QPointer<ImageCombineWindow> window=review();auto model=session(window);
    if(!window||!model||inputHashes.size()!=paths.size()||!settled(window)||!model->ready()||!equal(model->snapshot(),expected))return fail(QStringLiteral("Numerically sorted review preview differs from original exact fixture"));
    window->resize(qEnvironmentVariableIsSet("NKSHOT_TEST_COMPACT")?QSize(420,620):QSize(900,650));QTest::qWait(120);
    owner->combineImages(keepAll,horizontal);
    if(review()!=window)return fail(QStringLiteral("Existing review was duplicated"));
    const auto originalIds=QStringList{idAt(model,0),idAt(model,1)};
    const auto originalPreview=model->previewUrl();
    if(!click(window,"nkshotCombineReviewItem_0")||!click(window,"nkshotCombineReviewDown")||!settled(window)||idAt(model,1)!=originalIds[0])return fail(QStringLiteral("Actual Down did not move selected source"));
    if(window->rootObject()->property("selectedId").toString()!=originalIds[0]||model->previewUrl()==originalPreview)return fail(QStringLiteral("Reorder lost selected identity or retained stale preview"));
    if(!click(window,"nkshotCombineReviewUp")||!settled(window)||idAt(model,0)!=originalIds[0]||!equal(model->snapshot(),expected))return fail(QStringLiteral("Actual Up did not restore exact combination"));
    // Focus the stable ListView, not a model-reset delegate. Consecutive keys
    // must keep operating after asynchronous preview/model updates.
    auto list=find(window->contentItem(),QStringLiteral("nkshotCombineReviewFiles"));
    if(!click(window,"nkshotCombineReviewItem_0")||!list||!list->hasActiveFocus())return fail(QStringLiteral("List click did not establish keyboard focus"));
    QTest::keyClick(window,Qt::Key_Down,Qt::AltModifier);
    if(!settled(window)||idAt(model,1)!=originalIds[0]||!list->hasActiveFocus())return fail(QStringLiteral("Alt Down lost selection or keyboard focus"));
    QTest::keyClick(window,Qt::Key_Up,Qt::AltModifier);
    if(!settled(window)||idAt(model,0)!=originalIds[0]||!list->hasActiveFocus()||!equal(model->snapshot(),expected))return fail(QStringLiteral("Consecutive Alt Up failed without refocusing"));
    if(!choose(window,"nkshotCombineReviewMethod",0)||!settled(window)||!model->keepAll()||!model->ready())return fail(QStringLiteral("Keep all pixels choice failed"));
    const auto stacked=model->snapshot();
    if(!keepAll&&equal(stacked,expected))return fail(QStringLiteral("Method change retained matched-overlap preview"));
    if(!choose(window,"nkshotCombineReviewDirection",horizontal?0:1)||!settled(window)||model->direction()!=(horizontal?QStringLiteral("vertical"):QStringLiteral("horizontal")))return fail(QStringLiteral("Direction choice did not reach session"));
    if(model->ready()&&equal(model->snapshot(),stacked))return fail(QStringLiteral("Direction retained stale preview"));
    if(!choose(window,"nkshotCombineReviewDirection",horizontal?1:0)||!settled(window)||!choose(window,"nkshotCombineReviewMethod",keepAll?0:1)||!settled(window)||!equal(model->snapshot(),expected)||!unchanged())return fail(QStringLiteral("Restored options lost exact preview or source isolation"));
    if(!window->grabWindow().save(evidence+QStringLiteral(".review.png")))return fail(QStringLiteral("Review evidence failed"));
    if(qEnvironmentVariableIsSet("NKSHOT_TEST_COMPACT")){
        auto body=find(window->contentItem(),QStringLiteral("nkshotCombineReviewScroll"));
        auto preview=find(window->contentItem(),QStringLiteral("nkshotCombineReviewPreview"));
        auto primary=find(window->contentItem(),QStringLiteral("nkshotCombineReviewCreate"));
        if(!body||!preview||!primary)return fail(QStringLiteral("Compact preview or fixed Create missing"));
        const auto contained=[&]{return QRectF(body->mapToScene(QPointF()),body->size()).contains(QRectF(preview->mapToScene(QPointF()),preview->size()));};
        for(int attempt=0;attempt<20&&!contained();++attempt){
            const auto at=body->mapToScene(QPointF(body->width()-16,body->height()/2));
            const int delta=preview->mapToScene(QPointF()).y()<body->mapToScene(QPointF()).y()?120:-120;
            QWheelEvent wheel(at,window->mapToGlobal(at.toPoint()),QPoint(),QPoint(0,delta),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
            QCoreApplication::sendEvent(window,&wheel);QTest::qWait(100);
        }
        if(!contained()||!preview->isVisible()||!primary->isVisible()||!primary->isEnabled()
            ||!QRectF(QPointF(),window->size()).contains(QRectF(primary->mapToScene(QPointF()),primary->size()))
            ||!window->grabWindow().save(evidence+QStringLiteral(".review-preview.png")))
            return fail(QStringLiteral("Compact preview not fully reachable with fixed Create visible"));
    }
    if(!closeReview(window)||!unchanged())return fail(QStringLiteral("Review cancel changed original/source"));
    if(!reopen(owner,paths,keepAll,horizontal))return fail(QStringLiteral("Review did not reopen after cancel"));
    window=review();model=session(window);
    window->resize(qEnvironmentVariableIsSet("NKSHOT_TEST_COMPACT")?QSize(420,620):QSize(900,650));
    QTest::qWait(150);
    if(!settled(window)||!click(window,"nkshotCombineReviewItem_1"))return fail(QStringLiteral("Reopened review second row not clickable"));
    if(!QTest::qWaitFor([&]{return window->rootObject()->property("selectedIndex").toInt()==1;},500))
        return fail(QStringLiteral("Actual second-row click did not select it"));
    if(!click(window,"nkshotCombineReviewRemove")||!settled(window))return fail(QStringLiteral("Selected row Remove unavailable"));
    auto create=find(window->contentItem(),QStringLiteral("nkshotCombineReviewCreate"));
    if(model->items().size()!=1||model->ready()||!create||create->isEnabled()||window->createCopy()!=nullptr||!unchanged())return fail(QStringLiteral("Single-image remainder allowed Create or modified source"));
    if(!window->grabWindow().save(evidence+QStringLiteral(".review-one-image.png")))return fail(QStringLiteral("One-image review evidence failed"));
    const auto remainingItems=model->items();
    const auto remainingPreview=model->previewUrl();
    const auto remainingError=model->error();
    const auto remainingDirection=model->direction();
    const bool remainingMethod=model->keepAll();
    const auto openReplacement=[&]() -> QPointer<QFileDialog> {
        if(!click(window,"nkshotCombineReviewChoose"))return {};
        QPointer<QFileDialog> picker;
        if(!QTest::qWaitFor([&]{
            for(auto widget:QApplication::topLevelWidgets())
                if(widget->objectName()==QStringLiteral("nkshotCombineReviewFileDialog"))picker=qobject_cast<QFileDialog *>(widget);
            return picker&&picker->isVisible();
        },1500))return {};
        return picker;
    };
    auto picker=openReplacement();
    if(!picker||!window->choosingImages()||window->createCopy())return fail(QStringLiteral("Replacement picker failed to lock review creation"));
    QTest::keyClick(picker,Qt::Key_Escape);
    if(!QTest::qWaitFor([&]{return picker.isNull()&&!window->choosingImages();},1500)
        ||model->items()!=remainingItems||model->previewUrl()!=remainingPreview||model->error()!=remainingError
        ||model->direction()!=remainingDirection||model->keepAll()!=remainingMethod||!unchanged())
        return fail(QStringLiteral("Canceling replacement changed the existing one-image review"));
    picker=openReplacement();
    if(!picker)return fail(QStringLiteral("Replacement picker did not reopen"));
    if(!selectFilesAndAccept(picker,paths))return fail(QStringLiteral("Replacement picker could not select the exact requested files"));
    if(!QTest::qWaitFor([&]{return picker.isNull()&&!window->choosingImages();},1500)
        ||review()!=window||session(window)!=model||!settled(window)||model->items().size()!=2
        ||model->direction()!=remainingDirection||model->keepAll()!=remainingMethod||!equal(model->snapshot(),expected)||!unchanged())
        return fail(QStringLiteral("Accepted replacement did not recover exact combination in the same review window"));
    if(!settled(window)||!equal(model->snapshot(),expected)||!click(window,"nkshotCombineReviewCreate"))return fail(QStringLiteral("Actual reviewed Create failed"));
    if(!QTest::qWaitFor([&]{return window.isNull();},1500)||!unchanged())return fail(QStringLiteral("Create failed to retire review or altered source"));
    QPointer<PinnedImageWindow> pin;
    for(auto candidate:QGuiApplication::allWindows())if(candidate->objectName()==QStringLiteral("nkshotCombinedImagePin"))pin=qobject_cast<PinnedImageWindow *>(candidate);
    if(!pin||!equal(pin->snapshot(),expected))return fail(QStringLiteral("Reviewed Create did not produce exact independent pin"));
    qInfo()<<"114 real review reorder, continuous keyboard focus, options, remove, cancel, replacement-picker recovery, exact Create and source hashes passed";return true;
}
bool ImageCombineUiChecks::checkError(ViewerWindow *,const QStringList &paths,const QString &expectedError,const QString &evidence,QString *failure){
    const auto fail=[&](const QString &text){if(failure)*failure=text;return false;};
    auto document=SpectacleCore::instance()->annotationDocument();const auto image=document->renderToImage();const auto depth=document->undoStackDepth();const auto inputHashes=hashes(paths);
    QPointer<ImageCombineWindow> window=review();auto model=session(window);
    if(!window||!model||!settled(window)||model->ready()||!model->error().contains(expectedError,Qt::CaseInsensitive))return fail(QStringLiteral("Expected review error '%1', got '%2'").arg(expectedError,model?model->error():QStringLiteral("no review")));
    auto create=find(window->contentItem(),QStringLiteral("nkshotCombineReviewCreate"));
    if(!create||create->isEnabled()||window->createCopy())return fail(QStringLiteral("Error review allowed creation"));
    if(!window->grabWindow().save(evidence+QStringLiteral(".review-error.png")))return fail(QStringLiteral("Error review capture failed"));
    if(model->items().size()>=2){
        // A bad overlap can be repaired in place with explicit full-content stack.
        if(!choose(window,"nkshotCombineReviewMethod",0)||!settled(window))return fail(QStringLiteral("Error review did not allow method repair"));
        if(expectedError.contains(QStringLiteral("overlap"),Qt::CaseInsensitive)&&!model->ready())return fail(QStringLiteral("Keep all pixels did not repair failed overlap"));
        if(expectedError.contains(QStringLiteral("edges"),Qt::CaseInsensitive)){
            if(!choose(window,"nkshotCombineReviewDirection",model->direction()==QStringLiteral("horizontal")?0:1)||!settled(window)||!model->ready())return fail(QStringLiteral("Direction did not repair incompatible cross-edge widths"));
        }
    }
    if(!closeReview(window)||document->renderToImage()!=image||document->undoStackDepth()!=depth||hashes(paths)!=inputHashes)return fail(QStringLiteral("Error review cancellation changed source"));
    for(auto candidate:QGuiApplication::allWindows())if(candidate->objectName()==QStringLiteral("nkshotCombinedImagePin"))return fail(QStringLiteral("Error review produced an unrequested pin"));
    return true;
}
