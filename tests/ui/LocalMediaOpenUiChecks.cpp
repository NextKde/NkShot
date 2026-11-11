// SPDX-License-Identifier: GPL-3.0-only
// Actual local-file dialogs and decoders, using generated nonpersonal fixtures.
#include "LocalMediaOpenUiChecks.h"
#include "VideoAnnotationsUiChecks.h"
#include "SpectacleCore.h"
#include "RecentMediaModel.h"
#include "Platforms/ImagePlatform.h"
#include "CaptureModeModel.h"
#include "Gui/ViewerWindow.h"
#include "Gui/PinnedImageWindow.h"
#include "Gui/ImageCopyEditor.h"
#include <QColorDialog>
#include <QVideoSink>
#include <QProcess>
#include <QVideoFrame>
#include "Gui/InlineMessageModel.h"
#include <QEventLoop>
#include <QTimer>
#include <KQuickImageEditor/AnnotationDocument>
#include <KQuickImageEditor/AnnotationTool>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QColorSpace>
#include <cstring>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
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
        if(!QTest::qWaitFor([&]{
            if(QGuiApplication::focusWindow()==viewer())return true;
            // A just-closed native/widget dialog can clear focus after the first
            // activation request. Repeat the supported request while it settles.
            viewer()->requestActivate();return false;
        },1500)) {
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
    if(!qEnvironmentVariableIsSet("NKSHOT_TEST_SKIP_LONG_IMAGE")
        &&!qEnvironmentVariableIsSet("NKSHOT_TEST_COMPACT")&&!qEnvironmentVariableIsSet("NKSHOT_TEST_DARK")) {
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
    const bool webm=QFileInfo(movie).suffix().compare(QStringLiteral("webm"),Qt::CaseInsensitive)==0;
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
        const auto suffix=QFileInfo(movie).suffix().toLower();
        const auto defaults=fileDialog()->selectedFiles();
        if(defaults.isEmpty()||QFileInfo(defaults.first()).suffix().toLower()!=suffix)
            return fail(QStringLiteral("Whole-video Save As default extension does not match original container"));
        const auto copy=fixtures.filePath(QStringLiteral("whole imported movie copy.")+suffix);
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
        auto scroll=visual(viewer()->contentItem(),QStringLiteral("nkshotClipDetailScroll"));
        bool inside=false;for(auto parent=item->parentItem();parent;parent=parent->parentItem())if(parent==scroll)inside=true;
        if(scroll&&inside) {
            const QRectF bounds(scroll->mapToScene(QPointF()),scroll->size());
            const QRectF target(item->mapToScene(QPointF()),item->size());
            if(!bounds.adjusted(-0.5,-0.5,0.5,0.5).contains(target)) {
                auto bar=visual(viewer()->contentItem(),QStringLiteral("nkshotClipDetailScrollbar"));
                if(!bar||!bar->isVisible())return false;
                const qreal pos=bar->property("position").toReal(), size=bar->property("size").toReal();
                const qreal content=scroll->property("contentHeight").toReal();
                if(content<=0)return false;
                const qreal desired=qBound(0.,pos+(target.top()-bounds.top()-2)/content,qMax(0.,1.-size));
                const auto from=bar->mapToScene(QPointF(bar->width()/2,(pos+size/2)*bar->height())).toPoint();
                const auto to=bar->mapToScene(QPointF(bar->width()/2,(desired+size/2)*bar->height())).toPoint();
                QTest::mousePress(viewer(),Qt::LeftButton,Qt::NoModifier,from);
                QTest::mouseMove(viewer(),to,100);QTest::mouseRelease(viewer(),Qt::LeftButton,Qt::NoModifier,to);QTest::qWait(150);
                if(!bounds.adjusted(-0.5,-0.5,0.5,0.5).contains(QRectF(item->mapToScene(QPointF()),item->size())))return false;
            }
        }
        for(auto parent=item->parentItem();parent;parent=parent->parentItem())
            if(parent->property("moving").isValid()&&!QTest::qWaitFor([&]{return !parent->property("moving").toBool();},4000))return false;
        QTest::mouseClick(viewer(),Qt::LeftButton,Qt::NoModifier,item->mapToScene(QPointF(item->width()/2,item->height()/2)).toPoint());return true;
    };
    // The production playback toolbar reveals itself on actual pointer hover.
    QTest::mouseMove(viewer(),recording->mapToScene(QPointF(recording->width()/2,recording->height()-35)).toPoint());
    if(!recording->property("playing").toBool()&&!click(QStringLiteral("nkshotPlaybackToggle")))return fail(QStringLiteral("Playback control unavailable"));
    const auto position=player->property("position").toLongLong();
    if(!QTest::qWaitFor([&]{return player->property("position").toLongLong()>position+100;},3000))
        return fail(QStringLiteral("Imported MP4 playback position did not advance"));
    // Extract the decoded image, not the scaled/letterboxed player window.
    if(!click(QStringLiteral("nkshotPlaybackSettings"))||!capture(QStringLiteral(".frame-menu.png"))
        ||!click(QStringLiteral("nkshotExtractVideoFrame")))
        return fail(QStringLiteral("Frame extraction entry unavailable"));
    PinnedImageWindow *framePin=nullptr;
    if(!QTest::qWaitFor([&]{for(auto w:QGuiApplication::allWindows()) {
        if(w->objectName()==QStringLiteral("nkshotVideoFramePin"))framePin=qobject_cast<PinnedImageWindow *>(w);
    }return framePin&&framePin->isVisible();},2000))return fail(QStringLiteral("Decoded frame pin not shown"));
    auto software=recording->findChild<QObject *>(QStringLiteral("nkshotSoftwareVideoPreview"));
    auto sink=software?software->property("sink").value<QVideoSink *>():nullptr;
    if(!sink||player->property("playing").toBool()||framePin->snapshot()!=sink->videoFrame().toImage())
        return fail(QStringLiteral("Extracted frame differs from paused decoded pixels"));
    const auto extracted=framePin->snapshot();
    framePin->copyImage();
    if(application.clipboard()->image()!=extracted||core->currentVideo()!=QUrl::fromLocalFile(movie)
        ||bytes(movie)!=movieBytes||!sourcesIntact())return fail(QStringLiteral("Frame copy changed source or pixels"));
    QSignalSpy frameSaved(framePin,&PinnedImageWindow::saveFinished);
    framePin->saveAs();
    if(!QTest::qWaitFor([&]{return fileDialog()&&fileDialog()->acceptMode()==QFileDialog::AcceptSave;},1500))
        return fail(QStringLiteral("Frame PNG Save As did not open"));
    const auto frameFile=fixtures.filePath(QStringLiteral("extracted-frame.png"));
    QTest::qWait(200);
    if(!choose(fileDialog(QStringLiteral("nkshotPinSaveDialog")),frameFile))
        return fail(QStringLiteral("Frame PNG dialog interaction failed"));
    if(!QTest::qWaitFor([&]{return !frameSaved.isEmpty();},2000)||!frameSaved.first().at(1).toBool()) {
        if(auto d=fileDialog()) {qWarning()<<"Pin save still open"<<d->selectedFiles();d->grab().save(screenshot+QStringLiteral(".save-dialog.png"));}
        return fail(QStringLiteral("Frame PNG writer failed: ")+(frameSaved.isEmpty()?QStringLiteral("no signal"):frameSaved.first().at(0).toString()));
    }
    const QImage savedFrame(frameFile);
    if(!samePixels(savedFrame,extracted)) {
        qWarning()<<"Frame PNG mismatch"<<frameFile<<savedFrame.size()<<extracted.size()<<savedFrame.format()<<extracted.format()
            <<savedFrame.pixelColor(0,0)<<extracted.pixelColor(0,0);
        QFile::copy(frameFile,screenshot+QStringLiteral(".exported-frame.png"));
        extracted.save(screenshot+QStringLiteral(".expected-frame.png"));
        return fail(QStringLiteral("Frame PNG export pixels changed"));
    }
    QSignalSpy pinFrame(framePin,&QQuickWindow::frameSwapped);framePin->update();
    if(!QTest::qWaitFor([&]{return !pinFrame.isEmpty();},1000)
        ||!framePin->grabWindow().save(screenshot+QStringLiteral(".frame.png")))
        return fail(QStringLiteral("Frame preview evidence failed"));
    if(viewer()->pinVideoFrame(nullptr,QUrl::fromLocalFile(movie))
        ||viewer()->pinVideoFrame(sink,QUrl::fromLocalFile(movie+QStringLiteral(".old"))))
        return fail(QStringLiteral("Frame extraction accepted missing sink or retired source"));
    player->setProperty("position",3000);
    QTest::qWait(400);
    if(framePin->snapshot()!=extracted||bytes(movie)!=movieBytes)
        return fail(QStringLiteral("Seeking changed immutable frame or original recording"));
    if(qEnvironmentVariableIsSet("NKSHOT_TEST_COPY_EDITOR")) {
        const auto mainImage=core->annotationDocument()->renderToImage();
        const auto mainDepth=core->annotationDocument()->undoStackDepth();
        const auto mainColor=core->annotationDocument()->tool()->strokeColor();
        auto editAction=visual(framePin->contentItem(),QStringLiteral("nkshotPinEditCopy"));
        if(!editAction)return fail(QStringLiteral("Pin edit-copy entry missing"));
        QTest::mouseClick(framePin,Qt::LeftButton,Qt::NoModifier,editAction->mapToScene(QPointF(editAction->width()/2,editAction->height()/2)).toPoint());
        QPointer<ImageCopyEditor> editor;
        if(!QTest::qWaitFor([&]{for(auto w:QGuiApplication::allWindows())if(auto e=qobject_cast<ImageCopyEditor *>(w))editor=e;
            return editor&&editor->isVisible()&&editor->status()==QQuickView::Ready;},2000))return fail(QStringLiteral("Independent editor did not load"));
        editor->resize(qEnvironmentVariableIsSet("NKSHOT_TEST_COMPACT")?QSize(420,520):QSize(900,640));QTest::qWait(150);
        if(editor->document()==core->annotationDocument()||!samePixels(editor->document()->renderToImage(),extracted)
            ||framePin->editCopy()!=editor)return fail(QStringLiteral("Editor did not isolate/reuse its own document"));
        const auto hit=[&](const QString &name) {
            auto item=visual(editor->contentItem(),name);
            if(!item||!item->isVisible()||!item->isEnabled())return false;
            const auto point=item->mapToScene(QPointF(item->width()/2,item->height()/2));
            if(!QRectF(QPointF(),editor->size()).contains(point))return false;
            QTest::mouseClick(editor,Qt::LeftButton,Qt::NoModifier,point.toPoint());return true;
        };
        if(!hit(QStringLiteral("nkshotFreehandTool")))return fail(QStringLiteral("Shared freehand tool inaccessible"));
        editor->showColorDialog(AnnotationTool::StrokeOption);
        QColorDialog *color=nullptr;
        if(!QTest::qWaitFor([&]{for(auto w:QApplication::topLevelWidgets())if(w->objectName()==QStringLiteral("nkshotCopyColorDialog"))color=qobject_cast<QColorDialog *>(w);return color&&color->isVisible();},1000))
            return fail(QStringLiteral("Isolated color chooser failed"));
        color->setCurrentColor(QColor("#ec3974"));color->accept();QTest::qWait(100);
        if(editor->document()->tool()->strokeColor()!=QColor("#ec3974")||core->annotationDocument()->tool()->strokeColor()!=mainColor)
            return fail(QStringLiteral("Color chooser mutated the main editor"));
        auto viewport=visual(editor->contentItem(),QStringLiteral("nkshotAnnotationViewport"));
        if(!viewport)return fail(QStringLiteral("Shared independent viewport missing"));
        const QPointF a(extracted.width()*.2,extracted.height()*.25),b(extracted.width()*.75,extracted.height()*.7);
        QTest::mousePress(editor,Qt::LeftButton,Qt::NoModifier,viewport->mapToScene(a).toPoint());
        for(int i=1;i<=12;++i)QTest::mouseMove(editor,viewport->mapToScene(a+(b-a)*(i/12.)).toPoint(),15);
        QTest::mouseRelease(editor,Qt::LeftButton,Qt::NoModifier,viewport->mapToScene(b).toPoint());
        if(!QTest::qWaitFor([&]{return editor->document()->undoStackDepth()>0;},1000))return fail(QStringLiteral("Independent drawing did not create history"));
        const auto edited=editor->document()->renderToImage();
        if(samePixels(edited,extracted)||!hit(QStringLiteral("nkshotUndo"))||!samePixels(editor->document()->renderToImage(),extracted)
            ||!hit(QStringLiteral("nkshotRedo"))||!samePixels(editor->document()->renderToImage(),edited))
            return fail(QStringLiteral("Independent undo/redo failed"));
        editor->close();QMessageBox *discard=nullptr;
        if(!QTest::qWaitFor([&]{for(auto w:QApplication::topLevelWidgets())if(w->objectName()==QStringLiteral("nkshotDiscardCopyDialog"))discard=qobject_cast<QMessageBox *>(w);return discard&&discard->isVisible();},1000))
            return fail(QStringLiteral("Unsaved independent edit closed without protection"));
        QTest::mouseClick(discard->button(QMessageBox::Cancel),Qt::LeftButton);QTest::qWait(100);
        if(!editor||!editor->isVisible()||!samePixels(editor->document()->renderToImage(),edited))return fail(QStringLiteral("Cancel discarded independent edits"));
        if(!hit(QStringLiteral("nkshotCopyEditorCopy"))||!samePixels(application.clipboard()->image(),edited))return fail(QStringLiteral("Independent copy pixels changed"));
        // Save the edited revision directly, without creating a pin or replacing the source.
        QSignalSpy editorSave(editor,&ImageCopyEditor::saveFinished);
        auto saveDialog=[&]() -> QFileDialog * {
            for(auto w:QApplication::topLevelWidgets())
                if(w->objectName()==QStringLiteral("nkshotCopySaveDialog"))return qobject_cast<QFileDialog *>(w);
            return nullptr;
        };
        if(!hit(QStringLiteral("nkshotCopyEditorSave"))||!QTest::qWaitFor([&]{return saveDialog()&&saveDialog()->isVisible();},1000))
            return fail(QStringLiteral("Direct image save entry did not open"));
        QPointer<QFileDialog> save=saveDialog();editor->saveAs();
        if(saveDialog()!=save)return fail(QStringLiteral("Repeated direct save created another dialog"));
        editor->close();
        if(!editor||!editor->isVisible())return fail(QStringLiteral("Closing behind save dialog lost the editor"));
        save->reject();
        if(!QTest::qWaitFor([&]{return save.isNull();},1000)||!editorSave.isEmpty()||!editor->hasUnsavedChanges())
            return fail(QStringLiteral("Cancelled direct save marked edits saved"));
        const auto exportEditor=[&](const QString &path) {
            editor->saveAs();
            if(!QTest::qWaitFor([&]{return saveDialog()&&saveDialog()->isVisible();},1000))return false;
            save=saveDialog();save->setOption(QFileDialog::DontUseNativeDialog);save->selectFile(path);QTest::qWait(200);
            const auto box=save->findChild<QDialogButtonBox *>();
            if(!box||!box->button(QDialogButtonBox::Save))return false;
            QTest::mouseClick(box->button(QDialogButtonBox::Save),Qt::LeftButton);
            return QTest::qWaitFor([&]{return save.isNull();},1500);
        };
        const auto editedPath=screenshot+QStringLiteral(".edited-%1.png").arg(QDateTime::currentMSecsSinceEpoch());
        if(!exportEditor(editedPath+QStringLiteral(".jpg"))||editorSave.size()!=1||editorSave.last().at(1).toBool()
            ||QFileInfo::exists(editedPath+QStringLiteral(".jpg"))||!editor->hasUnsavedChanges())
            return fail(QStringLiteral("Invalid direct save changed success/dirty state"));
        if(!exportEditor(editedPath)||editorSave.size()!=2||!editorSave.last().at(1).toBool()
            ||!samePixels(QImage(editedPath),edited)||editor->hasUnsavedChanges()||!editor->isVisible())
            return fail(QStringLiteral("Direct PNG save did not preserve exact revision/session"));
        if(!hit(QStringLiteral("nkshotUndo"))||!editor->hasUnsavedChanges())return fail(QStringLiteral("Undo after save lost unsaved protection"));
        editor->close();discard=nullptr;
        if(!QTest::qWaitFor([&]{for(auto w:QApplication::topLevelWidgets())if(w->objectName()==QStringLiteral("nkshotDiscardCopyDialog"))discard=qobject_cast<QMessageBox *>(w);return discard&&discard->isVisible();},1000))
            return fail(QStringLiteral("Changed saved revision closed silently"));
        QTest::mouseClick(discard->button(QMessageBox::Cancel),Qt::LeftButton);QTest::qWait(100);
        if(!hit(QStringLiteral("nkshotRedo"))||editor->hasUnsavedChanges()||!samePixels(QImage(editedPath),edited))
            return fail(QStringLiteral("Redo did not restore saved revision; or export was modified"));
        // Verify an actual edited canvas pixel in the window, not only the model.
        QList<QPoint> strokePixels;
        for(int y=0;y<edited.height();++y)for(int x=0;x<edited.width();++x) {
            const auto c=edited.pixelColor(x,y),o=extracted.pixelColor(x,y);
            if(qAbs(c.red()-236)<8&&qAbs(c.green()-57)<8&&qAbs(c.blue()-116)<8
                &&qAbs(c.red()-o.red())+qAbs(c.green()-o.green())+qAbs(c.blue()-o.blue())>160)strokePixels.append({x,y});
        }
        if(strokePixels.size()<12)return fail(QStringLiteral("No strong edited stroke pixels found"));
        QImage editorShot;
        const auto painted=[&] {
            editor->update();editorShot=editor->grabWindow();
            int matched=0;
            // Check the length of the stroke, not a single point that may coincide
            // with a neighboring fixture color before the canvas redraw arrives.
            for(int i=0;i<12;++i){
                const auto location=viewport->mapToScene(strokePixels.at((strokePixels.size()-1)*i/11)).toPoint();
                bool found=false;
                for(int y=location.y()-2;y<=location.y()+2;++y)for(int x=location.x()-2;x<=location.x()+2;++x) {
                    if(!editorShot.rect().contains(x,y))continue;
                    const auto c=editorShot.pixelColor(x,y);
                    if(qAbs(c.red()-236)<30&&qAbs(c.green()-57)<30&&qAbs(c.blue()-116)<30)found=true;
                }
                matched+=found;
            }
            return matched>=10;
        };
        if(!QTest::qWaitFor(painted,1500)||!editorShot.save(screenshot+QStringLiteral(".editor.png")))
            return fail(QStringLiteral("Edited canvas did not visibly repaint after redo"));
        if(!hit(QStringLiteral("nkshotCopyEditorDone")))return fail(QStringLiteral("Independent Done action unavailable"));
        QPointer<PinnedImageWindow> result;
        if(!QTest::qWaitFor([&]{for(auto w:QGuiApplication::allWindows())if(w->objectName()==QStringLiteral("nkshotEditedImagePin"))result=qobject_cast<PinnedImageWindow *>(w);return result&&editor.isNull();},2000)
            ||!samePixels(result->snapshot(),edited)||!samePixels(framePin->snapshot(),extracted)
            ||core->annotationDocument()->renderToImage()!=mainImage||core->annotationDocument()->undoStackDepth()!=mainDepth
            ||core->currentVideo()!=QUrl::fromLocalFile(movie)||bytes(movie)!=movieBytes)
            return fail(QStringLiteral("Independent result altered original pin/main media/history"));
        result->close();
        auto fresh=qobject_cast<ImageCopyEditor *>(framePin->editCopy());
        if(!fresh||fresh->document()->undoStackDepth()!=0||!samePixels(fresh->document()->renderToImage(),extracted))
            return fail(QStringLiteral("New edit session reused an old edited copy"));
        QPointer<ImageCopyEditor> freshGuard=fresh;fresh->close();
        if(!QTest::qWaitFor([&]{return freshGuard.isNull();},1000))return fail(QStringLiteral("Unchanged independent editor did not close"));
        fresh=qobject_cast<ImageCopyEditor *>(framePin->editCopy());freshGuard=fresh;
        if(!fresh)return fail(QStringLiteral("Independent crop session failed to reopen"));
        fresh->document()->cropCanvas(QRectF(0,0,100,80));
        if(fresh->document()->renderToImage().size()!=QSize(100,80))return fail(QStringLiteral("Isolated crop model failed"));
        fresh->close();discard=nullptr;
        if(!QTest::qWaitFor([&]{for(auto w:QApplication::topLevelWidgets())if(w->objectName()==QStringLiteral("nkshotDiscardCopyDialog"))discard=qobject_cast<QMessageBox *>(w);return discard&&discard->isVisible();},1000))
            return fail(QStringLiteral("Cropped session lost discard protection"));
        QTest::mouseClick(discard->button(QMessageBox::Discard),Qt::LeftButton);
        if(!QTest::qWaitFor([&]{return freshGuard.isNull();},1000)||!samePixels(framePin->snapshot(),extracted)
            ||core->annotationDocument()->renderToImage()!=mainImage||core->annotationDocument()->undoStackDepth()!=mainDepth)
            return fail(QStringLiteral("Discard leaked crop into original documents"));
        editor=qobject_cast<ImageCopyEditor *>(framePin->editCopy());
        if(!editor)return fail(QStringLiteral("Saved-close session did not open"));
        editor->document()->cropCanvas(QRectF(0,0,100,80));
        if(!exportEditor(editedPath+QStringLiteral(".crop.png"))||editor->hasUnsavedChanges())
            return fail(QStringLiteral("Cropped copy did not save independently"));
        editor->close();
        if(!QTest::qWaitFor([&]{return editor.isNull();},1000)||!samePixels(framePin->snapshot(),extracted))
            return fail(QStringLiteral("Saved copy did not close cleanly or changed original"));
    }
    framePin->close();
    QTest::mouseMove(viewer(),recording->mapToScene(QPointF(recording->width()/2,recording->height()-35)).toPoint());
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
    const bool resolutionExport=qEnvironmentVariableIsSet("NKSHOT_TEST_RESOLUTION_CLIP");
    const bool silentExport=qEnvironmentVariableIsSet("NKSHOT_TEST_SILENT_CLIP");
    const bool cropExport=qEnvironmentVariableIsSet("NKSHOT_TEST_CROP_CLIP");
    const bool privacyExport=qEnvironmentVariableIsSet("NKSHOT_TEST_PRIVACY_CLIP");
    QRect expectedPrivacy;
    const bool annotationExport=qEnvironmentVariableIsSet("NKSHOT_TEST_ANNOTATION_CLIP");
    QRect expectedCrop;
    if(!click(QStringLiteral("nkshotClipDetails"))||!capture(QStringLiteral(".details.png")))
        return fail(QStringLiteral("Export details could not be opened"));
    if(resolutionExport) {
        if(job->property("outputResolution").toString()!=QStringLiteral("original")||!click(QStringLiteral("nkshotClipResolution")))return fail(QStringLiteral("Resolution default/control unavailable"));
        QTest::keyClick(viewer(),Qt::Key_Home);QTest::keyClick(viewer(),Qt::Key_Down);QTest::keyClick(viewer(),Qt::Key_Down);QTest::keyClick(viewer(),Qt::Key_Return);
        if(!QTest::qWaitFor([&]{return job->property("outputResolution").toString()==QStringLiteral("720p");},1000))return fail(QStringLiteral("Resolution selection did not reach job"));
        const auto output=job->property("outputSize").toSize(),source=job->property("sourceSize").toSize();
        if(output.isEmpty()||output.width()>source.width()||output.height()>source.height()||qMax(output.width(),output.height())>1280
            ||!capture(QStringLiteral(".resolution-options.png")))return fail(QStringLiteral("Resolution dimensions/preview invalid"));
    }
    if(silentExport) {
        if(!job->property("includeAudio").toBool()||!click(QStringLiteral("nkshotClipIncludeAudio"))
            ||job->property("includeAudio").toBool()||!capture(QStringLiteral(".silent-options.png")))
            return fail(QStringLiteral("Silent export option did not update actual job"));
    }
    if(cropExport) {
        if(!job->property("cropAvailable").toBool()||job->property("cropAspect").toString()!=QStringLiteral("original")
            ||!click(QStringLiteral("nkshotClipCropAspect")))return fail(QStringLiteral("Crop preset unavailable/default incorrect"));
        QTest::keyClick(viewer(),Qt::Key_Home);QTest::keyClick(viewer(),Qt::Key_Down);QTest::keyClick(viewer(),Qt::Key_Down);QTest::keyClick(viewer(),Qt::Key_Return);
        if(!QTest::qWaitFor([&]{return job->property("cropAspect").toString()==QStringLiteral("portrait");},1000))
            return fail(QStringLiteral("Actual center-crop choice did not reach encoder"));
        expectedCrop=job->property("cropRect").toRect();
        const auto sourceSize=job->property("sourceSize").toSize();
        auto overlay=visual(viewer()->contentItem(),QStringLiteral("nkshotVideoCropOverlay"));
        auto frame=visual(viewer()->contentItem(),QStringLiteral("nkshotVideoCropFrame"));
        if(!overlay||!frame||!overlay->isVisible()||expectedCrop.width()>=sourceSize.width()||expectedCrop.height()!=sourceSize.height()
            ||!QTest::qWaitFor([&]{return frame->height()>10&&qAbs(frame->width()/frame->height()-qreal(expectedCrop.width())/expectedCrop.height())<.01;},1000)
            ||!capture(QStringLiteral(".crop-options.png")))return fail(QStringLiteral("Crop preview does not reflect export geometry"));
        if(qEnvironmentVariableIsSet("NKSHOT_TEST_REFRAME_CLIP")) {
            const auto center=frame->mapToScene(QPointF(frame->width()/2,frame->height()/2)).toPoint();
            QTest::mousePress(viewer(),Qt::LeftButton,Qt::NoModifier,center);
            for(int i=1;i<=6;++i)QTest::mouseMove(viewer(),center+QPoint(i*3,0),20);
            QTest::mouseRelease(viewer(),Qt::LeftButton,Qt::NoModifier,center+QPoint(18,0));
            if(!QTest::qWaitFor([&]{return job->property("cropRect").toRect().x()>expectedCrop.x();},1000)
                ||job->property("cropCentered").toBool())return fail(QStringLiteral("Actual crop drag did not reframe the export"));
            auto moved=job->property("cropRect").toRect();
            QTest::keyClick(viewer(),Qt::Key_Left);
            if(job->property("cropRect").toRect().x()!=moved.x()-2)return fail(QStringLiteral("Crop arrow fine adjustment failed"));
            QTest::keyClick(viewer(),Qt::Key_Home);
            if(job->property("cropRect").toRect()!=expectedCrop||!job->property("cropCentered").toBool())
                return fail(QStringLiteral("Crop Home did not restore center"));
            QTest::keyClick(viewer(),Qt::Key_Right,Qt::ShiftModifier);
            if(job->property("cropRect").toRect().x()!=expectedCrop.x()+10||!click(QStringLiteral("nkshotClipCenterCrop"))
                ||job->property("cropRect").toRect()!=expectedCrop)return fail(QStringLiteral("Crop fast adjustment/Center action failed"));
            // A second independent drag starts from the current rectangle, not
            // accumulated pointer translation from the previous gesture.
            const auto again=frame->mapToScene(QPointF(frame->width()/2,frame->height()/2)).toPoint();
            QTest::mousePress(viewer(),Qt::LeftButton,Qt::NoModifier,again);
            for(int i=1;i<=6;++i)QTest::mouseMove(viewer(),again-QPoint(i*3,0),20);
            QTest::mouseRelease(viewer(),Qt::LeftButton,Qt::NoModifier,again-QPoint(18,0));
            if(!QTest::qWaitFor([&]{return job->property("cropRect").toRect().x()<expectedCrop.x();},1000))
                return fail(QStringLiteral("Repeated crop gesture did not move left independently"));
            expectedCrop=job->property("cropRect").toRect();
            if(!capture(QStringLiteral(".reframe-options.png")))return fail(QStringLiteral("Reframed preview evidence failed"));
        }
    }
    auto cancelControl=visual(viewer()->contentItem(),QStringLiteral("nkshotClipCancel"));
    auto previewControl=visual(viewer()->contentItem(),QStringLiteral("nkshotSoftwareVideoPreview"));
    if(!cancelControl||!previewControl||previewControl->height()<50
        ||cancelControl->mapToScene(QPointF(0,cancelControl->height())).y()>viewer()->height()
        ||!click(QStringLiteral("nkshotClipDetails")))return fail(QStringLiteral("Details hides preview or cancel in the real window"));
    if(qEnvironmentVariableIsSet("NKSHOT_TEST_REFRAME_CLIP")) {
        QTest::qWait(150);
        auto frame=visual(viewer()->contentItem(),QStringLiteral("nkshotVideoCropFrame"));
        if(!frame)return fail(QStringLiteral("Collapsed crop frame missing"));
        const auto point=frame->mapToScene(QPointF(frame->width()/2,frame->height()/2)).toPoint();
        QTest::mousePress(viewer(),Qt::LeftButton,Qt::NoModifier,point);
        for(int i=1;i<=5;++i)QTest::mouseMove(viewer(),point+QPoint(i*2,0),20);
        QTest::mouseRelease(viewer(),Qt::LeftButton,Qt::NoModifier,point+QPoint(10,0));
        if(!QTest::qWaitFor([&]{return job->property("cropRect").toRect().x()>expectedCrop.x();},1000))
            return fail(QStringLiteral("Collapsed preview did not keep crop interaction"));
        expectedCrop=job->property("cropRect").toRect();
    }
    if(privacyExport) {
        if(job->property("privacyEnabled").toBool()||!job->property("privacyAvailable").toBool()
            ||!click(QStringLiteral("nkshotClipDetails")))
            return fail(QStringLiteral("Privacy cover option unavailable or not initially off"));
        QTest::qWait(150);
        if(!click(QStringLiteral("nkshotClipPrivacyEnabled")))return fail(QStringLiteral("Privacy checkbox unavailable after layout"));
        auto overlay=visual(viewer()->contentItem(),QStringLiteral("nkshotVideoPrivacyOverlay"));
        auto frame=visual(viewer()->contentItem(),QStringLiteral("nkshotVideoPrivacyFrame"));
        if(!overlay||!frame||!QTest::qWaitFor([&]{return overlay->isVisible()&&frame->width()>12&&frame->height()>12;},1000))
            return fail(QStringLiteral("Enabled privacy preview missing"));
        const auto original=job->property("privacyRect").toRect();
        auto point=frame->mapToScene(QPointF(frame->width()/2,frame->height()/2)).toPoint();
        QTest::mousePress(viewer(),Qt::LeftButton,Qt::NoModifier,point);
        for(int i=1;i<=5;++i)QTest::mouseMove(viewer(),point+QPoint(i*2,0),20);
        QTest::mouseRelease(viewer(),Qt::LeftButton,Qt::NoModifier,point+QPoint(10,0));
        if(!QTest::qWaitFor([&]{return job->property("privacyRect").toRect().x()>original.x();},1000))
            return fail(QStringLiteral("Actual privacy move gesture did not update source rectangle"));
        auto moved=job->property("privacyRect").toRect();
        QTest::keyClick(viewer(),Qt::Key_Left);
        if(job->property("privacyRect").toRect().x()!=moved.x()-2)return fail(QStringLiteral("Privacy keyboard move failed"));
        QTest::keyClick(viewer(),Qt::Key_Right,Qt::ControlModifier|Qt::ShiftModifier);
        const auto resized = job->property("privacyRect").toRect();
        const auto expectedWidth = qMin(moved.width() + 10, job->property("sourceSize").toSize().width() - resized.x()) & ~1;
        if (resized.width() != expectedWidth) return fail(QStringLiteral("Privacy keyboard resize failed"));
        // A reframed preview may drag to the source edge. Make room through real
        // keyboard movement before asserting the corner can grow in both axes.
        for (int i = 0; i < 80 && job->property("privacyRect").toRect().x() > 20; ++i)
            QTest::keyClick(viewer(), Qt::Key_Left, Qt::ShiftModifier);
        for (int i = 0; i < 80 && job->property("privacyRect").toRect().y() > 20; ++i)
            QTest::keyClick(viewer(), Qt::Key_Up, Qt::ShiftModifier);
        auto handle=visual(viewer()->contentItem(),QStringLiteral("nkshotVideoPrivacyHandle_3"));
        if(!handle)return fail(QStringLiteral("Privacy corner missing"));
        const auto beforeResize=job->property("privacyRect").toRect();
        point=handle->mapToScene(QPointF(handle->width()/2,handle->height()/2)).toPoint();
        QTest::mousePress(viewer(),Qt::LeftButton,Qt::NoModifier,point);
        for(int i=1;i<=4;++i)QTest::mouseMove(viewer(),point+QPoint(i*2,i*2),20);
        QTest::mouseRelease(viewer(),Qt::LeftButton,Qt::NoModifier,point+QPoint(8,8));
        if(!QTest::qWaitFor([&]{auto r=job->property("privacyRect").toRect();return r.width()>beforeResize.width()&&r.height()>beforeResize.height();},1000))
            return fail(QStringLiteral("Actual privacy corner resize failed"));
        expectedPrivacy=job->property("privacyRect").toRect();
        for(int i=0;i<80;++i){QTest::keyClick(viewer(),Qt::Key_Right,Qt::ShiftModifier);QTest::keyClick(viewer(),Qt::Key_Down,Qt::ShiftModifier);}
        const auto edge=job->property("privacyRect").toRect();const auto source=job->property("sourceSize").toSize();
        if(edge.size()!=expectedPrivacy.size()||edge.x()+edge.width()!=source.width()||edge.y()+edge.height()!=source.height())
            return fail(QStringLiteral("Moving privacy cover to boundary shrank the protected region"));
        QMetaObject::invokeMethod(job,"setPrivacyRect",Q_ARG(int,expectedPrivacy.x()),Q_ARG(int,expectedPrivacy.y()),
            Q_ARG(int,expectedPrivacy.width()),Q_ARG(int,expectedPrivacy.height()));
        if(!click(QStringLiteral("nkshotClipDetails")))return fail(QStringLiteral("Privacy details unavailable"));
        QTest::qWait(150);
        if(previewControl->height()<50||cancelControl->mapToScene(QPointF(0,cancelControl->height())).y()>viewer()->height())
            return fail(QStringLiteral("Enabled privacy details hide preview or footer"));
        if(!click(QStringLiteral("nkshotClipPrivacyEnabled"))
            ||job->property("privacyEnabled").toBool()||overlay->isVisible()
            ||!click(QStringLiteral("nkshotClipPrivacyEnabled"))||!job->property("privacyEnabled").toBool()
            ||job->property("privacyRect").toRect()!=expectedPrivacy||!capture(QStringLiteral(".privacy.png")))
            return fail(QStringLiteral("Privacy toggle lost region or failed to hide preview"));
    }
    if(annotationExport) {
        const auto error=runVideoAnnotationsUiChecks(viewer(),sink,screenshot);
        if(!error.isEmpty())return fail(error);
    }
    if(!editRange(QStringLiteral("nkshotClipEnd"),QStringLiteral("3.15"),315)
        ||!editRange(QStringLiteral("nkshotClipStart"),QStringLiteral("1.35"),135)
        ||!capture(QStringLiteral(".clip.png"))||!click(QStringLiteral("nkshotClipExport"))
        ||!QTest::qWaitFor([&]{return fileDialog(QStringLiteral("nkshotClipSaveDialog"));},1500))
        return fail(QStringLiteral("Imported MP4 clip time inputs/Save dialog failed"));
    if(annotationExport) {
        const auto overlay=job->property("annotationOverlay").value<QImage>();
        QMetaObject::invokeMethod(job,"clearAnnotationOverlay");
        const auto stale=fixtures.filePath(QStringLiteral("stale-annotations.mp4"));
        if(!choose(fileDialog(QStringLiteral("nkshotClipSaveDialog")),stale)
            ||!QTest::qWaitFor([&]{return !viewer()->property("choosingClipOutput").toBool();},1500)
            ||QFileInfo::exists(stale)||job->property("busy").toBool())return fail(QStringLiteral("Stale annotation settings reached encoder"));
        QMetaObject::invokeMethod(job,"setAnnotationOverlay",Q_ARG(QImage,overlay));
        if(!click(QStringLiteral("nkshotClipExport"))||!QTest::qWaitFor([&]{return fileDialog(QStringLiteral("nkshotClipSaveDialog"));},1500))return fail(QStringLiteral("Annotation save race did not recover"));
    }
    if(privacyExport) {
        QMetaObject::invokeMethod(job,"setPrivacyRect",Q_ARG(int,expectedPrivacy.x()+2),Q_ARG(int,expectedPrivacy.y()),
            Q_ARG(int,expectedPrivacy.width()),Q_ARG(int,expectedPrivacy.height()));
        const auto stale=fixtures.filePath(QStringLiteral("stale-privacy.mp4"));
        if(!choose(fileDialog(QStringLiteral("nkshotClipSaveDialog")),stale)
            ||!QTest::qWaitFor([&]{return !viewer()->property("choosingClipOutput").toBool();},1500)
            ||QFileInfo::exists(stale)||job->property("busy").toBool())
            return fail(QStringLiteral("Changed privacy region leaked through stale Save dialog"));
        QMetaObject::invokeMethod(job,"setPrivacyRect",Q_ARG(int,expectedPrivacy.x()),Q_ARG(int,expectedPrivacy.y()),
            Q_ARG(int,expectedPrivacy.width()),Q_ARG(int,expectedPrivacy.height()));
        if(!click(QStringLiteral("nkshotClipExport"))||!QTest::qWaitFor([&]{return fileDialog(QStringLiteral("nkshotClipSaveDialog"));},1500))
            return fail(QStringLiteral("Privacy export did not recover after stale dialog"));
    }
    if(cropExport) {
        // An asynchronous dialog must not export a different crop than requested.
        if(qEnvironmentVariableIsSet("NKSHOT_TEST_REFRAME_CLIP"))
            QMetaObject::invokeMethod(job,"setCropPosition",Q_ARG(int,expectedCrop.x()+2),Q_ARG(int,expectedCrop.y()));
        else job->setProperty("cropAspect",QStringLiteral("square"));
        const auto stale=fixtures.filePath(QStringLiteral("stale-crop.mp4"));
        if(!choose(fileDialog(QStringLiteral("nkshotClipSaveDialog")),stale)
            ||!QTest::qWaitFor([&]{return !viewer()->property("choosingClipOutput").toBool();},1500))return fail(QStringLiteral("Crop-race dialog did not settle"));
        if(QFileInfo::exists(stale)||job->property("busy").toBool()||!job->property("outputUrl").toUrl().isEmpty())
            return fail(QStringLiteral("Changed crop was exported from an obsolete save dialog"));
        job->setProperty("cropAspect",QStringLiteral("portrait"));
        if(qEnvironmentVariableIsSet("NKSHOT_TEST_REFRAME_CLIP"))
            QMetaObject::invokeMethod(job,"setCropPosition",Q_ARG(int,expectedCrop.x()),Q_ARG(int,expectedCrop.y()));
        if(!click(QStringLiteral("nkshotClipExport"))||!QTest::qWaitFor([&]{return fileDialog(QStringLiteral("nkshotClipSaveDialog"));},1500))
            return fail(QStringLiteral("Crop export did not recover after stale dialog"));
    }
    if(resolutionExport) {
        job->setProperty("outputResolution",QStringLiteral("original"));
        const auto stale=fixtures.filePath(QStringLiteral("stale-resolution.mp4"));
        if(!choose(fileDialog(QStringLiteral("nkshotClipSaveDialog")),stale)
            ||!QTest::qWaitFor([&]{return !viewer()->property("choosingClipOutput").toBool();},1500)
            ||QFileInfo::exists(stale)||job->property("busy").toBool())return fail(QStringLiteral("Stale resolution Save accepted"));
        job->setProperty("outputResolution",QStringLiteral("720p"));
        if(!click(QStringLiteral("nkshotClipExport"))||!QTest::qWaitFor([&]{return fileDialog(QStringLiteral("nkshotClipSaveDialog"));},1500))return fail(QStringLiteral("Resolution save did not recover"));
    }
    const QSize expectedOutput=job->property("outputSize").toSize();
    const auto clip=fixtures.filePath(QStringLiteral("imported ' clip.mp4"));
    if(!choose(fileDialog(QStringLiteral("nkshotClipSaveDialog")),clip)
        ||!QTest::qWaitFor([&]{return !job->property("busy").toBool()&&job->property("outputUrl").toUrl()==QUrl::fromLocalFile(clip);},30000)
        ||bytes(clip).isEmpty()||bytes(movie)!=movieBytes||!sourcesIntact()||core->currentVideo()!=QUrl::fromLocalFile(movie))
        return fail(QStringLiteral("Real imported MP4 clip export/source isolation failed"));
    if(silentExport) {
        QProcess probe;probe.start(QStringLiteral("ffprobe"),{QStringLiteral("-v"),QStringLiteral("error"),
            QStringLiteral("-select_streams"),QStringLiteral("a"),QStringLiteral("-show_entries"),QStringLiteral("stream=index"),
            QStringLiteral("-of"),QStringLiteral("csv=p=0"),clip});
        if(!probe.waitForFinished(10000)||probe.exitCode()!=0||!probe.readAllStandardOutput().trimmed().isEmpty())
            return fail(QStringLiteral("Actual UI silent MP4 still has an audio track"));
    }
    if(cropExport||resolutionExport) {
        QProcess probe;probe.start(QStringLiteral("ffprobe"),{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-select_streams"),QStringLiteral("v:0"),
            QStringLiteral("-show_entries"),QStringLiteral("stream=width,height"),QStringLiteral("-of"),QStringLiteral("csv=p=0"),clip});
        if(!probe.waitForFinished(10000)||probe.exitCode()!=0||QString::fromUtf8(probe.readAllStandardOutput()).trimmed()!=QStringLiteral("%1,%2").arg(expectedOutput.width()).arg(expectedOutput.height()))
            return fail(QStringLiteral("Actual MP4 dimensions do not match output preview"));
    }
    if(privacyExport) {
        const auto decoded=fixtures.filePath(QStringLiteral("privacy-decoded.png"));
        QProcess decode;decode.start(QStringLiteral("ffmpeg"),{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-i"),clip,
            QStringLiteral("-frames:v"),QStringLiteral("1"),QStringLiteral("-threads"),QStringLiteral("1"),decoded});
        if(!decode.waitForFinished(10000)||decode.exitCode()!=0)return fail(QStringLiteral("Privacy output decode failed"));
        QImage image(decoded);auto protectedArea=expectedPrivacy;
        if(cropExport){protectedArea=protectedArea.intersected(expectedCrop);protectedArea.translate(-expectedCrop.topLeft());}
        if(image.isNull()||protectedArea.width()<8||protectedArea.height()<8)return fail(QStringLiteral("Privacy output region missing"));
        const auto color=image.pixelColor(protectedArea.center());
        if(color.red()>20||color.green()>20||color.blue()>20)return fail(QStringLiteral("Actual UI export did not bake opaque black privacy cover"));
    }
    if(annotationExport) {
        const auto overlay=job->property("annotationOverlay").value<QImage>();QPoint marked(-1,-1);
        for(int y=4;y<overlay.height()-4&&marked.x()<0;y+=2)for(int x=4;x<overlay.width()-4;x+=2){
            const QPoint p(x,y);if(qAlpha(overlay.pixel(p))>250&&(!cropExport||expectedCrop.contains(p))&&(!privacyExport||!expectedPrivacy.contains(p))){marked=p;break;}
        }
        const auto decoded=fixtures.filePath(QStringLiteral("annotations-decoded.png"));
        QProcess decode;decode.start(QStringLiteral("ffmpeg"),{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-i"),clip,QStringLiteral("-frames:v"),QStringLiteral("1"),QStringLiteral("-threads"),QStringLiteral("1"),decoded});
        if(marked.x()<0||!decode.waitForFinished(10000)||decode.exitCode()!=0)return fail(QStringLiteral("Annotated output decode/mark missing"));
        const QImage image(decoded);if(cropExport)marked-=expectedCrop.topLeft();
        if(image.isNull()||!image.rect().contains(marked))return fail(QStringLiteral("Annotated output geometry invalid"));
        const auto color=image.pixelColor(marked);if(color.red()<205||color.green()<205||color.blue()<205)return fail(QStringLiteral("Actual drawn white annotation absent from exported clip"));
    }
    const auto evidenceClip=screenshot+QStringLiteral(".export-")+QString::number(QDateTime::currentMSecsSinceEpoch())+QStringLiteral(".mp4");
    if(!QFile::copy(clip,evidenceClip))return fail(QStringLiteral("Unable to retain actual export evidence"));
    qInfo()<<"085 actual imported clip artifact"<<evidenceClip;
    if(webm) {
        if(!QTest::qWaitFor([&]{return !viewer()->property("choosingClipOutput").toBool();},1500)
            ||!click(QStringLiteral("nkshotClipFormat")))return fail(QStringLiteral("WebM GIF format choice unavailable"));
        QTest::keyClick(viewer(),Qt::Key_End);QTest::keyClick(viewer(),Qt::Key_Return);
        if(!QTest::qWaitFor([&]{return job->property("outputFormat").toString()==QStringLiteral("gif")
                &&job->property("ready").toBool()&&!job->property("busy").toBool();},15000)
            ||!editRange(QStringLiteral("nkshotClipEnd"),QStringLiteral("3.15"),315)
            ||!editRange(QStringLiteral("nkshotClipStart"),QStringLiteral("1.35"),135)
            ||!capture(QStringLiteral(".gif-range.png"))||!click(QStringLiteral("nkshotClipExport"))
            ||!QTest::qWaitFor([&]{return fileDialog(QStringLiteral("nkshotClipSaveDialog"));},1500))
            return fail(QStringLiteral("Actual WebM GIF combo/range/Save failed"));
        const auto gif=screenshot+QStringLiteral(".export-")+QString::number(QDateTime::currentMSecsSinceEpoch())+QStringLiteral(".gif");
        if(!choose(fileDialog(QStringLiteral("nkshotClipSaveDialog")),gif)
            ||!QTest::qWaitFor([&]{return !job->property("busy").toBool()&&job->property("outputUrl").toUrl()==QUrl::fromLocalFile(gif);},30000)
            ||bytes(gif).isEmpty()||bytes(movie)!=movieBytes||!sourcesIntact())
            return fail(QStringLiteral("Actual WebM GIF export/source preservation failed"));
        qInfo()<<"086 actual imported WebM GIF artifact"<<gif;
        if(!click(QStringLiteral("nkshotClipCancel")))return fail(QStringLiteral("GIF completion panel close failed"));
        // Load the real exported file in the production reusable player. This
        // does not import unsupported animation into the image document.
        recording->setProperty("sourceUrl",QUrl::fromLocalFile(gif));
        auto animated=recording->findChild<QObject *>(QStringLiteral("nkshotAnimatedImage"));
        if(!animated||!QTest::qWaitFor([&]{return recording->property("hasAnimatedImage").toBool()
                &&animated->property("frameCount").toInt()>1;},10000))
            return fail(QStringLiteral("Actual exported GIF did not decode in AnimatedImage"));
        if(!recording->property("playing").toBool())QMetaObject::invokeMethod(recording,"togglePlay");
        const auto frame=animated->property("currentFrame").toInt();
        if(!QTest::qWaitFor([&]{return animated->property("currentFrame").toInt()!=frame;},2000)
            ||!capture(QStringLiteral(".gif-playback.png")))return fail(QStringLiteral("Actual exported GIF did not animate"));
        QMetaObject::invokeMethod(recording,"togglePlay");
        if(recording->property("playing").toBool())return fail(QStringLiteral("Exported GIF pause failed"));
        recording->setProperty("sourceUrl",QUrl::fromLocalFile(movie));
        if(!QTest::qWaitFor([&]{return player->property("hasVideo").toBool();},10000)
            ||core->currentVideo()!=QUrl::fromLocalFile(movie)||bytes(movie)!=movieBytes)
            return fail(QStringLiteral("Original WebM playback restore failed"));
    }
    if(silentExport) {
        // Closing/reopening starts a fresh export with audio included by default.
        auto popup=recording->findChild<QObject *>(QStringLiteral("nkshotClipPopup"));
        if(popup&&popup->property("opened").toBool()&&!click(QStringLiteral("nkshotClipCancel")))
            return fail(QStringLiteral("Silent export panel close failed"));
        QTest::mouseMove(viewer(),recording->mapToScene(QPointF(recording->width()/2,recording->height()-35)).toPoint());
        if(!click(QStringLiteral("nkshotPlaybackSettings"))||!click(QStringLiteral("nkshotOpenClip"))
            ||!QTest::qWaitFor([&]{return job->property("ready").toBool()&&!job->property("busy").toBool();},15000)
            ||!job->property("includeAudio").toBool()||!click(QStringLiteral("nkshotClipCancel")))
            return fail(QStringLiteral("New export retained an old silent choice"));
    }
    if(cropExport) {
        auto popup=recording->findChild<QObject *>(QStringLiteral("nkshotClipPopup"));
        if(popup&&popup->property("opened").toBool()&&!click(QStringLiteral("nkshotClipCancel")))return fail(QStringLiteral("Crop export close failed"));
        QTest::mouseMove(viewer(),recording->mapToScene(QPointF(recording->width()/2,recording->height()-35)).toPoint());
        if(!click(QStringLiteral("nkshotPlaybackSettings"))||!click(QStringLiteral("nkshotOpenClip"))
            ||!QTest::qWaitFor([&]{return job->property("ready").toBool()&&!job->property("busy").toBool();},15000)
            ||job->property("cropAspect").toString()!=QStringLiteral("original")||!click(QStringLiteral("nkshotClipCancel")))
            return fail(QStringLiteral("New clip session inherited an old crop"));
    }
    if(qEnvironmentVariableIsSet("NKSHOT_TEST_CLIPBOARD_EDITOR")) {
        auto popup=recording->findChild<QObject *>(QStringLiteral("nkshotClipPopup"));
        if(popup&&popup->property("opened").toBool()&&!click(QStringLiteral("nkshotClipCancel")))
            return fail(QStringLiteral("Clipboard test could not dismiss clip panel"));
        const auto prior=core->annotationDocument()->renderToImage();
        const auto depth=core->annotationDocument()->undoStackDepth();
        auto clip=application.clipboard();
        clip->setText(QStringLiteral("plain text is not an image"));
        if(viewer()->editClipboardImage()||clip->text()!=QStringLiteral("plain text is not an image"))
            return fail(QStringLiteral("Text clipboard was imported or changed"));
        auto urls=new QMimeData;urls->setUrls({QUrl(QStringLiteral("https://example.invalid/not-an-image.png"))});clip->setMimeData(urls);
        if(viewer()->editClipboardImage()||clip->mimeData()->urls().size()!=1)
            return fail(QStringLiteral("Clipboard URL was followed or replaced"));
        QImage wide(8193,1,QImage::Format_ARGB32);wide.fill(Qt::transparent);clip->setImage(wide);
        if(viewer()->editClipboardImage())return fail(QStringLiteral("Overwide clipboard image accepted"));
        QImage sample(640,400,QImage::Format_ARGB32_Premultiplied);sample.fill(QColor("#f1f0fb"));
        {QPainter p(&sample);p.setRenderHint(QPainter::Antialiasing);p.setPen(Qt::NoPen);p.setBrush(QColor("#dedcf7"));p.drawEllipse(QRectF(440,-70,280,280));
            p.setPen(QColor("#6555cc"));QFont f=p.font();f.setPixelSize(17);f.setBold(true);p.setFont(f);p.drawText(48,68,QStringLiteral("A MOMENT WORTH KEEPING"));
            f.setPixelSize(38);p.setFont(f);p.setPen(QColor("#252237"));p.drawText(48,150,QStringLiteral("Make your point."));
            f.setPixelSize(20);f.setBold(false);p.setFont(f);p.setPen(QColor("#686277"));p.drawText(48,201,QStringLiteral("Annotate a copy. Keep the original."));
            p.setBrush(QColor("#6555cc"));p.setPen(Qt::NoPen);p.drawRoundedRect(QRectF(48,264,235,66),16,16);
            p.setPen(Qt::white);p.drawText(QRectF(48,264,235,66),Qt::AlignCenter,QStringLiteral("Local test image"));}
        sample.setColorSpace(QColorSpace::SRgb);sample.setDevicePixelRatio(1);
        clip->setImage(sample);InlineMessageModel::instance()->clear();
        if(!click(QStringLiteral("nkshotViewerMore")))return fail(QStringLiteral("More entry unavailable for clipboard edit"));
        QTest::qWait(120);
        const auto clipboardEntry=visual(viewer()->contentItem(),QStringLiteral("nkshotClipboardImageAction"));
        if(!clipboardEntry||clipboardEntry->mapToScene(QPointF()).x()<viewer()->width()-270)
            return fail(QStringLiteral("More menu detached from its upper-right trigger"));
        if(!viewer()->grabWindow().save(screenshot+QStringLiteral(".clipboard-menu.png"))||!click(QStringLiteral("nkshotClipboardImageAction")))
            return fail(QStringLiteral("Actual clipboard menu entry unavailable"));
        QPointer<ImageCopyEditor> first;
        if(!QTest::qWaitFor([&]{for(auto w:QGuiApplication::allWindows())if(auto e=qobject_cast<ImageCopyEditor *>(w))first=e;return first&&first->isVisible();},1500)
            ||!samePixels(first->document()->renderToImage(),sample)||!samePixels(clip->image(),sample))
            return fail(QStringLiteral("Clipboard snapshot failed or clipboard changed"));
        first->resize(qEnvironmentVariableIsSet("NKSHOT_TEST_COMPACT")?QSize(420,520):QSize(900,640));
        auto clipboardCanvas=visual(first->contentItem(),QStringLiteral("nkshotAnnotationViewport"));
        QList<QPoint> ink;
        for(int y=0;y<sample.height();++y)for(int x=0;x<sample.width();++x){
            const auto c=sample.pixelColor(x,y);if(c.red()<70&&c.green()<70&&c.blue()<90)ink.append({x,y});
        }
        QImage clipboardShot;
        const auto clipboardPainted=[&]{
            if(!clipboardCanvas||ink.size()<12||first->rootObject()->width()!=first->width()
                ||qAbs(clipboardCanvas->mapToScene(QPointF(clipboardCanvas->width()/2,clipboardCanvas->height()/2)).x()-first->width()/2.)>2)return false;
            first->update();clipboardShot=first->grabWindow();int matched=0;
            for(int i=0;i<12;++i){
                const auto p=clipboardCanvas->mapToScene(ink.at((ink.size()-1)*i/11)).toPoint();bool found=false;
                for(int y=p.y()-1;y<=p.y()+1;++y)for(int x=p.x()-1;x<=p.x()+1;++x){
                    if(!clipboardShot.rect().contains(x,y))continue;
                    const auto c=clipboardShot.pixelColor(x,y);
                    if(c.red()<100&&c.green()<100&&c.blue()<120)found=true;
                }
                matched+=found;
            }
            return matched>=10;
        };
        if(!QTest::qWaitFor(clipboardPainted,2000)||!clipboardShot.save(screenshot+QStringLiteral(".clipboard-editor.png")))
            return fail(QStringLiteral("Clipboard image content was not visibly painted"));
        QSignalSpy clipboardSaved(first,&ImageCopyEditor::saveFinished);
        first->saveAs();
        if(!QTest::qWaitFor([&]{return fileDialog(QStringLiteral("nkshotCopySaveDialog"));},1500))
            return fail(QStringLiteral("Clipboard copy Save dialog unavailable"));
        const auto clipboardPng=screenshot+QStringLiteral(".clipboard-%1.png").arg(QDateTime::currentMSecsSinceEpoch());
        QTest::qWait(200);
        if(!choose(fileDialog(QStringLiteral("nkshotCopySaveDialog")),clipboardPng)
            ||!QTest::qWaitFor([&]{return !clipboardSaved.isEmpty();},1500)||!clipboardSaved.last().at(1).toBool()
            ||!samePixels(QImage(clipboardPng),sample)||!samePixels(clip->image(),sample))
            return fail(QStringLiteral("Clipboard PNG save changed pixels or clipboard"));
        QImage replacement(80,60,QImage::Format_ARGB32_Premultiplied);replacement.fill(QColor(10,70,120,90));clip->setImage(replacement);
        if(!samePixels(first->document()->renderToImage(),sample))return fail(QStringLiteral("Later clipboard mutation changed open editor"));
        viewer()->requestActivate();
        if(!QTest::qWaitFor([&]{return QGuiApplication::focusWindow()==viewer();},1000))return fail(QStringLiteral("Clipboard shortcut focus unavailable"));
        QTest::keyClick(viewer(),Qt::Key_V,Qt::ControlModifier|Qt::ShiftModifier);
        QPointer<ImageCopyEditor> second;
        if(!QTest::qWaitFor([&]{for(auto w:QGuiApplication::allWindows())if(auto e=qobject_cast<ImageCopyEditor *>(w);e&&e!=first)second=e;return second&&second->isVisible();},1500)
            ||!samePixels(second->document()->renderToImage(),replacement)||second->document()==first->document())
            return fail(QStringLiteral("Clipboard shortcut did not create independent second snapshot"));
        first->close();second->close();
        if(!QTest::qWaitFor([&]{return first.isNull()&&second.isNull();},1000)
            ||core->annotationDocument()->renderToImage()!=prior||core->annotationDocument()->undoStackDepth()!=depth
            ||core->currentVideo()!=QUrl::fromLocalFile(movie)||bytes(movie)!=movieBytes)
            return fail(QStringLiteral("Clipboard workflow changed active media/history"));
        clip->setText(clipboard);
    }
    if(privacyExport||resolutionExport) {
        auto popup=recording->findChild<QObject *>(QStringLiteral("nkshotClipPopup"));
        if(popup&&popup->property("opened").toBool()&&!click(QStringLiteral("nkshotClipCancel")))return fail(QStringLiteral("Privacy panel close failed"));
        QTest::mouseMove(viewer(),recording->mapToScene(QPointF(recording->width()/2,recording->height()-35)).toPoint());
        if(!click(QStringLiteral("nkshotPlaybackSettings"))||!click(QStringLiteral("nkshotOpenClip"))
            ||!QTest::qWaitFor([&]{return job->property("ready").toBool()&&!job->property("busy").toBool();},15000)
            ||job->property("privacyEnabled").toBool()||job->property("outputResolution").toString()!=QStringLiteral("original")||!click(QStringLiteral("nkshotClipCancel")))
            return fail(QStringLiteral("New export retained an old privacy cover"));
        if(privacyExport)qInfo()<<"097 actual privacy toggle, drag, corner resize, keyboard, save-race, decoded output and fresh-session reset passed";
        if(resolutionExport)qInfo()<<"102 actual resolution control, output dimensions, stale Save rejection and fresh-session reset passed";
    }
    if(qEnvironmentVariableIsSet("NKSHOT_TEST_RECENT_MEDIA")) {
        auto popup=recording->findChild<QObject *>(QStringLiteral("nkshotClipPopup"));
        if(popup&&popup->property("opened").toBool()&&!click(QStringLiteral("nkshotClipCancel")))return fail(QStringLiteral("Recent test could not dismiss clip panel"));
        auto history=NkShot::RecentMediaModel::instance();
        const auto findRecent=[&](const QString &path) {
            const auto canonical=QUrl::fromLocalFile(QFileInfo(path).canonicalFilePath());
            for(int i=0;i<history->count();++i)if(history->data(history->index(i),NkShot::RecentMediaModel::UrlRole).toUrl()==canonical)return i;
            return -1;
        };
        if(findRecent(saved)<0||findRecent(movie)<0||findRecent(clip)!=0)
            return fail(QStringLiteral("Successful image/video open/save/clip hooks did not populate recent files"));
        const auto mainImage=core->annotationDocument()->renderToImage();
        const auto mainDepth=core->annotationDocument()->undoStackDepth();
        const auto openHistory=[&] {
            viewer()->requestActivate();QTest::qWait(80);
            return click(QStringLiteral("nkshotViewerMore"))&&click(QStringLiteral("nkshotRecentMediaAction"))
                &&QTest::qWaitFor([&]{return visual(viewer()->contentItem(),QStringLiteral("nkshotRecentFiles"));},1000);
        };
        if(!openHistory()||!capture(QStringLiteral(".recent.png"))||!click(QStringLiteral("nkshotRecentOpen_0"))
            ||!QTest::qWaitFor([&]{return replaceDialog();},15000)||!confirm(QMessageBox::Cancel)||!settled()
            ||core->currentVideo()!=QUrl::fromLocalFile(movie)||core->annotationDocument()->renderToImage()!=mainImage
            ||core->annotationDocument()->undoStackDepth()!=mainDepth||findRecent(clip)!=0)
            return fail(QStringLiteral("Recent replacement cancel altered media/history/order"));
        if(!openHistory()||!click(QStringLiteral("nkshotRecentOpen_0"))
            ||!QTest::qWaitFor([&]{return replaceDialog();},15000)||!confirm(QMessageBox::Discard)||!settled()
            ||core->currentVideo()!=QUrl::fromLocalFile(clip)||findRecent(clip)!=0)
            return fail(QStringLiteral("Actual recent-list click did not reopen exported clip"));
        viewer()->openRecentMedia(QUrl::fromLocalFile(saved));
        if(!QTest::qWaitFor([&]{return replaceDialog();},10000)||!confirm(QMessageBox::Discard)||!settled()
            ||core->videoMode()||!samePixels(core->annotationDocument()->baseImage(),QImage(saved))||findRecent(saved)!=0)
            return fail(QStringLiteral("Recent image route did not share validated Open workflow"));
        const auto savedBytes=bytes(saved);const auto beforeRemove=history->count();
        if(!openHistory()||!click(QStringLiteral("nkshotRecentRemove_0"))||history->count()!=beforeRemove-1
            ||bytes(saved)!=savedBytes||!click(QStringLiteral("nkshotRecentClose")))
            return fail(QStringLiteral("Removing a recent row changed the media file"));
        viewer()->openRecentMedia(QUrl::fromLocalFile(movie));
        if(!QTest::qWaitFor([&]{return replaceDialog();},15000)||!confirm(QMessageBox::Discard)||!settled()
            ||core->currentVideo()!=QUrl::fromLocalFile(movie))return fail(QStringLiteral("Recent recording restore failed"));
        const auto clipIndex=findRecent(clip);const auto moved=clip+QStringLiteral(".moved");const auto clipBytes=bytes(clip);
        if(clipIndex<0||clipIndex>2||!QFile::rename(clip,moved)||!openHistory())return fail(QStringLiteral("Missing-file fixture/history unavailable"));
        auto unavailable=visual(viewer()->contentItem(),QStringLiteral("nkshotRecentOpen_%1").arg(clipIndex));
        if(!unavailable||unavailable->isEnabled()||!click(QStringLiteral("nkshotRecentRemove_%1").arg(clipIndex))||bytes(moved)!=clipBytes
            ||!QFile::rename(moved,clip)||!click(QStringLiteral("nkshotRecentClear"))||history->count()!=0
            ||bytes(saved)!=savedBytes||bytes(movie)!=movieBytes||bytes(clip)!=clipBytes||!sourcesIntact()
            ||!capture(QStringLiteral(".recent-empty.png"))||!click(QStringLiteral("nkshotRecentClose")))
            return fail(QStringLiteral("Missing-row/Clear-list workflow deleted or changed actual media"));
    }
    if(!capture(QString()))return fail(QStringLiteral("Final imported media screenshot failed"));
    qInfo()<<"085 actual dialogs: PNG/JPEG/static WebP, edit replacement cancel/failure, new-copy Save, missing ffprobe recovery, real MP4 Qt playback and clip export passed";
    return true;
}
