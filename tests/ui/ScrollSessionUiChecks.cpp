// SPDX-License-Identifier: GPL-3.0-only
// Controlled synthetic frames exercise real controller/session/window UI.
// No KWin API, compositor exclusion, real scrolling, or desktop pixels are claimed.
#include "ScrollSessionUiChecks.h"
#include "Gui/ScrollCaptureWindow.h"
#include "Gui/PinnedImageWindow.h"
#include "Gui/ViewerWindow.h"
#include "SpectacleCore.h"
#include <KQuickImageEditor/AnnotationDocument>
#include <QApplication>
#include <QClipboard>
#include <QPainter>
#include <QQuickItem>
#include <QSignalSpy>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QFile>
#include <QWheelEvent>
#include <QtTest/QTest>

bool ScrollSessionUiChecks::run(QApplication &app,const QString &path,QString *failure)
{
    using Controller=NkShot::Imaging::ScrollCaptureController;
    using Layout=NkShot::Imaging::ScrollOutputLayout;
    auto fail=[&](const QString &message){if(failure)*failure=message;return false;};
    auto viewer=ViewerWindow::instance();auto core=SpectacleCore::instance();
    if(!viewer||!core)return fail(QStringLiteral("Original viewer unavailable"));
    const QImage original=core->annotationDocument()->baseImage();
    const auto depth=core->annotationDocument()->undoStackDepth();
    const auto currentVideo=core->currentVideo();const bool videoMode=core->videoMode();
    const QString clipboard=app.clipboard()->text();
    const QString sentinel=QStringLiteral("scroll session has not copied");
    app.clipboard()->setText(sentinel);
    QPointer<ScrollCaptureWindow> window;
    QPointer<PinnedImageWindow> pin;
    const auto cleanup=qScopeGuard([&]{if(window)delete window;if(pin)delete pin;app.clipboard()->setText(clipboard);});
    const auto unchanged=[&]{return core->annotationDocument()->baseImage()==original
        &&core->annotationDocument()->undoStackDepth()==depth&&core->currentVideo()==currentVideo&&core->videoMode()==videoMode;};
    const auto visual=[](QQuickItem *root,const QString &name)->QQuickItem* {
        auto visit=[&](auto &&self,QQuickItem *node)->QQuickItem*{
            if(node->objectName()==name)return node;
            for(auto child:node->childItems())if(auto found=self(self,child))return found;
            return nullptr;
        };return root?visit(visit,root):nullptr;
    };
    auto click=[&](QQuickView *view,const char *name){
        auto item=visual(view->contentItem(),QString::fromLatin1(name));
        if(!item||!item->isVisible()||!item->isEnabled())return false;
        const auto point=item->mapToScene(QPointF(item->width()/2,item->height()/2));
        if(point.x()<0||point.y()<0||point.x()>view->width()||point.y()>view->height())return false;
        QTest::mouseClick(view,Qt::LeftButton,Qt::NoModifier,point.toPoint());return true;
    };
    const bool longPreview=qEnvironmentVariableIsSet("NKSHOT_TEST_SCROLL_SESSION_LONG");
    const int frameHeight=longPreview?2200:420, secondOffset=longPreview?1400:180, thirdOffset=longPreview?2800:410;
    const Layout layout{QStringLiteral("synthetic-page"),QRectF(0,0,640,frameHeight),QSize(640,frameHeight),1};
    QObject backendLifetime;
    QImage pending;
    int requests=0;
    window=new ScrollCaptureWindow(viewer->engine());
    // Let the constructor's real, unavailable compositor query finish before
    // the compile-time friend replaces only the controller capture dependency.
    QTest::qWait(1100);
    delete window->m_controller;
    window->m_controller=new Controller(&backendLifetime,[&](quint64 ticket,QRect rect){
        ++requests;
        if(rect!=QRect(0,0,640,frameHeight)||pending.isNull())return false;
        const QImage frame=pending;
        QMetaObject::invokeMethod(window,[&,ticket,frame]{if(window)window->m_controller->frameCaptured(ticket,layout,frame);},Qt::QueuedConnection);
        return true;
    },[]{},window);
    window->bindController();
    QObject::connect(window->m_controller,&Controller::hideWindowsRequested,window,[&](quint64 ticket){
        QMetaObject::invokeMethod(window,[&,ticket]{if(window)window->m_controller->confirmHidden(ticket,layout);},Qt::QueuedConnection);
    });
    window->m_phase=2;window->m_available=true;
    if(!window->m_controller->begin(layout,layout.logicalGeometry))return fail(QStringLiteral("Synthetic selected-region controller setup failed"));
    window->resize(qEnvironmentVariableIsSet("NKSHOT_TEST_SCROLL_SESSION_COMPACT")?420:760,600);
    window->show();Q_EMIT window->changed();
    if(!QTest::qWaitFor([&]{return window->rootObject()&&window->rootObject()->size()==QSizeF(window->size());},1500))
        return fail(QStringLiteral("Actual scroll session view not ready"));
    const auto deliver=[&](const QImage &frame){
        pending=frame;const int before=requests;
        if(!click(window,"nkshotScrollPrimary"))return false;
        return QTest::qWaitFor([&]{return requests==before+1&&!window->busy();},3000);
    };
    // A separate synthetic session verifies ambiguous-overlap UI recovery.
    QImage blank(640,frameHeight,QImage::Format_RGBA8888);blank.fill(Qt::white);
    if(!deliver(blank)||window->frameCount()!=1)return fail(QStringLiteral("Ambiguous fixture first frame failed"));
    QImage ambiguous=blank;
    {QPainter painter(&ambiguous);painter.fillRect(0,260,640,160,Qt::black);}
    if(!deliver(ambiguous)||window->frameCount()!=1||window->m_controller->preview()!=blank||!window->property("retryFrame").toBool())
        return fail(QStringLiteral("Ambiguous overlap did not preserve preview and enable retry"));
    window->m_controller->cancel();
    if(!window->m_controller->begin(layout,layout.logicalGeometry))return fail(QStringLiteral("New synthetic page session could not begin"));
    QImage page(640,longPreview?6000:1300,QImage::Format_RGBA8888);
    for(int y=0;y<page.height();++y)for(int x=0;x<page.width();++x) {
        quint32 n=quint32(x+1)*2654435761u ^ quint32(y+1)*2246822519u;
        n^=n>>13; n*=3266489917u;
        page.setPixelColor(x,y,QColor(225+n%25,225+(n>>8)%25,225+(n>>16)%25));
    }
    {QPainter painter(&page);QFont font(QStringLiteral("DejaVu Sans"));font.setPixelSize(21);painter.setFont(font);
        for(int row=0;row<(longPreview?85:18);++row){const int y=20+row*70;
            painter.fillRect(20,y,8,45,QColor::fromHsv((row*37)%360,180,170));
            painter.setPen(QColor(35,40,50));painter.drawText(QRect(42,y,570,32),QStringLiteral("Synthetic page · section %1").arg(row+1));
            painter.setPen(QColor(90,95,105));painter.drawText(QRect(42,y+28,570,28),QStringLiteral("Read, review, then continue down the page."));}}
    if(!page.save(path+QStringLiteral(".fixture.png")))return fail(QStringLiteral("Synthetic page provenance could not be saved"));
    const QImage first=page.copy(0,0,640,frameHeight);
    QImage second=page.copy(0,secondOffset,640,frameHeight);
    {QPainter painter(&second);painter.fillRect(510,70,12,10,QColor(220,90,90));}
    if(!deliver(first)||window->frameCount()!=1)return fail(QStringLiteral("Textured first frame not accepted"));
    if(!deliver(second)||window->frameCount()!=2)return fail(QStringLiteral("Variable-overlap frame with small dynamic island rejected"));
    const QImage afterTwo=window->m_controller->preview();
    if(afterTwo.size()!=QSize(640,frameHeight+secondOffset)||afterTwo.copy(0,0,640,frameHeight)!=first
        ||afterTwo.copy(0,frameHeight,640,secondOffset)!=page.copy(0,frameHeight,640,secondOffset))
        return fail(QStringLiteral("Accepted composite altered original pixels or appended wrong extent"));
    for(int rejected=0;rejected<2;++rejected){
        QImage candidate=second;if(rejected)candidate.fill(Qt::red);
        if(!deliver(candidate)||window->frameCount()!=2||window->m_controller->preview()!=afterTwo||!window->property("retryFrame").toBool())
            return fail(QStringLiteral("No-op/unrelated frame changed accumulated pixels or retry state"));
    }
    auto retryButton=visual(window->contentItem(),QStringLiteral("nkshotScrollPrimary"));
    if(!retryButton || !retryButton->property("text").toString().contains(QStringLiteral("Retry")))
        return fail(QStringLiteral("Rejected frame has no clear Retry action"));
    QSignalSpy retryFrame(window,&QQuickWindow::frameSwapped);window->update();
    if(!QTest::qWaitFor([&]{return !retryFrame.isEmpty();},1500)||!window->grabWindow().save(path+QStringLiteral(".retry.png")))
        return fail(QStringLiteral("Retained preview/retry capture failed"));
    if(!deliver(page.copy(0,thirdOffset,640,frameHeight))||window->frameCount()!=3)
        return fail(QStringLiteral("Retry after rejected frames did not accept next variable offset"));
    const QImage composite=window->m_controller->preview();
    if(composite.size()!=QSize(640,frameHeight+thirdOffset)||composite!=page.copy(0,0,640,frameHeight+thirdOffset)
        ||window->property("previewSize").toSize()!=composite.size()
        ||window->property("seamRows").toList().size()!=2||window->property("retryFrame").toBool())
        return fail(QStringLiteral("Final preview pixels/provider extent/seams/retry state incorrect"));
    if(!click(window,"nkshotScrollShowSeams")||window->rootObject()->property("showSeams").toBool()
        ||!click(window,"nkshotScrollShowSeams")||!window->rootObject()->property("showSeams").toBool())
        return fail(QStringLiteral("Actual seams toggle did not change visible state"));
    QSignalSpy fitted(window,&QQuickWindow::frameSwapped);window->update();
    if(!QTest::qWaitFor([&]{return !fitted.isEmpty();},1500)||!window->grabWindow().save(path+QStringLiteral(".preview.png")))
        return fail(QStringLiteral("Actual fitted seam preview screenshot failed"));
    if(!click(window,"nkshotScrollInspectPixels")||!window->rootObject()->property("inspectPixels").toBool())
        return fail(QStringLiteral("Actual 100-percent control unreachable"));
    auto viewport=visual(window->contentItem(),QStringLiteral("nkshotScrollPreviewViewport"));
    if(!viewport)return fail(QStringLiteral("Preview viewport missing"));
    const auto wheelPoint=viewport->mapToScene(QPointF(viewport->width()/2,viewport->height()/2));
    QWheelEvent wheel(wheelPoint,window->mapToGlobal(wheelPoint.toPoint()),QPoint(),QPoint(0,longPreview?-3600:-360),
        Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
    QCoreApplication::sendEvent(window,&wheel);
    if(!QTest::qWaitFor([&]{return viewport->property("contentY").toReal()>0;},1000))
        return fail(QStringLiteral("Actual wheel did not scroll 100-percent preview"));
    // Platforms can clamp one wheel event regardless of its large angle delta.
    // Exercise repeated real wheel events until a subsequent tile is visible.
    if(longPreview) for(int step=0;step<16 && viewport->property("contentY").toReal()<1024;++step) {
        QWheelEvent nextWheel(wheelPoint,window->mapToGlobal(wheelPoint.toPoint()),QPoint(),QPoint(0,-360),
            Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
        QCoreApplication::sendEvent(window,&nextWheel);QTest::qWait(60);
    }
    auto image=visual(window->contentItem(),QStringLiteral("nkshotScrollPreviewImage"));
    if(!QTest::qWaitFor([&]{return image&&image->isVisible()&&image->property("status").toInt()==1
        &&qRound(image->width())==composite.width()&&qRound(image->height())==composite.height()
        &&image->property("source").toUrl()==window->property("previewSource").toUrl();},1500))
        return fail(QStringLiteral("Provider image did not load at actual 100-percent pixel dimensions"));
    auto tile=visual(window->contentItem(),QStringLiteral("nkshotScrollPreviewTile"));
    if(!tile || tile->width()>1024 || tile->height()>1024 || !tile->property("source").toUrl().path().contains(QStringLiteral("/tile/")))
        return fail(QStringLiteral("100-percent preview did not use bounded visible tiles"));
    if(longPreview && viewport->property("contentY").toReal()<1024)
        return fail(QStringLiteral("Long-preview wheel did not cross a tile boundary"));
    QSignalSpy rendered(window,&QQuickWindow::frameSwapped);window->update();
    if(!QTest::qWaitFor([&]{return !rendered.isEmpty();},1500)||!window->grabWindow().save(path+QStringLiteral(".pixels.png")))
        return fail(QStringLiteral("Actual session preview screenshot failed"));
    if(app.clipboard()->text()!=sentinel||!unchanged())return fail(QStringLiteral("Collection changed original document/clipboard"));
    if(!click(window,"nkshotScrollFinish"))return fail(QStringLiteral("Actual Finish action unreachable"));
    if(!QTest::qWaitFor([&]{for(auto view:QGuiApplication::allWindows())
        if(auto candidate=qobject_cast<PinnedImageWindow *>(view))if(candidate->snapshot()==composite)pin=candidate;
        return pin&&pin->isVisible();},1500))return fail(QStringLiteral("Finish did not create actual separate composite pin"));
    if(!click(pin,"nkshotPinCopy")||app.clipboard()->image().convertToFormat(QImage::Format_RGBA8888)!=composite)return fail(QStringLiteral("Finished pin explicit Copy mismatch"));
    QTemporaryDir files;
    if(!files.isValid()||!click(pin,"nkshotPinSave"))return fail(QStringLiteral("Finished pin Save unavailable"));
    QPointer<QFileDialog> dialog;
    if(!QTest::qWaitFor([&]{for(auto widget:QApplication::topLevelWidgets())if(widget->objectName()==QStringLiteral("nkshotPinSaveDialog"))dialog=qobject_cast<QFileDialog *>(widget);return dialog&&dialog->isVisible();},1000))
        return fail(QStringLiteral("Pin Save dialog not shown"));
    auto filename=dialog->findChild<QLineEdit *>(QStringLiteral("fileNameEdit"));
    auto buttons=dialog->findChild<QDialogButtonBox *>();auto save=buttons?buttons->button(QDialogButtonBox::Save):nullptr;
    if(!filename||!save)return fail(QStringLiteral("Pin Save controls missing"));
    const auto output=files.filePath(QStringLiteral("actual composite.png"));filename->setFocus();
    QTest::keyClick(filename,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(filename,output);QTest::mouseClick(save,Qt::LeftButton);
    if(!QTest::qWaitFor([&]{return QFile::exists(output);},1000)||QImage(output).convertToFormat(QImage::Format_RGBA8888)!=composite||!unchanged())
        return fail(QStringLiteral("Saved finished PNG or original isolation mismatch"));
    qInfo()<<"Controlled-frame actual scroll session UI passed; native KWin capture/exclusion remains unverified";
    return true;
}
