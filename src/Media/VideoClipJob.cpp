// SPDX-License-Identifier: GPL-3.0-only
#include "VideoClipJob.h"
#include <QProcess>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryFile>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cmath>
#include <sys/stat.h>
#include <unistd.h>

namespace {
QStringList probeArgs(const QString &path) {
    return {QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-protocol_whitelist"), QStringLiteral("file"),
        QStringLiteral("-f"), QStringLiteral("mov"), QStringLiteral("-enable_drefs"), QStringLiteral("0"), QStringLiteral("-use_absolute_path"), QStringLiteral("0"), QStringLiteral("-show_streams"), QStringLiteral("-show_format"), QStringLiteral("-of"), QStringLiteral("json"), path};
}
struct Media { bool valid=false; bool audio=false; qint64 duration=0; int width=0,height=0,audioRate=0; qreal frameMs=0; };
Media parseMedia(const QByteArray &data) {
    const auto doc=QJsonDocument::fromJson(data); if(!doc.isObject()) return {};
    const auto root=doc.object(); const auto format=root.value(QStringLiteral("format")).toObject();
    if(!format.value(QStringLiteral("format_name")).toString().split(u',').contains(QStringLiteral("mp4"))) return {};
    const double duration=format.value(QStringLiteral("duration")).toString().toDouble();
    if(!std::isfinite(duration)||duration<=0||duration>7200) return {};
    if(qAbs(format.value(QStringLiteral("start_time")).toString().toDouble())>0.001) return {};
    int video=0,audio=0; Media out;
    for(const auto &value:root.value(QStringLiteral("streams")).toArray()) {
        const auto s=value.toObject();
        if(qAbs(s.value(QStringLiteral("start_time")).toString().toDouble())>0.001) return {};
        const auto type=s.value(QStringLiteral("codec_type")).toString();
        if(type==QStringLiteral("video")) {
            ++video; out.width=s.value(QStringLiteral("width")).toInt(); out.height=s.value(QStringLiteral("height")).toInt();
            if(s.value(QStringLiteral("codec_name")).toString()!=QStringLiteral("h264") || out.width<2||out.height<2||out.width>4096||out.height>4096||out.width%2||out.height%2) return {};
            const auto pixel=s.value(QStringLiteral("pix_fmt")).toString();
            if(pixel!=QStringLiteral("yuv420p")&&pixel!=QStringLiteral("yuvj420p")) return {};
            if(s.value(QStringLiteral("disposition")).toObject().value(QStringLiteral("attached_pic")).toInt()) return {};
            for(const auto &side:s.value(QStringLiteral("side_data_list")).toArray())
                if(side.toObject().contains(QStringLiteral("rotation")) && side.toObject().value(QStringLiteral("rotation")).toDouble()!=0) return {};
            const auto transfer=s.value(QStringLiteral("color_transfer")).toString();
            if(transfer==QStringLiteral("smpte2084")||transfer==QStringLiteral("arib-std-b67")) return {};
            const auto rate=s.value(QStringLiteral("avg_frame_rate")).toString().split(u'/');
            if(rate.size()!=2 || rate[1].toDouble()<=0 || rate[0].toDouble()/rate[1].toDouble()<=0 || rate[0].toDouble()/rate[1].toDouble()>120) return {};
            out.frameMs=1000*rate[1].toDouble()/rate[0].toDouble();
        } else if(type==QStringLiteral("audio")) {
            ++audio;out.audioRate=s.value(QStringLiteral("sample_rate")).toString().toInt();
            if(out.audioRate<1||out.audioRate>192000)return {};
            if(s.value(QStringLiteral("channels")).toInt()<1||s.value(QStringLiteral("channels")).toInt()>2) return {};
        } else return {}; // No silent loss of subtitle/data/extra streams.
    }
    if(video!=1||audio>1) return {};
    out.audio=audio==1; out.duration=qRound64(duration*1000); out.valid=true; return out;
}
QString seconds(qint64 ms) { return QString::number(double(ms)/1000,'f',3); }
}
VideoClipJob::VideoClipJob(QObject *parent):QObject(parent) {
    m_timeout.setSingleShot(true);
    connect(&m_timeout,&QTimer::timeout,this,[this]{fail(tr("The clip operation timed out. The original and destination are unchanged."));});
}
VideoClipJob::~VideoClipJob() {
    if(m_process) { disconnect(m_process,nullptr,this,nullptr); m_process->kill(); m_process->waitForFinished(1000); }
    cleanup();
}
void VideoClipJob::cleanup() {
    if(!m_temp.isEmpty()) QFile::remove(m_temp);
    m_temp.clear();
}
void VideoClipJob::stop() {
    ++m_epoch; m_timeout.stop(); m_stage=None;
    if(m_process) {
        auto process=m_process.data(); const auto temporary=m_temp;
        disconnect(process,nullptr,this,nullptr);
        connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),process,[process,temporary]{if(!temporary.isEmpty())QFile::remove(temporary);process->deleteLater();});
        process->kill();
        if(process->state()==QProcess::NotRunning) {if(!temporary.isEmpty())QFile::remove(temporary);process->deleteLater();}
        m_temp.clear(); // The finished handler owns cleanup until the killed process exits.
        m_process=nullptr;
    }
    cleanup(); m_busy=false;
}
void VideoClipJob::cancel() { stop(); m_progress=0; m_output=QUrl(); m_error.clear(); Q_EMIT changed(); }
void VideoClipJob::fail(const QString &message) { stop(); m_error=message; m_progress=0; m_output=QUrl(); Q_EMIT changed(); }
bool VideoClipJob::sourceUnchanged() const {
    const QFileInfo f(m_source.toLocalFile());
    struct stat st{};
    return !f.isSymLink() && f.isFile() && f.size()==m_sourceSize && f.lastModified()==m_sourceModified
        && ::stat(QFile::encodeName(f.absoluteFilePath()).constData(),&st)==0
        && quint64(st.st_dev)==m_sourceDevice && quint64(st.st_ino)==m_sourceInode;
}
bool VideoClipJob::destinationUnchanged() const {
    const QFileInfo f(m_target);
    return !f.isSymLink() && !f.exists();
}
void VideoClipJob::inspect(const QUrl &source) {
    stop();m_ready=false;m_duration=0;m_audio=false;m_source=QUrl();m_output=QUrl();m_error.clear();m_progress=0;
    const QFileInfo f(source.toLocalFile());
    if(!source.isLocalFile()||!f.isAbsolute()||!f.isFile()||!f.isReadable()||f.suffix().compare(QStringLiteral("mp4"),Qt::CaseInsensitive)||f.size()<=0||f.size()>qint64(2)*1024*1024*1024) {
        fail(tr("Choose a readable local MP4 recording, up to 2 GiB. The original is unchanged."));return;
    }
    m_source=QUrl::fromLocalFile(f.canonicalFilePath());m_sourceSize=f.size();m_sourceModified=f.lastModified();
    struct stat st{};
    if(::stat(QFile::encodeName(m_source.toLocalFile()).constData(),&st)!=0){fail(tr("The source recording could not be inspected."));return;}
    m_sourceDevice=st.st_dev;m_sourceInode=st.st_ino;
    m_ffmpeg=QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));m_ffprobe=QStandardPaths::findExecutable(QStringLiteral("ffprobe"));
    if(m_ffmpeg.isEmpty()||m_ffprobe.isEmpty()) {fail(tr("Clip export needs FFmpeg and ffprobe installed by your system package manager. No software was installed."));return;}
    m_busy=true;const auto epoch=m_epoch;Q_EMIT changed();if(epoch==m_epoch&&m_busy)launch(m_ffprobe,probeArgs(m_source.toLocalFile()),Inspect,15000);
}
void VideoClipJob::launch(const QString &program,const QStringList &args,Stage stage,int timeoutMs) {
    m_stage=stage; m_stdout.clear();m_stderr.clear();m_progressBuffer.clear();
    auto process=new QProcess(this);m_process=process;const auto epoch=m_epoch;
    connect(process,&QProcess::readyReadStandardOutput,this,[this,process,epoch,stage]{
        if(epoch!=m_epoch||m_process!=process)return;
        auto bytes=process->readAllStandardOutput();
        if(stage==Encode) {
            m_progressBuffer+=bytes;
            if(m_progressBuffer.size()>65536){fail(tr("The encoder returned invalid progress data."));return;}
            while(m_progressBuffer.contains('\n')) {
                const auto line=m_progressBuffer.left(m_progressBuffer.indexOf('\n'));m_progressBuffer.remove(0,line.size()+1);
                if(line.startsWith("out_time_us=")) {bool ok=false;const auto us=line.mid(12).toLongLong(&ok);if(ok&&m_clipDuration>0){m_progress=qBound(0.,double(us)/(m_clipDuration*1000.)*.9,.9);Q_EMIT changed();if(epoch!=m_epoch||m_process!=process)return;}}
            }
        } else {m_stdout+=bytes;if(m_stdout.size()>1024*1024)fail(tr("The media metadata exceeds the safe size limit."));}
    });
    connect(process,&QProcess::readyReadStandardError,this,[this,process,epoch]{if(epoch==m_epoch&&m_process==process){m_stderr+=process->readAllStandardError();if(m_stderr.size()>16384)m_stderr=m_stderr.right(16384);}});
    connect(process,&QProcess::errorOccurred,this,[this,process,epoch](QProcess::ProcessError e){if(epoch==m_epoch&&m_process==process&&e==QProcess::FailedToStart)fail(tr("The media tool could not start. Check your FFmpeg installation."));});
    connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,process,epoch,stage](int code,QProcess::ExitStatus status){
        if(epoch!=m_epoch||m_process!=process)return;
        m_timeout.stop();m_process=nullptr;process->deleteLater();
        if(code!=0||status!=QProcess::NormalExit){fail(tr("The media tool could not finish this clip. The original and destination are unchanged."));return;}
        completed(stage);
    });
    m_timeout.start(timeoutMs);process->setProgram(program);process->setArguments(args);process->start(QIODevice::ReadOnly);
}
void VideoClipJob::completed(Stage stage) {
    if(!sourceUnchanged()){m_ready=false;fail(tr("The source recording changed. Reopen clip export to inspect it again."));return;}
    if(stage==Inspect) {
        const auto info=parseMedia(m_stdout);
        if(!info.valid){fail(tr("This clip needs one SDR H.264 video track and at most one mono/stereo audio track in an MP4, with even dimensions up to 4096 pixels and duration up to two hours. Rotated, HDR and extra-track files are not supported yet."));return;}
        m_duration=info.duration;m_audio=info.audio;m_width=info.width;m_height=info.height;m_frameMs=info.frameMs;
        launch(m_ffmpeg,{QStringLiteral("-hide_banner"),QStringLiteral("-encoders")},Encoders,15000);
    } else if(stage==Encoders) {
        if(!m_stdout.contains(" libx264 ")||(m_audio&&!m_stdout.contains(" aac "))){fail(tr("This FFmpeg installation lacks the H.264 or AAC encoder needed for clip export."));return;}
        m_ready=true;m_busy=false;m_stage=None;Q_EMIT changed();
    } else if(stage==Encode) {
        if(QFileInfo(m_temp).size()<=0||QFileInfo(m_temp).size()>qint64(2)*1024*1024*1024){fail(tr("The encoded clip is empty or exceeds the 2 GiB limit."));return;}
        launch(m_ffprobe,probeArgs(m_temp),Verify,15000);
    } else if(stage==Verify) {
        const auto info=parseMedia(m_stdout);
        if(!info.valid||info.audio!=m_audio||(info.audio&&info.audioRate!=48000)||info.width!=m_width||info.height!=m_height||qAbs(info.duration-m_clipDuration)>qMax(m_frameMs, qreal(m_audio ? 24 : 0))+3){fail(tr("The exported clip could not be verified. The destination is unchanged."));return;}
        launch(m_ffmpeg,{QStringLiteral("-hide_banner"),QStringLiteral("-loglevel"),QStringLiteral("error"),QStringLiteral("-nostdin"),QStringLiteral("-xerror"),QStringLiteral("-protocol_whitelist"),QStringLiteral("file"),QStringLiteral("-f"),QStringLiteral("mov"),QStringLiteral("-enable_drefs"),QStringLiteral("0"),QStringLiteral("-use_absolute_path"),QStringLiteral("0"),QStringLiteral("-threads"),QStringLiteral("2"),QStringLiteral("-filter_threads"),QStringLiteral("2"),QStringLiteral("-i"),m_temp,QStringLiteral("-map"),QStringLiteral("0:v:0"),QStringLiteral("-map"),QStringLiteral("0:a:0?"),QStringLiteral("-f"),QStringLiteral("null"),QStringLiteral("-")},Decode,30*60*1000);
    } else if(stage==Decode) {
        if(!destinationUnchanged()){fail(tr("The destination changed during export. Choose another file and try again."));return;}
        // link() publishes a complete same-filesystem temporary file without
        // replacing a destination that appeared after the save dialog.
        if(::link(QFile::encodeName(m_temp).constData(),QFile::encodeName(m_target).constData())!=0) {
            fail(tr("The destination appeared or could not be created. Choose a new filename."));return;
        }
        const auto output=QUrl::fromLocalFile(m_target);stop();m_output=output;m_progress=1;m_error.clear();const auto epoch=m_epoch;Q_EMIT changed();if(epoch==m_epoch)Q_EMIT exported(output);
    }
}
void VideoClipJob::exportClip(const QUrl &target,qint64 startMs,qint64 endMs) {
    if(m_busy)return;
    m_error.clear();m_output=QUrl();m_progress=0;
    if(!m_ready||!sourceUnchanged()){m_ready=false;fail(tr("Reopen clip export to inspect the current recording."));return;}
    if(startMs<0||startMs>m_duration||endMs<startMs||endMs>m_duration||endMs-startMs<100){fail(tr("Choose a range of at least 0.1 seconds inside the recording."));return;}
    QFileInfo f(target.toLocalFile());const QFileInfo source(m_source.toLocalFile());
    if(!target.isLocalFile()||!f.isAbsolute()||f.suffix().compare(QStringLiteral("mp4"),Qt::CaseInsensitive)||f.isSymLink()||!f.dir().exists()||(f.exists()&&!f.isFile())||f.absoluteFilePath()==source.absoluteFilePath()||(!f.canonicalFilePath().isEmpty()&&f.canonicalFilePath()==source.canonicalFilePath())) {fail(tr("Choose a different local MP4 file, not the original recording."));return;}
    struct stat inputStat{},outputStat{};
    if(::stat(QFile::encodeName(source.absoluteFilePath()).constData(),&inputStat)==0
        && ::stat(QFile::encodeName(f.absoluteFilePath()).constData(),&outputStat)==0
        && inputStat.st_dev==outputStat.st_dev && inputStat.st_ino==outputStat.st_ino) {
        fail(tr("Choose a different file, not a hard link to the original recording."));return;
    }
    if(f.exists()){fail(tr("The destination already exists. Choose a new filename."));return;}
    m_target=f.absoluteFilePath();m_clipDuration=endMs-startMs;
    QTemporaryFile temp(f.dir().filePath(QStringLiteral(".nkshot-clip-XXXXXX.mp4")));temp.setAutoRemove(false);
    if(!temp.open()){fail(tr("A temporary clip cannot be created in this folder."));return;}m_temp=temp.fileName();temp.close();
    QStringList args{QStringLiteral("-hide_banner"),QStringLiteral("-loglevel"),QStringLiteral("error"),QStringLiteral("-nostdin"),QStringLiteral("-xerror"),QStringLiteral("-y"),QStringLiteral("-protocol_whitelist"),QStringLiteral("file"),QStringLiteral("-f"),QStringLiteral("mov"),QStringLiteral("-enable_drefs"),QStringLiteral("0"),QStringLiteral("-use_absolute_path"),QStringLiteral("0"),QStringLiteral("-threads"),QStringLiteral("2"),QStringLiteral("-filter_threads"),QStringLiteral("2"),QStringLiteral("-i"),m_source.toLocalFile(),QStringLiteral("-t"),seconds(m_clipDuration),QStringLiteral("-vf"),QStringLiteral("trim=start=%1:end=%2,setpts=PTS-STARTPTS").arg(seconds(startMs),seconds(endMs)),QStringLiteral("-map"),QStringLiteral("0:v:0")};
    if(m_audio)args<<QStringLiteral("-map")<<QStringLiteral("0:a:0")<<QStringLiteral("-af")<<QStringLiteral("atrim=start=%1:end=%2,asetpts=PTS-STARTPTS").arg(seconds(startMs),seconds(endMs));
    args<<QStringLiteral("-map_metadata")<<QStringLiteral("-1")<<QStringLiteral("-map_chapters")<<QStringLiteral("-1")<<QStringLiteral("-c:v")<<QStringLiteral("libx264")<<QStringLiteral("-preset")<<QStringLiteral("veryfast")<<QStringLiteral("-crf")<<QStringLiteral("20")<<QStringLiteral("-pix_fmt")<<QStringLiteral("yuv420p")<<QStringLiteral("-threads")<<QStringLiteral("2");
    if(m_audio)args<<QStringLiteral("-c:a")<<QStringLiteral("aac")<<QStringLiteral("-b:a")<<QStringLiteral("160k")<<QStringLiteral("-ar")<<QStringLiteral("48000");
    args<<QStringLiteral("-movflags")<<QStringLiteral("+faststart")<<QStringLiteral("-progress")<<QStringLiteral("pipe:1")<<QStringLiteral("-nostats")<<QStringLiteral("-fs")<<QStringLiteral("2147483648")<<QStringLiteral("-f")<<QStringLiteral("mp4")<<m_temp;
    m_busy=true;const auto epoch=m_epoch;Q_EMIT changed();if(epoch==m_epoch&&m_busy)launch(m_ffmpeg,args,Encode,30*60*1000);
}
