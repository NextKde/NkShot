// SPDX-License-Identifier: GPL-3.0-only
// Actual local-file dialogs and decoders, using generated nonpersonal fixtures.
#include "LocalMediaOpenUiChecks.h"
#include "SpectacleCore.h"
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
        // Verify an actual edited canvas pixel in the window, not only the model.
        QPoint strokePixel(-1,-1);
        for(int y=0;y<edited.height()&&strokePixel.x()<0;++y)for(int x=0;x<edited.width();++x) {
            const auto c=edited.pixelColor(x,y),o=extracted.pixelColor(x,y);
            if(qAbs(c.red()-236)<8&&qAbs(c.green()-57)<8&&qAbs(c.blue()-116)<8
                &&qAbs(c.red()-o.red())+qAbs(c.green()-o.green())+qAbs(c.blue()-o.blue())>160) {strokePixel={x,y};break;}
        }
        if(strokePixel.x()<0)return fail(QStringLiteral("No strong edited stroke pixel found"));
        QImage editorShot;
        const auto painted=[&] {
            editor->update();editorShot=editor->grabWindow();
            const auto location=viewport->mapToScene(strokePixel).toPoint();
            for(int y=location.y()-2;y<=location.y()+2;++y)for(int x=location.x()-2;x<=location.x()+2;++x) {
                if(!editorShot.rect().contains(x,y))continue;
                const auto c=editorShot.pixelColor(x,y);
                if(qAbs(c.red()-236)<30&&qAbs(c.green()-57)<30&&qAbs(c.blue()-116)<30)return true;
            }
            return false;
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
    const bool silentExport=qEnvironmentVariableIsSet("NKSHOT_TEST_SILENT_CLIP");
    if(!click(QStringLiteral("nkshotClipDetails"))||!capture(QStringLiteral(".details.png")))
        return fail(QStringLiteral("Export details could not be opened"));
    if(silentExport) {
        if(!job->property("includeAudio").toBool()||!click(QStringLiteral("nkshotClipIncludeAudio"))
            ||job->property("includeAudio").toBool()||!capture(QStringLiteral(".silent-options.png")))
            return fail(QStringLiteral("Silent export option did not update actual job"));
    }
    auto cancelControl=visual(viewer()->contentItem(),QStringLiteral("nkshotClipCancel"));
    auto previewControl=visual(viewer()->contentItem(),QStringLiteral("nkshotSoftwareVideoPreview"));
    if(!cancelControl||!previewControl||previewControl->height()<50
        ||cancelControl->mapToScene(QPointF(0,cancelControl->height())).y()>viewer()->height()
        ||!click(QStringLiteral("nkshotClipDetails")))return fail(QStringLiteral("Details hides preview or cancel in the real window"));
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
    if(silentExport) {
        QProcess probe;probe.start(QStringLiteral("ffprobe"),{QStringLiteral("-v"),QStringLiteral("error"),
            QStringLiteral("-select_streams"),QStringLiteral("a"),QStringLiteral("-show_entries"),QStringLiteral("stream=index"),
            QStringLiteral("-of"),QStringLiteral("csv=p=0"),clip});
        if(!probe.waitForFinished(10000)||probe.exitCode()!=0||!probe.readAllStandardOutput().trimmed().isEmpty())
            return fail(QStringLiteral("Actual UI silent MP4 still has an audio track"));
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
    if(!capture(QString()))return fail(QStringLiteral("Final imported media screenshot failed"));
    qInfo()<<"085 actual dialogs: PNG/JPEG/static WebP, edit replacement cancel/failure, new-copy Save, missing ffprobe recovery, real MP4 Qt playback and clip export passed";
    return true;
}
