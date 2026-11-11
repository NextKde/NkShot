// SPDX-License-Identifier: GPL-3.0-only
// Actual local-file dialogs and decoders, using generated nonpersonal fixtures.
#include "LocalMediaOpenUiChecks.h"
#include "SpectacleCore.h"
#include "Platforms/ImagePlatform.h"
#include "CaptureModeModel.h"
#include "Gui/ViewerWindow.h"
#include "Gui/InlineMessageModel.h"
#include <QEventLoop>
#include <QTimer>
#include <KQuickImageEditor/AnnotationDocument>
#include <KQuickImageEditor/AnnotationTool>
#include <QApplication>
#include <QClipboard>
#include <QColorSpace>
#include <cstring>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFile>
#include <QDateTime>
#include <QLineEdit>
#include <QLineF>
#include <QElapsedTimer>
#include <QMessageBox>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QQuickItem>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QTest>

namespace {
// Controlled terminal delivery only, never captures desktop pixels.
class DelayedOpenImagePlatform final : public ImagePlatform
{
public:
    int calls=0;
    GrabModes supportedGrabModes() const override {return AllScreens;}
    ShutterModes supportedShutterModes() const override {return Immediate;}
    void doGrab(ShutterMode,GrabMode,bool,bool,bool) override {++calls;}
};
class FinishedOpenVideoPlatform final : public VideoPlatform
{
public:
    FinishedOpenVideoPlatform() {setRecordingState(RecordingState::Finished);}
    RecordingModes supportedRecordingModes() const override {return NoRecordingModes;}
    Formats supportedFormats() const override {return NoFormat;}
    bool isRecordingAudio() const override {return false;}
    void startRecording(const QUrl &,RecordingMode,const QVariantMap &,bool) override {}
    void finishRecording() override {}
};
QQuickItem *visual(QQuickItem *root,const QString &name)
{
    if(!root)return nullptr;
    if(root->objectName()==name)return root;
    for(auto child:root->childItems())if(auto result=visual(child,name))return result;
    return nullptr;
}
QFileDialog *fileDialog(const QString &name={})
{
    for(auto widget:QApplication::topLevelWidgets())
        if(auto dialog=qobject_cast<QFileDialog *>(widget);dialog&&dialog->isVisible()
            &&(name.isEmpty()||dialog->objectName()==name))return dialog;
    return nullptr;
}
QMessageBox *replaceDialog()
{
    for(auto widget:QApplication::topLevelWidgets())
        if(auto dialog=qobject_cast<QMessageBox *>(widget);dialog&&dialog->isVisible()
            &&dialog->objectName()==QStringLiteral("nkshotReplaceMediaDialog"))return dialog;
    return nullptr;
}
bool samePixels(const QImage &actual,const QImage &expected)
{
    if(actual.isNull()||expected.isNull()||actual.size()!=expected.size())return false;
    const auto a=actual.convertToFormat(QImage::Format_RGBA8888);
    const auto b=expected.convertToFormat(QImage::Format_RGBA8888);
    for(int y=0;y<a.height();++y)
        if(std::memcmp(a.constScanLine(y),b.constScanLine(y),size_t(a.width())*4)!=0)return false;
    return true;
}
bool importedPixels(const QImage &actual,const QImage &expected)
{
    // AnnotationDocument normalizes painter storage and supplies a default
    // color profile. These metadata changes must not alter any decoded RGBA.
    return samePixels(actual,expected)&&(expected.colorSpace().isValid()||actual.colorSpace().isValid());
}
QByteArray bytes(const QString &path)
{
    QFile file(path);return file.open(QIODevice::ReadOnly)?file.readAll():QByteArray{};
}
bool choose(QFileDialog *dialog,const QString &path)
{
    auto edit=dialog?dialog->findChild<QLineEdit *>(QStringLiteral("fileNameEdit")):nullptr;
    auto box=dialog?dialog->findChild<QDialogButtonBox *>():nullptr;
    if(!edit||!box)return false;
    edit->setFocus();edit->selectAll();QTest::keyClicks(edit,path);
    QTest::qWait(30);
    for(auto button:box->buttons())if(box->buttonRole(button)==QDialogButtonBox::AcceptRole&&button->isEnabled()) {
        QTest::mouseClick(button,Qt::LeftButton);return true;
    }
    return false;
}
}

