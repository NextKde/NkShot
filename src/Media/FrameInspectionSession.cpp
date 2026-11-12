// SPDX-License-Identifier: GPL-3.0-only
#include "FrameInspectionSession.h"
#include "LocalVideoInput.h"
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QFileInfo>
#include <QFile>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QMutex>
#include <QMutexLocker>
#include <algorithm>
#include <cmath>
#include <numeric>
namespace {
bool same(const struct stat &a,const struct stat &b){return a.st_dev==b.st_dev&&a.st_ino==b.st_ino&&a.st_size==b.st_size&&a.st_mtim.tv_sec==b.st_mtim.tv_sec&&a.st_mtim.tv_nsec==b.st_mtim.tv_nsec&&a.st_ctim.tv_sec==b.st_ctim.tv_sec&&a.st_ctim.tv_nsec==b.st_ctim.tv_nsec;}
bool integer(const QJsonValue &v,qint64 &n){bool ok=false;if(v.isString())n=v.toString().toLongLong(&ok);else if(v.isDouble()){const auto d=v.toDouble();ok=std::isfinite(d)&&std::abs(d)<=9007199254740991.&&std::floor(d)==d;if(ok)n=qint64(d);}return ok&&n>=-9007199254740991LL&&n<=9007199254740991LL;}
}
class FrameInspectionProvider final:public QQuickImageProvider {
public:
    FrameInspectionProvider():QQuickImageProvider(Image){}
    void set(const QImage &value,quint64 epoch){QMutexLocker lock(&mutex);image=value;key=QString::number(epoch);}
    QImage requestImage(const QString &id,QSize *size,const QSize &) override{QMutexLocker lock(&mutex);const auto out=id==key?image:QImage();if(size)*size=out.size();return out;}
private:QMutex mutex;QImage image;QString key;
};
FrameInspectionSession::FrameInspectionSession(QQmlEngine *engine,QObject *parent):QObject(parent),m_engine(engine),m_process(new QProcess(this)) {
    static quint64 serial=0;m_providerId=QStringLiteral("nkshotframeinspection%1").arg(++serial);
    if(engine){m_provider=new FrameInspectionProvider;engine->addImageProvider(m_providerId,m_provider);}
    m_timeout.setSingleShot(true);connect(&m_timeout,&QTimer::timeout,this,[this]{fail(tr("Frame inspection timed out. Try a shorter recording."));});
    connect(m_process,&QProcess::readyReadStandardOutput,this,&FrameInspectionSession::readOutput);
    connect(m_process,&QProcess::readyReadStandardError,this,&FrameInspectionSession::readOutput);
    connect(m_process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,&FrameInspectionSession::finished);
    connect(m_process,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){if(error==QProcess::FailedToStart){fail(tr("FFmpeg or FFprobe could not start."));}});
}
FrameInspectionSession::~FrameInspectionSession(){
    m_process->disconnect(this);m_process->setParent(nullptr);
    if(m_process->state()==QProcess::NotRunning)m_process->deleteLater();
    else{connect(m_process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),m_process,&QObject::deleteLater);m_process->kill();}
    if(m_engine)m_engine->removeImageProvider(m_providerId);
}
void FrameInspectionSession::resetImage(){m_image={};m_current=-1;if(m_engine&&m_provider)m_provider->set({},m_epoch);}
void FrameInspectionSession::clear(){++m_epoch;m_timeout.stop();m_pending=Idle;m_busy=false;m_error.clear();m_path.clear();m_pts.clear();m_keys.clear();m_requested=-1;m_size={};m_output.clear();m_diagnostics.clear();resetImage();if(m_process->state()!=QProcess::NotRunning)m_process->kill();Q_EMIT changed();}
void FrameInspectionSession::fail(const QString &message){++m_epoch;m_pending=Idle;m_timeout.stop();m_busy=false;m_error=message;resetImage();if(m_process->state()!=QProcess::NotRunning)m_process->kill();Q_EMIT changed();}
bool FrameInspectionSession::unchanged() const {struct stat st{};return !m_path.isEmpty()&&::lstat(QFile::encodeName(m_path).constData(),&st)==0&&S_ISREG(st.st_mode)&&same(st,m_identity);}
bool FrameInspectionSession::begin(const QUrl &url,qint64 requestedPositionMs){
    QPointer<FrameInspectionSession> guard(this);const auto revision=m_epoch+1;clear();if(!guard||m_epoch!=revision)return false;
    const auto path=url.toLocalFile();QFileInfo file(path);
    if(!m_engine||!url.isValid()||!url.isLocalFile()||!url.host().isEmpty()||!url.userInfo().isEmpty()||url.port()!=-1||url.hasQuery()||url.hasFragment()||!file.isAbsolute()||path.startsWith(QStringLiteral("//"))||path.contains(QChar(u'\0'))||requestedPositionMs<0){fail(tr("Choose a local MP4 or WebM recording."));return false;}
    m_path=path;m_webm=file.suffix().compare(QStringLiteral("webm"),Qt::CaseInsensitive)==0;
    if((!m_webm&&file.suffix().compare(QStringLiteral("mp4"),Qt::CaseInsensitive)!=0)||::lstat(QFile::encodeName(path).constData(),&m_identity)||!S_ISREG(m_identity.st_mode)||m_identity.st_size<=0||m_identity.st_size>256LL*1024*1024||(m_webm&&!NkShot::LocalVideoInput::hasWebMDocType(path))){fail(tr("Use a regular local recording up to 256 MiB; links and special files are not supported."));return false;}
    m_ffmpeg=QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));m_ffprobe=QStandardPaths::findExecutable(QStringLiteral("ffprobe"));
    if(m_ffmpeg.isEmpty()||m_ffprobe.isEmpty()){fail(tr("Install FFmpeg and FFprobe to inspect frames."));return false;}
    m_initialMs=requestedPositionMs;queue(Metadata);return true;
}
void FrameInspectionSession::queue(Stage stage){m_pending=stage;m_busy=true;QPointer<FrameInspectionSession> guard(this);const auto epoch=m_epoch;Q_EMIT changed();if(!guard||epoch!=m_epoch)return;if(m_process->state()!=QProcess::NotRunning)m_process->kill();else dispatch();}
void FrameInspectionSession::dispatch(){
    if(m_pending==Idle||m_process->state()!=QProcess::NotRunning)return;
    if(!unchanged()){fail(tr("The recording changed. Reopen frame inspection."));return;}
    m_stage=m_pending;m_pending=Idle;m_runningEpoch=m_epoch;m_output.clear();m_diagnostics.clear();m_process->readAllStandardOutput();m_process->readAllStandardError();
    const auto demux=NkShot::LocalVideoInput::demuxArguments(m_webm?NkShot::LocalVideoInput::Container::WebM:NkShot::LocalVideoInput::Container::Mp4);
    QStringList args;
    if(m_stage!=Frame){args={QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-max_alloc"),QStringLiteral("134217728"),QStringLiteral("-threads"),QStringLiteral("2")};args+=demux;
        if(m_stage==Metadata)args<<QStringLiteral("-show_streams")<<QStringLiteral("-show_format");
        else args<<QStringLiteral("-select_streams")<<QStringLiteral("v:0")<<QStringLiteral("-show_frames")<<QStringLiteral("-show_entries")<<QStringLiteral("frame=best_effort_timestamp,key_frame");
        args<<QStringLiteral("-of")<<QStringLiteral("json")<<m_path;
    }else{
        const auto it=std::upper_bound(m_keys.cbegin(),m_keys.cend(),m_requested);const int anchor=*(it-1);
        const auto seconds=QString::number(double(m_pts[anchor])*double(m_num)/double(m_den),'f',12);
        args={QStringLiteral("-v"),QStringLiteral("info"),QStringLiteral("-xerror"),QStringLiteral("-max_alloc"),QStringLiteral("134217728"),QStringLiteral("-nostdin"),QStringLiteral("-copyts"),QStringLiteral("-seek_timestamp"),QStringLiteral("1"),QStringLiteral("-ss"),seconds,QStringLiteral("-noaccurate_seek"),QStringLiteral("-threads"),QStringLiteral("2"),QStringLiteral("-filter_threads"),QStringLiteral("2")};args+=demux;
        args<<QStringLiteral("-noautorotate")<<QStringLiteral("-i")<<m_path<<QStringLiteral("-map")<<QStringLiteral("0:v:0")<<QStringLiteral("-an")<<QStringLiteral("-vf")
            <<QStringLiteral("settb=expr=%1,select=eq(pts\\,%2),showinfo").arg(m_timeBase).arg(m_pts[m_requested])
            <<QStringLiteral("-frames:v")<<QStringLiteral("1")<<QStringLiteral("-fps_mode")<<QStringLiteral("passthrough")<<QStringLiteral("-threads")<<QStringLiteral("2")<<QStringLiteral("-pix_fmt")<<QStringLiteral("rgb24")<<QStringLiteral("-f")<<QStringLiteral("rawvideo")<<QStringLiteral("pipe:1");
    }
    m_timeout.start(m_stage==Frame?10000:60000);m_process->start(m_stage==Frame?m_ffmpeg:m_ffprobe,args,QIODevice::ReadOnly);
}
void FrameInspectionSession::readOutput(){
    const auto out=m_process->readAllStandardOutput(),err=m_process->readAllStandardError();if(m_runningEpoch!=m_epoch)return;
    const qint64 limit=m_stage==Frame?qint64(m_size.width())*m_size.height()*3:m_stage==Index?8LL*1024*1024:1024*1024;
    if(m_output.size()+out.size()>limit||m_diagnostics.size()+err.size()>64*1024){fail(tr("Frame inspection exceeded its safe output limit."));return;}
    m_output+=out;m_diagnostics+=err;
}
bool FrameInspectionSession::parseMetadata(){
    const auto doc=QJsonDocument::fromJson(m_output);if(!doc.isObject())return false;const auto root=doc.object();const auto format=root.value(QStringLiteral("format")).toObject();
    bool ok=false;const auto duration=format.value(QStringLiteral("duration")).toString().toDouble(&ok);
    if(!ok||!std::isfinite(duration)||duration<=0||duration>600||!format.value(QStringLiteral("format_name")).toString().split(u',').contains(m_webm?QStringLiteral("webm"):QStringLiteral("mp4")))return false;
    int videos=0,audios=0;
    for(const auto &value:root.value(QStringLiteral("streams")).toArray()){
        const auto s=value.toObject();const auto type=s.value(QStringLiteral("codec_type")).toString();
        if(type==QStringLiteral("audio")){if(++audios>1||s.value(QStringLiteral("channels")).toInt()<1||s.value(QStringLiteral("channels")).toInt()>2)return false;continue;}
        if(type!=QStringLiteral("video")||++videos>1)return false;
        if(s.value(QStringLiteral("codec_name")).toString()!=(m_webm?QStringLiteral("vp9"):QStringLiteral("h264")))return false;
        m_size={s.value(QStringLiteral("width")).toInt(),s.value(QStringLiteral("height")).toInt()};
        if(m_size.width()<2||m_size.height()<2||m_size.width()>4096||m_size.height()>4096||qint64(m_size.width())*m_size.height()>8294400)return false;
        const auto pixel=s.value(QStringLiteral("pix_fmt")).toString();if(pixel!=QStringLiteral("yuv420p")&&pixel!=QStringLiteral("yuvj420p"))return false;
        const auto sar=s.value(QStringLiteral("sample_aspect_ratio")).toString();if(!sar.isEmpty()&&sar!=QStringLiteral("N/A")&&sar!=QStringLiteral("0:1")&&sar!=QStringLiteral("1:1"))return false;
        const auto transfer=s.value(QStringLiteral("color_transfer")).toString();if(transfer==QStringLiteral("smpte2084")||transfer==QStringLiteral("arib-std-b67"))return false;
        if(s.value(QStringLiteral("disposition")).toObject().value(QStringLiteral("attached_pic")).toInt())return false;
        for(const auto &side:s.value(QStringLiteral("side_data_list")).toArray()){const auto entry=side.toObject();if(entry.value(QStringLiteral("rotation")).toDouble()!=0||entry.value(QStringLiteral("side_data_type")).toString()==QStringLiteral("Display Matrix"))return false;}
        const auto tags=s.value(QStringLiteral("tags")).toObject();if(tags.contains(QStringLiteral("rotate"))&&tags.value(QStringLiteral("rotate")).toString()!=QStringLiteral("0"))return false;
        m_timeBase=s.value(QStringLiteral("time_base")).toString();const auto parts=m_timeBase.split(u'/');if(parts.size()!=2)return false;
        m_num=parts[0].toLongLong(&ok);if(!ok||m_num<1||m_num>1000000000)return false;m_den=parts[1].toLongLong(&ok);if(!ok||m_den<1||m_den>1000000000)return false;
        // Reconstruct trusted numeric filter syntax, never interpolate input strings.
        const auto divisor=std::gcd(m_num,m_den);m_num/=divisor;m_den/=divisor;
        m_timeBase=QStringLiteral("%1/%2").arg(m_num).arg(m_den);
    }
    return videos==1;
}
bool FrameInspectionSession::parseIndex(){
    const auto doc=QJsonDocument::fromJson(m_output);if(!doc.isObject())return false;const auto frames=doc.object().value(QStringLiteral("frames")).toArray();if(frames.isEmpty()||frames.size()>36000)return false;
    QVector<qint64> pts;QVector<int> keys;
    for(const auto &value:frames){const auto frame=value.toObject();qint64 timestamp=0;if(!integer(frame.value(QStringLiteral("best_effort_timestamp")),timestamp)||(!pts.isEmpty()&&timestamp<=pts.last()))return false;
        const int key=frame.value(QStringLiteral("key_frame")).toInt(-1);if(key!=0&&key!=1)return false;if(key)keys.append(pts.size());pts.append(timestamp);}
    if(keys.isEmpty()||keys.first()!=0||pts.first()<0)return false;
    const auto span=(static_cast<long double>(pts.last())-pts.first())*m_num/m_den;
    if(span<0||span>600||std::abs(static_cast<long double>(pts.first())*m_num/m_den)>86400)return false;
    m_pts=pts;m_keys=keys;return true;
}
void FrameInspectionSession::requestFrame(int index){
    if(index<0||index>=m_pts.size()){if(!m_pts.isEmpty())fail(tr("Choose a frame within this recording."));return;}
    if(!unchanged()){fail(tr("The recording changed. Reopen frame inspection."));return;}
    if(ready()&&index==m_current)return;
    ++m_epoch;m_timeout.stop();m_error.clear();m_requested=index;resetImage();queue(Frame);
}
void FrameInspectionSession::finished(int code,QProcess::ExitStatus status){
    const auto completedEpoch=m_runningEpoch;
    const QPointer<FrameInspectionSession> guard(this);readOutput();
    // A failure observer may synchronously begin and dispatch a new process.
    if(!guard||m_runningEpoch!=completedEpoch)return;
    m_timeout.stop();
    if(m_runningEpoch!=m_epoch){dispatch();return;}
    if(code!=0||status!=QProcess::NormalExit){fail(tr("The requested frame could not be decoded safely."));return;}
    if(!unchanged()){fail(tr("The recording changed. Reopen frame inspection."));return;}
    if(m_stage==Metadata){if(!parseMetadata()){fail(tr("Use one SDR H.264 MP4 or VP9 WebM video track, square pixels, up to 4K and ten minutes."));return;}queue(Index);return;}
    if(m_stage==Index){
        if(!parseIndex()){fail(tr("This recording needs unique increasing frame timestamps and at most 36,000 frames."));return;}
        int target=0;for(int i=1;i<m_pts.size();++i){const long double ms=(static_cast<long double>(m_pts[i])-m_pts[0])*m_num*1000/m_den;if(ms>m_initialMs)break;target=i;}
        requestFrame(target);return;
    }
    if(m_stage!=Frame)return;
    const auto diagnostic=QString::fromUtf8(m_diagnostics);
    const QRegularExpression config(QStringLiteral("config in time_base:\\s*([0-9]+/[0-9]+)"));
    const QRegularExpression frame(QStringLiteral("\\bn:\\s*[0-9]+\\s+pts:\\s*(-?[0-9]+)"));
    const auto cm=config.match(diagnostic);auto matches=frame.globalMatch(diagnostic);int count=0;qint64 observed=0;
    while(matches.hasNext()){const auto match=matches.next();bool ok=false;observed=match.captured(1).toLongLong(&ok);if(!ok){fail(tr("Frame timestamp verification failed."));return;}++count;}
    const auto length=qint64(m_size.width())*m_size.height()*3;
    if(!cm.hasMatch()||cm.captured(1)!=m_timeBase||count!=1||observed!=m_pts[m_requested]||m_output.size()!=length){fail(tr("The decoder did not return exactly the selected frame. No image was extracted."));return;}
    m_image=QImage(reinterpret_cast<const uchar *>(m_output.constData()),m_size.width(),m_size.height(),m_size.width()*3,QImage::Format_RGB888).copy();
    m_output.clear();if(m_image.isNull()){fail(tr("Not enough memory to display the frame."));return;}
    m_image.setDevicePixelRatio(1);m_current=m_requested;m_busy=false;m_stage=Idle;
    if(m_engine&&m_provider){auto preview=m_image;if(qMax(preview.width(),preview.height())>1400)preview=preview.scaled(1400,1400,Qt::KeepAspectRatio,Qt::SmoothTransformation);m_provider->set(preview,m_epoch);}
    Q_EMIT changed();
}
qint64 FrameInspectionSession::positionMs() const {return ready()?qRound64(double(m_pts[m_current]-m_pts.first())*double(m_num)*1000/double(m_den)):0;}
QUrl FrameInspectionSession::previewUrl() const {return ready()&&m_engine?QUrl(QStringLiteral("image://%1/%2").arg(m_providerId).arg(m_epoch)):QUrl();}
