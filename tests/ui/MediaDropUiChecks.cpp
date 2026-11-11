// SPDX-License-Identifier: GPL-3.0-only
#include "MediaDropUiChecks.h"
#include "Gui/ViewerWindow.h"
#include "SpectacleCore.h"
#include <KQuickImageEditor/AnnotationTool>
#include <QApplication>
#include <QQuickItem>
#include <qpa/qwindowsysteminterface.h>
#include <qpa/qplatformdrag.h>
#include <QMimeData>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QPainter>
#include <memory>
#include <vector>
#include <QtTest/QTest>
namespace {
QQuickItem *find(QQuickItem *item,const QString &name) {
    if(!item)return nullptr;
    if(item->objectName()==name)return item;
    for(auto child:item->childItems())if(auto result=find(child,name))return result;
    return nullptr;
}
QWidget *widget(const QString &name) {
    for(auto w:QApplication::topLevelWidgets())if(w->isVisible()&&w->objectName()==name)return w;
    return nullptr;
}
QByteArray bytes(const QString &path) {QFile f(path);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray();}
bool pixels(const QImage &a,const QImage &b) {
    if(a.size()!=b.size())return false;
    const auto aa=a.convertToFormat(QImage::Format_ARGB32_Premultiplied),bb=b.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    for(int y=0;y<aa.height();++y)for(int x=0;x<aa.width();++x)if(aa.pixel(x,y)!=bb.pixel(x,y))return false;
    return true;
}
}
bool MediaDropUiChecks::run(QApplication &application,ViewerWindow *initialViewer,const QString &prefix,QString *failure) {
    Q_UNUSED(application);
    // Committing Open replaces the whole ViewerWindow. Never retain its pointer
    // across accepted media changes; mirrors the production singleton owner.
    const auto viewer=[] { return ViewerWindow::instance(); };
    if(!initialViewer || initialViewer!=viewer()) { if(failure)*failure=QStringLiteral("Missing current viewer");return false; }
    const QSize fixtureWindowSize=initialViewer->size();
    auto fail=[&](const QString &text){if(failure)*failure=text;if(viewer())viewer()->grabWindow().save(prefix+QStringLiteral(".drop-failed.png"));return false;};
    if(!viewer())return fail(QStringLiteral("Missing viewer"));
    auto core=SpectacleCore::instance();auto document=core->annotationDocument();
    if(core->videoMode()||document->baseImage().isNull())return fail(QStringLiteral("Drop fixture requires current screenshot"));
    QTemporaryDir folder;if(!folder.isValid())return fail(QStringLiteral("Drop fixture directory unavailable"));
    QImage fixture=document->renderToImage().copy();fixture.setDevicePixelRatio(1);
    {QPainter painter(&fixture);painter.fillRect(QRect(4,4,12,12),QColor(QStringLiteral("#007aff")));}
    const auto png=folder.filePath(QStringLiteral("sharing-card.png")),jpeg=folder.filePath(QStringLiteral("sharing-card.jpg"));
    if(!fixture.save(png)||!fixture.save(jpeg))return fail(QStringLiteral("Drop image fixture save failed"));
    const auto pngBytes=bytes(png),jpegBytes=bytes(jpeg);
    document->tool()->setType(AnnotationTool::RectangleTool);document->tool()->setStrokeColor(QColor(QStringLiteral("#007aff")));
    document->tool()->setStrokeWidth(3);document->tool()->setFillColor(Qt::transparent);document->tool()->setShadow(false);
    document->beginItem({24,24});document->continueItem({90,70});document->finishItem();
    auto current=document->renderToImage().copy();int depth=document->undoStackDepth();
    auto unchanged=[&]{return !core->videoMode()&&pixels(document->renderToImage(),current)&&document->undoStackDepth()==depth;};
    auto overlay=[&]{auto item=find(viewer()->contentItem(),QStringLiteral("nkshotMediaDropOverlay"));return item&&item->isVisible();};
    auto point=[&]{auto area=find(viewer()->contentItem(),QStringLiteral("nkshotMediaDropTarget"));return area?area->mapToScene(QPointF(area->width()/2,area->height()/2)).toPoint():QPoint(-1,-1);};
    auto leave=[&]{QWindowSystemInterface::handleDrag(viewer(),nullptr,QPoint(),Qt::IgnoreAction,Qt::NoButton,Qt::NoModifier);QTest::qWait(60);};
    auto enter=[&](QMimeData &mime,Qt::DropActions actions){return QWindowSystemInterface::handleDrag(viewer(),&mime,point(),actions,Qt::LeftButton,Qt::NoModifier).isAccepted();};
    std::vector<std::unique_ptr<QMimeData>> droppedMime;
    auto drop=[&](const QList<QUrl> &urls){
        droppedMime.push_back(std::make_unique<QMimeData>());
        auto &mime=*droppedMime.back();mime.setUrls(urls);if(!enter(mime,Qt::CopyAction|Qt::MoveAction))return false;
        const auto move=QWindowSystemInterface::handleDrag(viewer(),&mime,point(),Qt::CopyAction|Qt::MoveAction,Qt::LeftButton,Qt::ShiftModifier);
        if(!move.isAccepted()){leave();return false;}
        const auto event=QWindowSystemInterface::handleDrop(viewer(),&mime,point(),Qt::CopyAction|Qt::MoveAction,Qt::LeftButton,Qt::ShiftModifier);
        return event.isAccepted()&&event.acceptedAction()==Qt::CopyAction&&!overlay();
    };
    auto confirm=[&](QMessageBox::StandardButton answer){
        if(!QTest::qWaitFor([&]{return widget(QStringLiteral("nkshotReplaceMediaDialog"));},15000))return false;
        auto box=qobject_cast<QMessageBox *>(widget(QStringLiteral("nkshotReplaceMediaDialog")));auto button=box?box->button(answer):nullptr;
        if(!button)return false;
        box->grab().save(prefix+QStringLiteral(".replace.png"));
        QPointer<ViewerWindow> previousViewer=viewer();
        QTest::mouseClick(button,Qt::LeftButton);
        if(!QTest::qWaitFor([&]{return viewer()&&!widget(QStringLiteral("nkshotReplaceMediaDialog"))&&!viewer()->property("openingMedia").toBool();},2000))return false;
        if(answer==QMessageBox::Discard && !QTest::qWaitFor([&]{return previousViewer.isNull()&&viewer();},1500))return false;
        viewer()->resize(fixtureWindowSize);viewer()->requestActivate();QTest::qWait(100);
        return true;
    };
    viewer()->requestActivate();QTest::qWait(150);
    QMimeData hover;hover.setUrls({QUrl::fromLocalFile(png)});
    if(!enter(hover,Qt::CopyAction|Qt::MoveAction)||!QTest::qWaitFor(overlay,500))return fail(QStringLiteral("Real drag enter did not show overlay"));
    QWindowSystemInterface::handleDrag(viewer(),&hover,point()+QPoint(6,4),Qt::CopyAction|Qt::MoveAction,Qt::LeftButton,Qt::NoModifier);
    QTest::qWait(100);viewer()->grabWindow().save(prefix+QStringLiteral(".drop-hover.png"));leave();
    if(overlay()||!unchanged())return fail(QStringLiteral("Drag leave changed content or retained overlay"));
    for(int kind=0;kind<4;++kind){QMimeData invalid;
        if(kind==0)invalid.setUrls({QUrl(QStringLiteral("https://example.com/image.png"))});
        else if(kind==1)invalid.setUrls({QUrl::fromLocalFile(png),QUrl::fromLocalFile(jpeg)});
        else if(kind==2)invalid.setText(QStringLiteral("not a file"));
        else invalid.setUrls({QUrl::fromLocalFile(png)});
        const bool accepted=enter(invalid,kind==3?Qt::MoveAction:Qt::CopyAction);
        leave();if(accepted||overlay()||!unchanged())return fail(QStringLiteral("Invalid or Move-only drag accepted"));
    }
    if(!enter(hover,Qt::CopyAction)||!overlay())return fail(QStringLiteral("Busy-transition hover unavailable"));
    viewer()->openMedia();
    if(!QTest::qWaitFor([&]{return widget(QStringLiteral("nkshotOpenMediaDialog"));},1000))return fail(QStringLiteral("Open chooser busy fixture failed"));
    if(overlay())return fail(QStringLiteral("Opening chooser retained drop overlay"));
    const bool busyAccepted=enter(hover,Qt::CopyAction);leave();
    auto chooser=qobject_cast<QFileDialog *>(widget(QStringLiteral("nkshotOpenMediaDialog")));auto box=chooser?chooser->findChild<QDialogButtonBox *>():nullptr;auto cancel=box?box->button(QDialogButtonBox::Cancel):nullptr;
    if(!cancel)return fail(QStringLiteral("Open chooser Cancel missing"));
    QTest::mouseClick(cancel,Qt::LeftButton);
    if(busyAccepted||!QTest::qWaitFor([&]{return viewer()->canOpenMedia();},1500)||!unchanged())return fail(QStringLiteral("Busy chooser admitted drop"));
    if(!drop({QUrl::fromLocalFile(png)})||!confirm(QMessageBox::Cancel)||!unchanged())return fail(QStringLiteral("Drop replacement Cancel lost edits"));
    if(!drop({QUrl::fromLocalFile(png)})||!confirm(QMessageBox::Discard)||!QTest::qWaitFor([&]{return !core->videoMode()&&pixels(document->renderToImage(),fixture);},2000))return fail(QStringLiteral("Repeat PNG drop did not open exact fixture"));
    viewer()->grabWindow().save(prefix+QStringLiteral(".opened.png"));current=document->renderToImage().copy();depth=document->undoStackDepth();
    const auto corrupt=folder.filePath(QStringLiteral("corrupt.png"));{QFile file(corrupt);if(!file.open(QIODevice::WriteOnly)||file.write("not-an-image")<0)return fail(QStringLiteral("Corrupt fixture creation failed"));}
    for(const auto &path:{corrupt,folder.filePath(QStringLiteral("missing.png"))}) {
        qInfo()<<"108 failure fixture drop"<<path;
        if(!drop({QUrl::fromLocalFile(path)}))return fail(QStringLiteral("Lexically valid failure fixture not admitted"));
        qInfo()<<"108 failure fixture admitted"<<path;
        if(!QTest::qWaitFor([&]{return !viewer()->property("openingMedia").toBool()&&viewer()->canOpenMedia();},15000)||!unchanged()||widget(QStringLiteral("nkshotReplaceMediaDialog")))return fail(QStringLiteral("Decode failure replaced current content"));
    }
    if(bytes(corrupt)!="not-an-image"||QFileInfo::exists(folder.filePath(QStringLiteral("missing.png"))))return fail(QStringLiteral("Failure fixture changed on disk"));
    qInfo()<<"108 failure fixtures passed; entering video checks";
    const auto movie=qEnvironmentVariable("NKSHOT_TEST_LOCAL_MEDIA_VIDEO",QStringLiteral("/logs/playback062.mp4"));const auto movieBytes=bytes(movie);
    if(movieBytes.isEmpty())return fail(QStringLiteral("Movie fixture unavailable: ")+movie);
    if(!drop({QUrl::fromLocalFile(movie)})||!confirm(QMessageBox::Cancel)||!unchanged())return fail(QStringLiteral("Movie drop Cancel changed image"));
    if(!drop({QUrl::fromLocalFile(movie)})||!confirm(QMessageBox::Discard)||!QTest::qWaitFor([&]{return core->videoMode()&&core->currentVideo()==QUrl::fromLocalFile(movie);},15000))return fail(QStringLiteral("Movie drop did not enter playback"));
    if(bytes(movie)!=movieBytes||bytes(png)!=pngBytes||bytes(jpeg)!=jpegBytes)return fail(QStringLiteral("Drop mutated source files"));
    qInfo()<<"108 actual Qt drag/drop admission, Copy action, hover/leave, replacement, failures and MP4 playback passed";
    return true;
}
