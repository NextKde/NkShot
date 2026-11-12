// SPDX-License-Identifier: GPL-3.0-only
// Independent sequential FFmpeg decode is the oracle, never QMediaPlayer seek.
#include "FrameInspectionUiChecks.h"
#include "Gui/FrameInspectionWindow.h"
#include "Media/FrameInspectionSession.h"
#include "Gui/ViewerWindow.h"
#include "Gui/PinnedImageWindow.h"
#include "SpectacleCore.h"
#include <KQuickImageEditor/AnnotationDocument>
#include <QApplication>
#include <QClipboard>
#include <QCryptographicHash>
#include <QFile>
#include <QFileDialog>
#include <QLineEdit>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QPointer>
#include <QQuickItem>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QTest>
#include <cstring>
namespace {
QQuickItem *find(QQuickItem *root,const QString &name){
    if(!root)return nullptr;
    if(root->objectName()==name)return root;
    for(auto child:root->childItems())if(auto item=find(child,name))return item;
    return nullptr;
}
bool click(QQuickWindow *window,const char *name){
    QPointer<QQuickItem> item;
    if(!QTest::qWaitFor([&]{item=find(window->contentItem(),QString::fromLatin1(name));return item&&item->isVisible()&&item->isEnabled()&&item->width()>0;},1500))return false;
    const auto point=item->mapToScene(QPointF(item->width()/2,item->height()/2));
    if(!QRectF(QPointF(),window->size()).contains(point))return false;
    window->requestActivate();QTest::qWait(30);
    QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,point.toPoint());return true;
}
QByteArray execute(const QString &program,const QStringList &args,bool *ok=nullptr){
    QProcess process;process.start(program,args);
    const bool success=process.waitForFinished(30000)&&process.exitStatus()==QProcess::NormalExit&&process.exitCode()==0;
    if(ok)*ok=success;
    if(!success)qWarning()<<"Frame oracle process failed"<<program<<process.readAllStandardError();
    return success?process.readAllStandardOutput():QByteArray();
}
QByteArray hash(const QString &path){QFile file(path);if(!file.open(QIODevice::ReadOnly))return {};QCryptographicHash digest(QCryptographicHash::Sha256);return digest.addData(&file)?digest.result():QByteArray();}
struct Oracle {QSize size;QList<double> ms;QList<QImage> frames;bool hasB=false;};
Oracle decode(const QString &path){
    Oracle result;bool ok=false;
    const auto json=execute(QStringLiteral("ffprobe"),{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-select_streams"),QStringLiteral("v:0"),QStringLiteral("-show_frames"),QStringLiteral("-show_streams"),QStringLiteral("-show_entries"),QStringLiteral("frame=best_effort_timestamp_time,pict_type:stream=width,height"),QStringLiteral("-of"),QStringLiteral("json"),path},&ok);
    if(!ok)return {};
    const auto object=QJsonDocument::fromJson(json).object();const auto streams=object.value(QStringLiteral("streams")).toArray();
    if(streams.size()!=1)return {};
    result.size={streams.first().toObject().value(QStringLiteral("width")).toInt(),streams.first().toObject().value(QStringLiteral("height")).toInt()};
    const auto frames=object.value(QStringLiteral("frames")).toArray();double first=0,previous=-1;
    for(const auto &value:frames){const auto frame=value.toObject();bool valid=false;const double timestamp=frame.value(QStringLiteral("best_effort_timestamp_time")).toString().toDouble(&valid)*1000;
        if(!valid||(!result.ms.isEmpty()&&timestamp<=previous))return {};
        if(result.ms.isEmpty())first=timestamp;
        previous=timestamp;result.ms.append(timestamp-first);result.hasB|=frame.value(QStringLiteral("pict_type")).toString()==QStringLiteral("B");
    }
    const qint64 stride=qint64(result.size.width())*3,total=stride*result.size.height()*result.ms.size();
    if(result.size.isEmpty()||result.ms.isEmpty()||total>128*1024*1024)return {};
    // Full sequential decode from file start; no seek, select, PTS filter or scale.
    const auto raw=execute(QStringLiteral("ffmpeg"),{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-noautorotate"),QStringLiteral("-i"),path,QStringLiteral("-map"),QStringLiteral("0:v:0"),QStringLiteral("-an"),QStringLiteral("-fps_mode"),QStringLiteral("passthrough"),QStringLiteral("-threads"),QStringLiteral("2"),QStringLiteral("-pix_fmt"),QStringLiteral("rgb24"),QStringLiteral("-f"),QStringLiteral("rawvideo"),QStringLiteral("pipe:1")},&ok);
    if(!ok||raw.size()!=total)return {};
    for(int index=0;index<result.ms.size();++index)result.frames.append(QImage(reinterpret_cast<const uchar *>(raw.constData()+index*stride*result.size.height()),result.size.width(),result.size.height(),stride,QImage::Format_RGB888).copy());
    return result;
}
bool equal(const QImage &left,const QImage &right){
    if(left.isNull()||right.isNull()||left.size()!=right.size())return false;
    const auto a=left.convertToFormat(QImage::Format_RGB888),b=right.convertToFormat(QImage::Format_RGB888);
    for(int y=0;y<a.height();++y)if(std::memcmp(a.constScanLine(y),b.constScanLine(y),a.width()*3))return false;
    return true;
}
FrameInspectionWindow *inspector(){for(auto window:QGuiApplication::allWindows())if(auto view=qobject_cast<FrameInspectionWindow *>(window))return view;return nullptr;}
}
bool FrameInspectionUiChecks::run(QApplication &application,ViewerWindow *owner,QObject *recording,const QString &source,const QString &evidence,const std::function<void(const QUrl &)> &setTestSource,QString *failure){
    const auto fail=[&](const QString &text){if(failure)*failure=text;if(inspector())inspector()->grabWindow().save(evidence+QStringLiteral(".inspect-failed.png"));return false;};
    auto core=SpectacleCore::instance();auto document=core->annotationDocument();
    const auto image=document->renderToImage();const auto base=document->baseImage();const auto history=document->undoStackDepth();
    const auto sourceUrl=core->currentVideo();const auto sourceHash=hash(source);const auto clipboard=application.clipboard()->image();const auto clipboardText=application.clipboard()->text();
    const auto restoreClipboard=qScopeGuard([&]{if(!clipboard.isNull())application.clipboard()->setImage(clipboard);else application.clipboard()->setText(clipboardText);});
    auto player=recording?recording->findChild<QObject *>(QStringLiteral("nkshotMediaPlayer")):nullptr;
    if(!player||sourceHash.isEmpty())return fail(QStringLiteral("Inspection requires loaded synthetic local video"));
    auto recordingItem=qobject_cast<QQuickItem *>(recording);
    QTest::mouseMove(owner,recordingItem->mapToScene(QPointF(recordingItem->width()/2,recordingItem->height()-35)).toPoint());
    if(recording->property("playing").toBool()&&!click(owner,"nkshotPlaybackToggle"))return fail(QStringLiteral("Unable to pause source before independent inspection"));
    if(!QTest::qWaitFor([&]{return !player->property("playing").toBool();},1000))return fail(QStringLiteral("Source playback did not pause"));
    QTest::qWait(120);
    const auto sourcePosition=player->property("position").toLongLong();
    auto clip=owner->property("clipJob").value<QObject *>();QVariantMap clipState;
    const QStringList clipKeys={QStringLiteral("sourceUrl"),QStringLiteral("outputFormat"),QStringLiteral("includeAudio"),QStringLiteral("cropRect"),QStringLiteral("cropAspect"),QStringLiteral("privacyRect"),QStringLiteral("privacyEnabled"),QStringLiteral("outputResolution"),QStringLiteral("exportSpeed")};
    for(const auto &key:clipKeys)if(clip)clipState.insert(key,clip->property(key.toLatin1().constData()));
    auto clipPopup=recording->findChild<QObject *>(QStringLiteral("nkshotClipPopup"));
    if(!clip||!clipPopup||!clipPopup->property("startMs").isValid()||!clipPopup->property("endMs").isValid())return fail(QStringLiteral("Actual clip range state unavailable"));
    const auto clipStart=clipPopup->property("startMs"),clipEnd=clipPopup->property("endMs");
    const auto unchanged=[&]{
        if(clipPopup->property("startMs")!=clipStart||clipPopup->property("endMs")!=clipEnd)return false;
        if(core->currentVideo()!=sourceUrl||hash(source)!=sourceHash||document->renderToImage()!=image||document->baseImage()!=base||document->undoStackDepth()!=history||player->property("position").toLongLong()!=sourcePosition)return false;
        for(const auto &key:clipKeys)if(clip&&clip->property(key.toLatin1().constData())!=clipState.value(key))return false;
        return true;
    };
    const auto openActual=[&]{
        QTest::mouseMove(owner,recordingItem->mapToScene(QPointF(recordingItem->width()/2,recordingItem->height()-35)).toPoint());
        return click(owner,"nkshotPlaybackSettings")&&click(owner,"nkshotInspectVideoFrames")&&QTest::qWaitFor([&]{return inspector()!=nullptr;},1500);
    };
    const auto oracle=decode(source);if(oracle.frames.isEmpty())return fail(QStringLiteral("Independent sequential source decode failed"));
    if(!openActual())return fail(QStringLiteral("Actual Browse exact frames menu unavailable"));
    QPointer<FrameInspectionWindow> window=inspector();
    auto model=qobject_cast<FrameInspectionSession *>(window->sessionObject());
    const auto verify=[&](FrameInspectionWindow *view,FrameInspectionSession *session,const Oracle &reference,int index){
        if(!QTest::qWaitFor([&]{return !session->busy();},15000)||!session->ready()||session->currentIndex()!=index||session->requestedIndex()!=index||session->frameCount()!=reference.frames.size()||!equal(session->snapshot(),reference.frames.value(index))||qAbs(session->positionMs()-qRound64(reference.ms.value(index)))>1){qWarning()<<"Exact frame mismatch"<<index<<session->currentIndex()<<session->error();return false;}
        if(!QTest::qWaitFor([&]{
            auto preview=find(view->contentItem(),QStringLiteral("nkshotFrameInspectionPreview"));
            return preview&&preview->property("source").toUrl()==session->previewUrl()
                &&preview->property("status").toInt()==1&&preview->isVisible();
        },3000)) {
            qWarning()<<"Exact frame QML preview not ready"<<index<<session->previewUrl();return false;
        }
        QTest::qWait(80); // Allow the ready image to reach the rendered window.
        auto label=find(view->contentItem(),QStringLiteral("nkshotFrameInspectionIndex"));
        auto time=find(view->contentItem(),QStringLiteral("nkshotFrameInspectionTime"));
        return label&&time&&label->property("text").toString()==QStringLiteral("Frame %1 of %2").arg(index+1).arg(reference.frames.size())&&time->property("text").toString().contains(QString::number(session->positionMs()/1000.,'f',3));
    };
    int initial=0;for(int i=1;i<oracle.ms.size()&&oracle.ms[i]<=sourcePosition;++i)initial=i;
    if(!model||!verify(window,model,oracle,initial))return fail(QStringLiteral("Initial source-position mapping or exact pixels failed"));
    window->resize(qEnvironmentVariableIsSet("NKSHOT_TEST_COMPACT")?QSize(420,520):QSize(900,620));QTest::qWait(120);
    if(owner->inspectVideoFrames(sourceUrl,sourcePosition)!=window||!unchanged())return fail(QStringLiteral("Inspector duplicate/open altered original timeline"));
    const auto navigate=[&](FrameInspectionWindow *view,FrameInspectionSession *session,const Oracle &reference){
        view->requestActivate();QTest::qWait(50);
        for(int count=0;count<reference.frames.size()&&session->requestedIndex()>0;++count)QTest::keyClick(view,Qt::Key_Left);
        if(!verify(view,session,reference,0))return false;
        auto previous=find(view->contentItem(),QStringLiteral("nkshotFrameInspectionPrevious"));
        if(!previous||previous->isEnabled()||!click(view,"nkshotFrameInspectionNext")||!verify(view,session,reference,1))return false;
        if(!click(view,"nkshotFrameInspectionPrevious")||!verify(view,session,reference,0))return false;
        // Deliver rapid actual shortcuts without waiting for any seek/decode.
        const int target=qMin(5,int(reference.frames.size())-1);
        for(int i=0;i<target;++i)QTest::keyClick(view,Qt::Key_Right);
        auto extract=find(view->contentItem(),QStringLiteral("nkshotFrameInspectionExtract"));
        if(session->busy()&&(session->currentIndex()!=-1||!session->snapshot().isNull()||!extract||extract->isEnabled()||view->extractCopy()))return false;
        if(!verify(view,session,reference,target))return false;
        QTest::keyClick(view,Qt::Key_Left);if(!verify(view,session,reference,target-1))return false;
        for(int count=0;count<reference.frames.size()&&session->requestedIndex()<reference.frames.size()-1;++count)QTest::keyClick(view,Qt::Key_Right);
        if(!verify(view,session,reference,reference.frames.size()-1))return false;
        auto next=find(view->contentItem(),QStringLiteral("nkshotFrameInspectionNext"));
        return next&&!next->isEnabled();
    };
    if(!navigate(window,model,oracle)||!unchanged()||!window->grabWindow().save(evidence+QStringLiteral(".exact-frame.png")))return fail(QStringLiteral("Actual exact-frame navigation or timeline isolation failed"));
    const auto expected=model->snapshot();
    if(!click(window,"nkshotFrameInspectionExtract"))return fail(QStringLiteral("Extract selected exact frame unavailable"));
    QPointer<PinnedImageWindow> pin;
    if(!QTest::qWaitFor([&]{for(auto candidate:QGuiApplication::allWindows())if(candidate->objectName()==QStringLiteral("nkshotInspectedFramePin"))pin=qobject_cast<PinnedImageWindow *>(candidate);return pin&&pin->isVisible();},1500)||!equal(pin->snapshot(),expected)||!window)return fail(QStringLiteral("Extract did not make exact independent pin"));
    if(!click(pin,"nkshotPinCopy")||!equal(application.clipboard()->image(),expected)||!unchanged())return fail(QStringLiteral("Actual inspected frame Copy differs"));
    QTemporaryDir temporary;if(!temporary.isValid())return fail(QStringLiteral("Frame fixture directory unavailable"));
    if(!click(pin,"nkshotPinSave"))return fail(QStringLiteral("Inspected frame Save button unavailable"));
    QPointer<QFileDialog> save;
    if(!QTest::qWaitFor([&]{for(auto widget:QApplication::topLevelWidgets())if(widget->objectName()==QStringLiteral("nkshotPinSaveDialog"))save=qobject_cast<QFileDialog *>(widget);return save&&save->isVisible();},1500))return fail(QStringLiteral("Frame PNG Save dialog absent"));
    const auto png=temporary.filePath(QStringLiteral("exact-frame.png"));auto filename=save->findChild<QLineEdit *>(QStringLiteral("fileNameEdit"));
    if(!filename)return fail(QStringLiteral("Frame Save filename input absent"));
    QTest::qWait(200);filename->setFocus();filename->selectAll();QTest::keyClicks(filename,png);QMetaObject::invokeMethod(save,"accept");
    if(!QTest::qWaitFor([&]{return QFile::exists(png)&&(!save||!save->isVisible());},3000)||!equal(QImage(png),expected)||!unchanged())return fail(QStringLiteral("Saved inspected PNG differs or modified source"));
    pin->close();if(!QTest::qWaitFor([&]{return pin.isNull();},1000)||!click(window,"nkshotFrameInspectionClose")||!QTest::qWaitFor([&]{return window.isNull();},1000))return fail(QStringLiteral("Exact frame windows did not close"));
    if(!openActual())return fail(QStringLiteral("Inspector did not reopen"));
    window=inspector();owner->hide();
    if(!QTest::qWaitFor([&]{return window.isNull();},1000))return fail(QStringLiteral("Hidden owner retained frame inspector"));
    owner->show();owner->requestActivate();QTest::qWait(100);
    if(!openActual())return fail(QStringLiteral("Inspector unavailable after owner restored"));
    window=inspector();
    setTestSource(QUrl());
    if(!QTest::qWaitFor([&]{return window.isNull();},1000))return fail(QStringLiteral("Retired source retained frame inspector"));
    setTestSource(sourceUrl);
    // The explicit source-change fixture above necessarily reloads Qt playback;
    // original playback position isolation was checked before that lifecycle test.
    const auto cfr=temporary.filePath(QStringLiteral("b-frames.mp4")),vfr=temporary.filePath(QStringLiteral("irregular-vfr.mp4"));bool generated=false;
    execute(QStringLiteral("ffmpeg"),{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-y"),QStringLiteral("-f"),QStringLiteral("lavfi"),QStringLiteral("-i"),QStringLiteral("nullsrc=s=192x128:r=12:d=2,geq=lum='16+mod(N*7+X+Y,200)':cb=128:cr=128"),QStringLiteral("-c:v"),QStringLiteral("libx264"),QStringLiteral("-g"),QStringLiteral("12"),QStringLiteral("-bf"),QStringLiteral("2"),QStringLiteral("-x264-params"),QStringLiteral("b-adapt=0"),QStringLiteral("-threads"),QStringLiteral("2"),QStringLiteral("-pix_fmt"),QStringLiteral("yuv420p"),cfr},&generated);
    if(!generated)return fail(QStringLiteral("B-frame fixture generation failed"));
    execute(QStringLiteral("ffmpeg"),{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-y"),QStringLiteral("-i"),cfr,QStringLiteral("-vf"),QStringLiteral("select='eq(n,0)+eq(n,1)+eq(n,4)+eq(n,10)+eq(n,11)+eq(n,23)'"),QStringLiteral("-fps_mode"),QStringLiteral("vfr"),QStringLiteral("-c:v"),QStringLiteral("libx264"),QStringLiteral("-g"),QStringLiteral("12"),QStringLiteral("-bf"),QStringLiteral("2"),QStringLiteral("-x264-params"),QStringLiteral("b-adapt=0"),QStringLiteral("-threads"),QStringLiteral("2"),vfr},&generated);
    if(!generated)return fail(QStringLiteral("Irregular VFR fixture generation failed"));
    for(const auto &path:{vfr}){
        const auto reference=decode(path);const auto originalHash=hash(path);
        if(reference.frames.size()<6||!reference.hasB)return fail(QStringLiteral("Fixture lacks verified decoded B frames"));
        if(path==vfr){bool irregular=false;for(int i=2;i<reference.ms.size();++i)irregular|=qAbs((reference.ms[i]-reference.ms[i-1])-(reference.ms[1]-reference.ms[0]))>1;if(!irregular)return fail(QStringLiteral("VFR fixture has no genuinely unequal PTS gaps"));}
        window=new FrameInspectionWindow(QUrl::fromLocalFile(path),0,owner->engine());window->resize(qEnvironmentVariableIsSet("NKSHOT_TEST_COMPACT")?QSize(420,520):QSize(900,620));window->show();model=qobject_cast<FrameInspectionSession *>(window->sessionObject());
        if(!model||!verify(window,model,reference,0)||!navigate(window,model,reference))return fail(QStringLiteral("B/VFR actual navigation failed sequential oracle"));
        // Representative reverse step across a genuinely unequal-PTS interval;
        // exhaustive B/CFR/VFR index coverage lives in the backend tests.
        QTest::keyClick(window,Qt::Key_Left);
        if(!verify(window,model,reference,int(reference.frames.size())-2))return fail(QStringLiteral("VFR reverse frame differs from sequential oracle"));
        if(hash(path)!=originalHash||!window->grabWindow().save(evidence+(path==vfr?QStringLiteral(".exact-vfr.png"):QStringLiteral(".exact-bframes.png")))||!click(window,"nkshotFrameInspectionClose")||!QTest::qWaitFor([&]{return window.isNull();},1000))return fail(QStringLiteral("B/VFR fixture source or close isolation failed"));
    }
    if(hash(source)!=sourceHash||core->currentVideo()!=sourceUrl||document->renderToImage()!=image||document->undoStackDepth()!=history)return fail(QStringLiteral("Frame inspection changed original video or image history"));
    qInfo()<<"115 actual frame menu/navigation/copy/save and owner/source lifecycle passed; B-frame and unequal-PTS VFR pixels match independent full decode, no Qt-seek oracle";
    return true;
}
