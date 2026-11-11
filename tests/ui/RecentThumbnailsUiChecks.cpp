// SPDX-License-Identifier: GPL-3.0-only
#include "RecentThumbnailsUiChecks.h"
#include "Gui/ViewerWindow.h"
#include "Gui/RecentThumbnails.h"
#include "RecentMediaModel.h"
#include "SpectacleCore.h"
#include <QApplication>
#include <QFile>
#include <QPointer>
#include <QQuickItem>
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QTemporaryDir>
#include <QPainter>
#include <QtTest/QTest>
namespace {
QQuickItem *find(QQuickItem *root,const QString &name){if(!root)return nullptr;if(root->objectName()==name)return root;for(auto child:root->childItems())if(auto found=find(child,name))return found;return nullptr;}
QByteArray bytes(const QString &path){QFile f(path);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray();}
struct Restore {
    NkShot::RecentMediaModel *model;
    QList<QUrl> urls;
    ~Restore(){model->clear();for(auto it=urls.crbegin();it!=urls.crend();++it)model->remember(*it);}
};
}
bool RecentThumbnailsUiChecks::run(QApplication &application,ViewerWindow *raw,const QString &prefix,QString *failure){
    Q_UNUSED(application);
    QPointer<ViewerWindow> viewer=raw;
    auto fail=[&](const QString &message){if(failure)*failure=message;if(viewer)viewer->grabWindow().save(prefix+QStringLiteral(".recent-thumbnails-failed.png"));return false;};
    if(!viewer)return fail(QStringLiteral("Recent thumbnail viewer missing"));
    auto core=SpectacleCore::instance();auto document=core->annotationDocument();
    const auto image=document->renderToImage().copy();const auto depth=document->undoStackDepth();const auto video=core->currentVideo();const bool videoMode=core->videoMode();
    auto unchanged=[&]{return viewer&&document->renderToImage()==image&&document->undoStackDepth()==depth&&core->currentVideo()==video&&core->videoMode()==videoMode;};
    auto model=NkShot::RecentMediaModel::instance();Restore restore{model,{}};
    for(int i=0;i<model->count();++i)restore.urls.append(model->data(model->index(i),NkShot::RecentMediaModel::UrlRole).toUrl());
    if(!model->clear())return fail(QStringLiteral("Recent fixture could not clear metadata"));
    QTemporaryDir dir;const auto png=dir.filePath(QStringLiteral("Design board.png")),jpg=dir.filePath(QStringLiteral("Photo study.jpg"));
    const auto movie=dir.filePath(QStringLiteral("Recording.mp4")),missing=dir.filePath(QStringLiteral("Moved image.png"));
    QImage fixture(400,240,QImage::Format_RGB32);fixture.fill(QColor(QStringLiteral("#dbeafe")));
    {QPainter painter(&fixture);painter.fillRect(QRect(30,30,150,180),QColor(QStringLiteral("#007aff")));painter.fillRect(QRect(210,60,160,100),QColor(QStringLiteral("#ff9500")));}
    if(!fixture.save(png)||!fixture.save(jpg)||!fixture.save(missing))return fail(QStringLiteral("Recent thumbnail fixtures failed"));
    {QFile f(movie);if(!f.open(QIODevice::WriteOnly)||f.write("placeholder-not-decoded")<0)return fail(QStringLiteral("Video metadata fixture failed"));}
    for(const auto &path:{missing,movie,jpg,png})if(!model->remember(QUrl::fromLocalFile(path)))return fail(QStringLiteral("Recent fixture remember failed"));
    if(!QFile::remove(missing))return fail(QStringLiteral("Missing fixture preparation failed"));
    const auto pngBefore=bytes(png),jpgBefore=bytes(jpg),videoBefore=bytes(movie);
    auto cache=qobject_cast<RecentThumbnails *>(viewer->recentThumbnailsObject());if(!cache)return fail(QStringLiteral("Recent thumbnail cache missing"));
    auto item=[&](const QString &name){return viewer?find(viewer->contentItem(),name):nullptr;};
    auto click=[&](const QString &name){auto target=item(name);if(!target||!target->isVisible()||!target->isEnabled())return false;const auto point=target->mapToScene(QPointF(target->width()/2,target->height()/2)).toPoint();if(!QRect(QPoint(),viewer->size()).contains(point))return false;QTest::mouseClick(viewer,Qt::LeftButton,Qt::NoModifier,point);QTest::qWait(100);return true;};
    auto open=[&]{viewer->requestActivate();QTest::qWait(60);return click(QStringLiteral("nkshotViewerMore"))&&click(QStringLiteral("nkshotRecentMediaAction"))&&QTest::qWaitFor([&]{auto list=item(QStringLiteral("nkshotRecentFiles"));return list&&list->isVisible()&&cache->active();},1000);};
    auto ready=[&]{for(int i=0;i<2;++i){auto thumb=item(QStringLiteral("nkshotRecentThumbnail_%1").arg(i));if(!thumb||thumb->property("status").toInt()!=1||thumb->property("source").toUrl().scheme()!=QStringLiteral("image"))return false;}return true;};
    if(!open()||!QTest::qWaitFor(ready,10000)||!unchanged())return fail(QStringLiteral("Actual Recent menu did not decode image thumbnails"));
    auto list=item(QStringLiteral("nkshotRecentFiles"));if(!list||!list->hasActiveFocus())return fail(QStringLiteral("Recent list missing initial keyboard focus"));
    QTest::keyClick(viewer,Qt::Key_Down);if(list->property("currentIndex").toInt()!=1)return fail(QStringLiteral("Recent down-key navigation failed"));
    QTest::keyClick(viewer,Qt::Key_Up);if(list->property("currentIndex").toInt()!=0)return fail(QStringLiteral("Recent up-key navigation failed"));
    const auto oldUrl=cache->urlFor(QUrl::fromLocalFile(png));
    viewer->grabWindow().save(prefix+QStringLiteral(".recent-thumbnails.png"));
    list->setProperty("currentIndex",3);QMetaObject::invokeMethod(list,"positionViewAtEnd");QTest::qWait(120);
    for(int i=2;i<4;++i){auto thumb=item(QStringLiteral("nkshotRecentThumbnail_%1").arg(i));if(!thumb||!thumb->property("source").toUrl().isEmpty())return fail(QStringLiteral("Video/missing row unexpectedly requested thumbnail"));}
    auto missingButton=item(QStringLiteral("nkshotRecentOpen_3"));if(!missingButton||missingButton->isEnabled())return fail(QStringLiteral("Missing row remained openable"));
    QTest::keyClick(viewer,Qt::Key_Escape);QTest::qWait(150);
    if(cache->active()||!cache->urlFor(QUrl::fromLocalFile(png)).isEmpty()||!unchanged())return fail(QStringLiteral("Closing Recent retained live cache or changed main content"));
    auto provider=dynamic_cast<QQuickImageProvider *>(viewer->engine()->imageProvider(oldUrl.host()));
    QSize size;if(!provider||!provider->requestImage(oldUrl.path().mid(1),&size,QSize()).isNull())return fail(QStringLiteral("Closed thumbnail URL still resolved cached pixels"));
    if(!open()||!QTest::qWaitFor(ready,10000)||cache->urlFor(QUrl::fromLocalFile(png))==oldUrl)return fail(QStringLiteral("Recent reopen failed to refresh thumbnail generation"));
    if(!click(QStringLiteral("nkshotRecentRemove_0"))||model->count()!=3||bytes(png)!=pngBefore)return fail(QStringLiteral("Recent Remove changed original file"));
    if(!click(QStringLiteral("nkshotRecentClear"))||model->count()!=0||bytes(png)!=pngBefore||bytes(jpg)!=jpgBefore||bytes(movie)!=videoBefore||!unchanged())return fail(QStringLiteral("Recent Clear changed original files/main image"));
    if(!click(QStringLiteral("nkshotRecentClose")))return fail(QStringLiteral("Recent Close failed"));
    qInfo()<<"110 actual Recent thumbnail loading, placeholders, keyboard focus, cache expiry, reopen and metadata-only remove/clear passed";
    return true;
}