bool LocalMediaOpenUiChecks::run(QApplication &application,const QString &screenshot,QString *failure)
{
    const auto fail=[&](const QString &message){
        if(failure)*failure=message;
        if(auto window=ViewerWindow::instance())window->grabWindow().save(screenshot+QStringLiteral(".failure.png"));
        return false;
    };
    auto core=SpectacleCore::instance();
    if(!core||!ViewerWindow::instance())return fail(QStringLiteral("No real viewer"));
    const auto viewer=[] {return ViewerWindow::instance();};
    const auto document=core->annotationDocument();
    const auto clipboard=application.clipboard()->text();
    QTemporaryDir fixtures;
    if(!fixtures.isValid())return fail(QStringLiteral("Fixture directory unavailable"));
    QImage generated(640,400,QImage::Format_RGB32);
    for(int y=0;y<generated.height();++y)for(int x=0;x<generated.width();++x)
        generated.setPixelColor(x,y,QColor((x*7+y*3)%230,(x*3+y*11)%220,180));
    {QPainter painter(&generated);painter.fillRect(35,45,570,100,Qt::white);painter.setPen(Qt::black);
        auto font=painter.font();font.setPixelSize(28);painter.setFont(font);
        painter.drawText(QRect(45,50,550,90),Qt::AlignCenter,QStringLiteral("Local media • synthetic fixture"));}
    QStringList sources;QList<QByteArray> originals;
    for(const auto &extension:{QStringLiteral("png"),QStringLiteral("jpg"),QStringLiteral("webp")}) {
        auto path=fixtures.filePath(QStringLiteral("image ' source.")+extension);
        if(!generated.save(path))return fail(QStringLiteral("Real fixture encoder unavailable: ")+extension);
        sources<<path;originals<<bytes(path);
    }
    generated.save(screenshot+QStringLiteral(".fixture.png"));
    const auto sourcesIntact=[&] {
        for(int i=0;i<sources.size();++i)if(bytes(sources[i])!=originals[i])return false;
        return application.clipboard()->text()==clipboard;
    };
    const auto sizeViewer=[&] {viewer()->resize(qEnvironmentVariableIsSet("NKSHOT_TEST_COMPACT")?QSize(420,520):QSize(820,600));};
    const auto capture=[&](const QString &suffix) {
        sizeViewer();QSignalSpy frame(viewer(),&QQuickWindow::frameSwapped);viewer()->update();
        if(!QTest::qWaitFor([&]{return !frame.isEmpty();},1200))return false;
        if(!core->videoMode()) {
            auto canvas=visual(viewer()->contentItem(),QStringLiteral("nkshotAnnotationViewport"));
            if(!canvas)return false;
            QPointF previousA,previousB;bool previous=false,stable=false;
            QElapsedTimer wait;wait.start();
            while(wait.elapsed()<2500) {
                const auto a=canvas->mapToScene(QPointF(0,0));
                const auto b=canvas->mapToScene(QPointF(canvas->width(),canvas->height()));
                if(previous&&QLineF(a,b).length()>40&&QLineF(a,previousA).length()<1&&QLineF(b,previousB).length()<1) {
                    stable=true;break;
                }
                previousA=a;previousB=b;previous=true;QTest::qWait(50);
            }
            if(!stable)return false;
        }
        QTest::qWait(50);
        return viewer()->grabWindow().save(screenshot+suffix);
    };
    const auto open=[&] {
        viewer()->show();viewer()->requestActivate();
        if(!QTest::qWaitFor([&]{return QGuiApplication::focusWindow()==viewer();},1500)) {
            qWarning()<<"Open shortcut focus unavailable"<<QGuiApplication::focusWindow()<<viewer()
                <<"core allowed"<<core->canOpenMedia()<<"viewer allowed"<<viewer()->canOpenMedia()
                <<"opening"<<viewer()->property("openingMedia")<<"modal"<<QApplication::activeModalWidget();
            return false;
        }
        QTest::keyClick(viewer(),Qt::Key_O,Qt::ControlModifier);
        const bool opened=QTest::qWaitFor([&]{return fileDialog(QStringLiteral("nkshotOpenMediaDialog"));},1500);
        if(!opened)qWarning()<<"Activated Open shortcut did not create dialog"
            <<"core allowed"<<core->canOpenMedia()<<"viewer allowed"<<viewer()->canOpenMedia()
            <<"state"<<core->videoPlatform()->recordingState()<<"opening"<<viewer()->property("openingMedia")
            <<"focus"<<QGuiApplication::focusWindow()<<"modal"<<QApplication::activeModalWidget();
        return opened;
    };
    const auto settled=[&] {
        if(!QTest::qWaitFor([&]{return viewer()&&!viewer()->property("openingMedia").toBool()
            &&!fileDialog(QStringLiteral("nkshotOpenMediaDialog"))&&!replaceDialog();},20000))return false;
        viewer()->show();viewer()->requestActivate();
        return QTest::qWaitFor([&]{return QGuiApplication::focusWindow()==viewer();},2000);
    };
    const auto confirm=[&](QMessageBox::StandardButton choice) {
        auto dialog=replaceDialog();auto button=dialog?dialog->button(choice):nullptr;
        if(!button)return false;
        QTest::mouseClick(button,Qt::LeftButton);
        return true;
    };
    const auto select=[&](const QString &path,bool discard=true) {
        if(!open()||!choose(fileDialog(QStringLiteral("nkshotOpenMediaDialog")),path))return false;
        if(!QTest::qWaitFor([&]{return replaceDialog()||!viewer()->property("openingMedia").toBool();},20000))return false;
        if(replaceDialog()&&!confirm(discard?QMessageBox::Discard:QMessageBox::Cancel))return false;
        return settled();
    };
    // Cancel before a file is selected cannot replace the current document.
    const auto initial=document->renderToImage();const auto initialDepth=document->undoStackDepth();
    if(!open())return fail(QStringLiteral("Actual Ctrl+O did not open local-file dialog"));
    QTest::keyClick(fileDialog(QStringLiteral("nkshotOpenMediaDialog")),Qt::Key_Escape);
    if(!settled()||document->renderToImage()!=initial||document->undoStackDepth()!=initialDepth)
        return fail(QStringLiteral("File-dialog cancellation changed original document"));
    {
        auto actual=std::move(core->m_imagePlatform);
        auto delayed=std::make_unique<DelayedOpenImagePlatform>();auto probe=delayed.get();
        // Forward test-only terminal deliveries through the actual platform's
        // pre-existing production connections, including serial-aware release.
        QObject::connect(probe,&ImagePlatform::newScreenshotFailed,actual.get(),&ImagePlatform::newScreenshotFailed);
        core->m_imagePlatform=std::move(delayed);
        const auto restore=qScopeGuard([&]{core->m_imagePlatform=std::move(actual);});
        core->takeNewScreenshot(int(CaptureModeModel::AllScreens),0,false,true,true);
        if(!QTest::qWaitFor([&]{return probe->calls==1&&core->m_pendingImageGrabs==1;},2000))
            return fail(QStringLiteral("Actual screenshot dispatch did not enter inflight state"));
        core->takeNewScreenshot(int(CaptureModeModel::AllScreens),0,false,true,true);
        viewer()->show();QTest::keyClick(viewer(),Qt::Key_O,Qt::ControlModifier);viewer()->openMedia();
        QTest::qWait(100);
        if(probe->calls!=1||core->canOpenMedia()||fileDialog(QStringLiteral("nkshotOpenMediaDialog"))
            ||document->renderToImage()!=initial||document->undoStackDepth()!=initialDepth)
            return fail(QStringLiteral("Inflight screenshot allowed repeat dispatch/Open or changed document"));
        bool nestedObserved=false,nestedBlocked=false;
        QMetaObject::Connection nestedConnection;
        nestedConnection=QObject::connect(InlineMessageModel::instance(),&QAbstractItemModel::rowsInserted,viewer(),
            [&](const QModelIndex &,int,int) {
                QObject::disconnect(nestedConnection); // Rejecting Open may itself insert a message.
                nestedObserved=true;
                QEventLoop nestedLoop;QTimer::singleShot(0,&nestedLoop,&QEventLoop::quit);nestedLoop.exec();
                nestedBlocked=!core->canOpenMedia()&&!viewer()->canOpenMedia();
                viewer()->openMedia();
                nestedBlocked=nestedBlocked&&!fileDialog(QStringLiteral("nkshotOpenMediaDialog"));
            });
        Q_EMIT probe->newScreenshotFailed(QStringLiteral("Controlled screenshot failure for Open guard verification"));
        QObject::disconnect(nestedConnection);
        if(!nestedObserved||!nestedBlocked)
            return fail(QStringLiteral("Controlled nested event loop escaped terminal-handler Open guard"));
        Q_EMIT probe->newScreenshotFailed(QStringLiteral("Controlled duplicate terminal delivery"));
        if(!QTest::qWaitFor([&]{return core->m_pendingImageGrabs==0&&core->canOpenMedia();},2000)
            ||document->renderToImage()!=initial||document->undoStackDepth()!=initialDepth||!open())
            return fail(QStringLiteral("Screenshot terminal failure did not restore Open safely"));
        QTest::keyClick(fileDialog(QStringLiteral("nkshotOpenMediaDialog")),Qt::Key_Escape);
        if(!settled())return fail(QStringLiteral("Recovered Open cancellation did not settle"));
    }
    {
        auto actual=std::move(core->m_videoPlatform);
        core->m_videoPlatform=std::make_unique<FinishedOpenVideoPlatform>();
        const auto restore=qScopeGuard([&]{core->m_videoPlatform=std::move(actual);});
        if(!core->canOpenMedia())return fail(QStringLiteral("Finished recording Core canOpenMedia guard rejected"));
        if(!viewer()->canOpenMedia())return fail(QStringLiteral("Finished recording Viewer canOpenMedia guard rejected"));
        if(!open())return fail(QStringLiteral("Finished recording allowed by guards but actual Open shortcut/dialog failed"));
        QTest::keyClick(fileDialog(QStringLiteral("nkshotOpenMediaDialog")),Qt::Key_Escape);
        if(!settled()||core->videoPlatform()->recordingState()!=VideoPlatform::RecordingState::Finished
            ||document->renderToImage()!=initial||document->undoStackDepth()!=initialDepth)
            return fail(QStringLiteral("Finished-state Open cancellation changed state/document"));
    }
    if(!select(sources[0]))return fail(QStringLiteral("PNG actual dialog/confirmation did not settle"));
    if(core->videoMode())return fail(QStringLiteral("PNG import remained in video mode"));
    if(!core->outputUrl().isEmpty())return fail(QStringLiteral("PNG import retained overwrite output URL: ")+core->outputUrl().toString());
    if(!importedPixels(document->baseImage(),QImage(sources[0]))) {
        const auto expected=QImage(sources[0]);
        qWarning()<<"PNG decoded mismatch"<<document->baseImage().size()<<expected.size()
            <<document->baseImage().format()<<expected.format()<<document->baseImage().colorSpace()<<expected.colorSpace()
            <<"RGBA exact"<<samePixels(document->baseImage(),expected);
        return fail(QStringLiteral("PNG decoded pixels/default color profile contract failed"));
    }
    sizeViewer();viewer()->show();viewer()->requestActivate();
    QSignalSpy annotationFrame(viewer(),&QQuickWindow::frameSwapped);viewer()->update();
    if(!QTest::qWaitFor([&]{return !annotationFrame.isEmpty()&&QGuiApplication::focusWindow()==viewer()
        &&viewer()->rootObject()->size()==QSizeF(viewer()->size());},2000))
        return fail(QStringLiteral("Opened PNG layout/frame/focus did not settle before annotation"));
    QQuickItem *viewport=nullptr;
    if(!QTest::qWaitFor([&]{viewport=visual(viewer()->contentItem(),QStringLiteral("nkshotAnnotationViewport"));
        return viewport&&viewport->isVisible()&&viewport->isEnabled()&&viewport->width()>100&&viewport->height()>100;},1500))
        return fail(QStringLiteral("Opened PNG annotation viewport unavailable"));
    document->tool()->setType(AnnotationTool::PixelateTool);document->tool()->setStrength(0.6);
    QPointF previousA,previousB;bool havePrevious=false;
    QElapsedTimer transformWait;transformWait.start();bool transformStable=false;
    while(transformWait.elapsed()<2500) {
        const auto first=viewport->mapToScene(QPointF(viewport->width()*.2,viewport->height()*.3));
        const auto second=viewport->mapToScene(QPointF(viewport->width()*.7,viewport->height()*.7));
        if(havePrevious&&QLineF(first,second).length()>100
            &&QLineF(first,previousA).length()<1&&QLineF(second,previousB).length()<1) {
            transformStable=true;break;
        }
        previousA=first;previousB=second;havePrevious=true;QTest::qWait(50);
    }
    if(!transformStable)return fail(QStringLiteral("Annotation entrance transform did not settle to usable scene coordinates"));
    const auto a=viewport->mapToScene(QPointF(viewport->width()*.2,viewport->height()*.3)).toPoint();
    const auto b=viewport->mapToScene(QPointF(viewport->width()*.7,viewport->height()*.7)).toPoint();
    qInfo()<<"Actual Open annotation gesture"<<"annotating"<<viewer()->property("annotating")
        <<"tool"<<document->tool()->type()<<"viewport"<<viewport->size()<<viewport->isEnabled()
        <<"start/end"<<a<<b<<"window bounds"<<QRect(QPoint(),viewer()->size())
        <<"focus"<<QGuiApplication::focusWindow();
    if(!QRect(QPoint(),viewer()->size()).contains(a)||!QRect(QPoint(),viewer()->size()).contains(b))
        return fail(QStringLiteral("Annotation gesture lies outside actual window bounds"));
    QTest::mousePress(viewer(),Qt::LeftButton,Qt::NoModifier,a);
    QTest::mouseMove(viewer(),(a+b)/2,30);QTest::mouseMove(viewer(),b,50);
    QTest::mouseRelease(viewer(),Qt::LeftButton,Qt::NoModifier,b);
    if(!QTest::qWaitFor([&]{return document->undoStackDepth()>0;},1500)) {
        qWarning()<<"Annotation failed"<<"annotating"<<viewer()->property("annotating")
            <<"tool"<<document->tool()->type()<<"geometry"<<document->tool()->geometry()
            <<"depth"<<document->undoStackDepth()<<"focus"<<QGuiApplication::focusWindow();
        viewer()->grabWindow().save(screenshot+QStringLiteral(".annotation-failed.png"));
        return fail(QStringLiteral("Actual annotation gesture did not create history"));
    }
    const auto edited=document->renderToImage();const auto depth=document->undoStackDepth();const auto oldVideo=core->currentVideo();
    const auto retained=[&]{return document->renderToImage()==edited&&document->undoStackDepth()==depth
        &&core->currentVideo()==oldVideo&&sourcesIntact();};
    if(!open()||!choose(fileDialog(QStringLiteral("nkshotOpenMediaDialog")),sources[1])
        ||!QTest::qWaitFor([&]{return replaceDialog();},10000)||!retained()||!confirm(QMessageBox::Cancel)||!settled()||!retained())
        return fail(QStringLiteral("Unconfirmed replacement did not preserve edits/history"));
    auto corrupt=fixtures.filePath(QStringLiteral("corrupt.png"));
    {QFile file(corrupt);if(!file.open(QIODevice::WriteOnly))return fail(QStringLiteral("Corrupt fixture unavailable"));file.write("not an image");}
    if(!select(corrupt)||!retained()||!capture(QStringLiteral(".failed.png")))return fail(QStringLiteral("Invalid image failed preservation/recovery"));
    // Ctrl+S on an imported file must ask for a new destination, not overwrite it.
    QTest::keyClick(viewer(),Qt::Key_S,Qt::ControlModifier);
    if(!QTest::qWaitFor([&]{return fileDialog()&&fileDialog()->acceptMode()==QFileDialog::AcceptSave;},2000))
        return fail(QStringLiteral("Imported image Ctrl+S did not use Save As"));
    QPointer<QFileDialog> pendingSave=fileDialog();
    QTest::keyClick(pendingSave,Qt::Key_O,Qt::ControlModifier);
    viewer()->openMedia(); // Deliberately bypass modality to exercise Core's guard.
    QTest::qWait(100);
    if(fileDialog(QStringLiteral("nkshotOpenMediaDialog"))||!pendingSave||!pendingSave->isVisible()||!retained())
        return fail(QStringLiteral("Open bypassed pending Save As guard or changed edited media"));
    QTest::keyClick(pendingSave,Qt::Key_Escape);
    if(!QTest::qWaitFor([&]{return !pendingSave||!pendingSave->isVisible();},1500)||!retained()||!open())
        return fail(QStringLiteral("Save As cancellation did not restore Open availability"));
    QTest::keyClick(fileDialog(QStringLiteral("nkshotOpenMediaDialog")),Qt::Key_Escape);
    if(!settled()||!retained())return fail(QStringLiteral("Open cancel after Save As altered edited media"));
    QTest::keyClick(viewer(),Qt::Key_S,Qt::ControlModifier);
    if(!QTest::qWaitFor([&]{return fileDialog()&&fileDialog()->acceptMode()==QFileDialog::AcceptSave;},2000))
        return fail(QStringLiteral("Save As retry unavailable after guarded Open"));
    auto save=fileDialog();
    if(save->selectedFiles().contains(sources[0]))return fail(QStringLiteral("Save default targets imported original"));
    auto saved=fixtures.filePath(QStringLiteral("edited copy.png"));
    if(!choose(save,saved)||!QTest::qWaitFor([&]{return QFile::exists(saved)&&!fileDialog();},3000)
        ||!samePixels(QImage(saved),edited)||!sourcesIntact())return fail(QStringLiteral("Explicit copy save/pixels/source isolation failed"));
    for(int i=1;i<sources.size();++i) {
        if(!select(sources[i])||!importedPixels(document->baseImage(),QImage(sources[i]))||core->videoMode()||!sourcesIntact())
            return fail(QStringLiteral("Actual image format open failed: ")+sources[i]);
    }
    if(!capture(QStringLiteral(".image.png")))return fail(QStringLiteral("Image screenshot failed"));
    if(!qEnvironmentVariableIsSet("NKSHOT_TEST_COMPACT")&&!qEnvironmentVariableIsSet("NKSHOT_TEST_DARK")) {
        QImage longPage(1200,16000,QImage::Format_RGB32);longPage.fill(Qt::white);
        QPainter painter(&longPage);auto font=painter.font();font.setPixelSize(38);painter.setFont(font);
        for(int y=0;y<longPage.height();y+=400) {
            painter.fillRect(0,y,1200,400,QColor::fromHsv((y/400*31)%360,30,245));
            painter.setPen(QColor(25,35,45));
            painter.drawText(QRect(50,y+30,1100,140),Qt::AlignLeft|Qt::AlignVCenter,
                QStringLiteral("Synthetic long page • section %1\nLocal image, no personal content").arg(y/400+1));
            painter.fillRect(50,y+220,300+y/40,6,QColor(45,100,180));
        }
        painter.end();const auto path=fixtures.filePath(QStringLiteral("long page 19MP.png"));
        if(!longPage.save(path)||!select(path)||document->baseImage().size()!=QSize(1200,16000))
            return fail(QStringLiteral("Greater-than-16MP real PNG import failed"));
        auto viewport=visual(viewer()->contentItem(),QStringLiteral("nkshotAnnotationViewport"));
        if(!QTest::qWaitFor([&]{return viewport&&viewport->isVisible()&&viewport->width()>100&&viewport->height()>100;},2000)
            ||!capture(QStringLiteral(".long-fit.png")))return fail(QStringLiteral("Long image actual viewer evidence failed"));
        viewer()->requestActivate();
        if(!QTest::qWaitFor([&]{return QGuiApplication::focusWindow()==viewer();},1500))return fail(QStringLiteral("Long image zoom focus unavailable"));
        QTest::keyClick(viewer(),Qt::Key_Plus,Qt::ControlModifier);QTest::qWait(100);
        QTest::keyClick(viewer(),Qt::Key_Plus,Qt::ControlModifier);
        if(!capture(QStringLiteral(".long.png")))return fail(QStringLiteral("Long image zoomed evidence failed"));
    }
    const auto movie=qEnvironmentVariable("NKSHOT_TEST_LOCAL_MEDIA_VIDEO",QStringLiteral("/logs/playback062.mp4"));
    const auto movieBytes=bytes(movie);
    if(movieBytes.isEmpty())return fail(QStringLiteral("Real synthetic MP4 fixture missing"));
    const auto beforeMovie=document->renderToImage();const auto beforeDepth=document->undoStackDepth();
    const auto oldPath=qgetenv("PATH");const bool hadPath=qEnvironmentVariableIsSet("PATH");
    {
        const auto restore=qScopeGuard([&]{if(hadPath)qputenv("PATH",oldPath);else qunsetenv("PATH");});
        qputenv("PATH",fixtures.path().toUtf8());
        if(!select(movie)||document->renderToImage()!=beforeMovie||document->undoStackDepth()!=beforeDepth||core->videoMode())
            return fail(QStringLiteral("Missing ffprobe did not preserve current image"));
    }
    if(!select(movie)||!core->videoMode()||core->currentVideo()!=QUrl::fromLocalFile(movie))
        return fail(QStringLiteral("MP4 retry after restoring ffprobe failed"));
    if(!qEnvironmentVariableIsSet("NKSHOT_TEST_COMPACT")&&!qEnvironmentVariableIsSet("NKSHOT_TEST_DARK")) {
        viewer()->requestActivate();
        if(!QTest::qWaitFor([&]{return QGuiApplication::focusWindow()==viewer();},1500))
            return fail(QStringLiteral("Video Save As viewer focus unavailable"));
        QTest::keyClick(viewer(),Qt::Key_S,Qt::ControlModifier|Qt::ShiftModifier);
        if(!QTest::qWaitFor([&]{return fileDialog()&&fileDialog()->acceptMode()==QFileDialog::AcceptSave;},2000))
            return fail(QStringLiteral("Imported video actual Save As shortcut failed"));
        const auto copy=fixtures.filePath(QStringLiteral("whole imported movie copy.mp4"));
        if(!choose(fileDialog(),copy)||!QTest::qWaitFor([&]{return !fileDialog()&&core->canOpenMedia()&&bytes(copy)==movieBytes;},10000)
            ||bytes(movie)!=movieBytes||!sourcesIntact())
            return fail(QStringLiteral("Whole-video Save As failed byte-exact copy or moved source"));
        if(!select(movie)||!core->videoMode()||core->currentVideo()!=QUrl::fromLocalFile(movie))
            return fail(QStringLiteral("Original video reopen after Save As failed"));
    }
    sizeViewer();QQuickItem *recording=nullptr;QObject *player=nullptr;
    if(!QTest::qWaitFor([&]{recording=visual(viewer()->contentItem(),QStringLiteral("nkshotRecordingView"));
        player=recording?recording->findChild<QObject *>(QStringLiteral("nkshotMediaPlayer")):nullptr;
        return player&&recording->property("hasContent").toBool();},15000))return fail(QStringLiteral("Imported MP4 did not really decode in Qt"));
    const auto click=[&](const QString &name) {
        QQuickItem *item=nullptr;
        if(!QTest::qWaitFor([&]{item=visual(viewer()->contentItem(),name);return item&&item->isVisible()&&item->isEnabled();},1500))return false;
        QTest::mouseClick(viewer(),Qt::LeftButton,Qt::NoModifier,item->mapToScene(QPointF(item->width()/2,item->height()/2)).toPoint());return true;
    };
    // The production playback toolbar reveals itself on actual pointer hover.
    QTest::mouseMove(viewer(),recording->mapToScene(QPointF(recording->width()/2,recording->height()-35)).toPoint());
    if(!recording->property("playing").toBool()&&!click(QStringLiteral("nkshotPlaybackToggle")))return fail(QStringLiteral("Playback control unavailable"));
    const auto position=player->property("position").toLongLong();
    if(!QTest::qWaitFor([&]{return player->property("position").toLongLong()>position+100;},3000))
        return fail(QStringLiteral("Imported MP4 playback position did not advance"));
    if(!click(QStringLiteral("nkshotPlaybackSettings"))||!click(QStringLiteral("nkshotOpenClip")))return fail(QStringLiteral("Imported MP4 clip entry unavailable"));
    auto job=viewer()->property("clipJob").value<QObject *>();
    if(!job||!QTest::qWaitFor([&]{return !job->property("busy").toBool()&&job->property("ready").toBool();},15000))
        return fail(QStringLiteral("Imported MP4 clip inspection failed"));
    const auto editRange=[&](const QString &name,const QString &value,int expected) {
        auto spin=visual(viewer()->contentItem(),name);
        auto edit=spin?qobject_cast<QQuickItem *>(spin->property("contentItem").value<QObject *>()):nullptr;
        if(!edit)return false;
        QTest::mouseClick(viewer(),Qt::LeftButton,Qt::NoModifier,edit->mapToScene(QPointF(edit->width()/2,edit->height()/2)).toPoint());
        edit->forceActiveFocus();QTest::keyClick(viewer(),Qt::Key_A,Qt::ControlModifier);
        for(auto key:value.toLatin1())QTest::keyClick(viewer(),key);
        QTest::keyClick(viewer(),Qt::Key_Tab);
        return QTest::qWaitFor([&]{return spin->property("value").toInt()==expected;},500);
    };
    if(!editRange(QStringLiteral("nkshotClipEnd"),QStringLiteral("3.15"),315)
        ||!editRange(QStringLiteral("nkshotClipStart"),QStringLiteral("1.35"),135)
        ||!capture(QStringLiteral(".clip.png"))||!click(QStringLiteral("nkshotClipExport"))
        ||!QTest::qWaitFor([&]{return fileDialog(QStringLiteral("nkshotClipSaveDialog"));},1500))
        return fail(QStringLiteral("Imported MP4 clip time inputs/Save dialog failed"));
    const auto clip=fixtures.filePath(QStringLiteral("imported ' clip.mp4"));
    if(!choose(fileDialog(QStringLiteral("nkshotClipSaveDialog")),clip)
        ||!QTest::qWaitFor([&]{return !job->property("busy").toBool()&&job->property("outputUrl").toUrl()==QUrl::fromLocalFile(clip);},30000)
        ||bytes(clip).isEmpty()||bytes(movie)!=movieBytes||!sourcesIntact()||core->currentVideo()!=QUrl::fromLocalFile(movie))
        return fail(QStringLiteral("Real imported MP4 clip export/source isolation failed"));
    const auto evidenceClip=screenshot+QStringLiteral(".export-")+QString::number(QDateTime::currentMSecsSinceEpoch())+QStringLiteral(".mp4");
    if(!QFile::copy(clip,evidenceClip))return fail(QStringLiteral("Unable to retain actual export evidence"));
    qInfo()<<"085 actual imported clip artifact"<<evidenceClip;
    if(!capture(QString()))return fail(QStringLiteral("Final imported media screenshot failed"));
    qInfo()<<"085 actual dialogs: PNG/JPEG/static WebP, edit replacement cancel/failure, new-copy Save, missing ffprobe recovery, real MP4 Qt playback and clip export passed";
    return true;
}
