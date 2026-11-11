// SPDX-License-Identifier: GPL-3.0-only
#include "VideoClipJob.h"
#include "LocalVideoInput.h"
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
QStringList probeArgs(const QString &path, bool webm=false) {
    QStringList args{QStringLiteral("-v"), QStringLiteral("error")};
    args += NkShot::LocalVideoInput::demuxArguments(webm ? NkShot::LocalVideoInput::Container::WebM : NkShot::LocalVideoInput::Container::Mp4);
    args << QStringLiteral("-show_streams") << QStringLiteral("-show_format") << QStringLiteral("-of") << QStringLiteral("json") << path;
    return args;
}
struct Media { bool valid=false; bool audio=false; bool squarePixels=true; qint64 duration=0; int width=0,height=0,audioRate=0; qreal frameMs=0; };
Media parseMedia(const QByteArray &data, bool webm=false) {
    const auto doc=QJsonDocument::fromJson(data); if(!doc.isObject()) return {};
    const auto root=doc.object(); const auto format=root.value(QStringLiteral("format")).toObject();
    if(!format.value(QStringLiteral("format_name")).toString().split(u',').contains(webm ? QStringLiteral("webm") : QStringLiteral("mp4"))) return {};
    const double duration=format.value(QStringLiteral("duration")).toString().toDouble();
    if(!std::isfinite(duration)||duration<=0||duration>(webm?600:7200)) return {};
    bool startOk=false;const double start=format.value(QStringLiteral("start_time")).toString().toDouble(&startOk);
    if(!startOk || !std::isfinite(start) || (webm ? start<-.080 || start>.001 : qAbs(start)>.001)) return {};
    int video=0,audio=0; Media out;
    for(const auto &value:root.value(QStringLiteral("streams")).toArray()) {
        const auto s=value.toObject();
        bool streamStartOk=false;const double streamStart=s.value(QStringLiteral("start_time")).toString().toDouble(&streamStartOk);
        if(!streamStartOk || !std::isfinite(streamStart) || (!webm && qAbs(streamStart)>.001)) return {};
        const auto type=s.value(QStringLiteral("codec_type")).toString();
        if(type==QStringLiteral("video")) {
            ++video; if(webm && (streamStart<-.001 || streamStart>.080))return {}; out.width=s.value(QStringLiteral("width")).toInt(); out.height=s.value(QStringLiteral("height")).toInt();
            if(s.value(QStringLiteral("codec_name")).toString()!=(webm ? QStringLiteral("vp9") : QStringLiteral("h264")) || out.width<2||out.height<2||out.width>4096||out.height>4096||out.width%2||out.height%2) return {};
            const auto sar=s.value(QStringLiteral("sample_aspect_ratio")).toString();
            const auto ratio=sar.split(u':');
            out.squarePixels=sar.isEmpty()||sar==QStringLiteral("N/A")||sar==QStringLiteral("0:1")
                ||(ratio.size()==2&&ratio[0].toInt()>0&&ratio[0].toInt()==ratio[1].toInt());
            const auto pixel=s.value(QStringLiteral("pix_fmt")).toString();
            if(pixel!=QStringLiteral("yuv420p")&&(webm||pixel!=QStringLiteral("yuvj420p"))) return {};
            if(s.value(QStringLiteral("disposition")).toObject().value(QStringLiteral("attached_pic")).toInt()) return {};
            for(const auto &side:s.value(QStringLiteral("side_data_list")).toArray())
                if(side.toObject().contains(QStringLiteral("rotation")) && side.toObject().value(QStringLiteral("rotation")).toDouble()!=0) return {};
            const auto transfer=s.value(QStringLiteral("color_transfer")).toString();
            if(transfer==QStringLiteral("smpte2084")||transfer==QStringLiteral("arib-std-b67")) return {};
            const auto rate=s.value(QStringLiteral("avg_frame_rate")).toString().split(u'/');
            const double fps=rate.size()==2&&rate[1].toDouble()>0 ? rate[0].toDouble()/rate[1].toDouble() : 0;
            if(!std::isfinite(fps)||fps>(webm?60:120)||(!webm&&fps<=0))return {};
            out.frameMs=webm ? 1000./30 : 1000./fps;
        } else if(type==QStringLiteral("audio")) {
            ++audio;
            const int padding=s.value(QStringLiteral("initial_padding")).toInt();
            if(webm&&(padding<0||padding>3840))return {};
            if(webm && (streamStart<-.080 || streamStart>.001 || s.value(QStringLiteral("codec_name")).toString()!=QStringLiteral("opus")))return {};
            out.audioRate=s.value(QStringLiteral("sample_rate")).toString().toInt();
            if(out.audioRate<1||out.audioRate>192000||(webm&&out.audioRate!=48000))return {};
            if(s.value(QStringLiteral("channels")).toInt()<1||s.value(QStringLiteral("channels")).toInt()>2) return {};
        } else return {}; // No silent loss of subtitle/data/extra streams.
    }
    if(video!=1||audio>1) return {};
    out.audio=audio==1; out.duration=qRound64(duration*1000); out.valid=true; return out;
}
QStringList gifProbeArgs(const QString &path) {
    return {QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-protocol_whitelist"),QStringLiteral("file"),QStringLiteral("-f"),QStringLiteral("gif"),QStringLiteral("-ignore_loop"),QStringLiteral("1"),QStringLiteral("-count_frames"),QStringLiteral("-show_streams"),QStringLiteral("-show_format"),QStringLiteral("-of"),QStringLiteral("json"),path};
}
bool validGif(const QByteArray &data,qint64 duration,int sourceWidth,int sourceHeight) {
    const auto root=QJsonDocument::fromJson(data).object();
    if(root.value(QStringLiteral("format")).toObject().value(QStringLiteral("format_name")).toString()!=QStringLiteral("gif"))return false;
    const auto streams=root.value(QStringLiteral("streams")).toArray();
    if(streams.size()!=1)return false;
    const auto stream=streams.first().toObject();
    const int width=stream.value(QStringLiteral("width")).toInt(),height=stream.value(QStringLiteral("height")).toInt();
    const int frames=stream.value(QStringLiteral("nb_read_frames")).toString().toInt();
    const double seconds=root.value(QStringLiteral("format")).toObject().value(QStringLiteral("duration")).toString().toDouble();
    const double factor=qMin(1.,640./qMax(sourceWidth,sourceHeight));
    return stream.value(QStringLiteral("codec_name")).toString()==QStringLiteral("gif")
        && stream.value(QStringLiteral("codec_type")).toString()==QStringLiteral("video")
        && width==qMax(1,qRound(sourceWidth*factor))&&height==qMax(1,qRound(sourceHeight*factor))
        && frames>=2&&frames<=150&&std::isfinite(seconds)&&qAbs(seconds*1000-duration)<=103;
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
    if(!m_palette.isEmpty())QFile::remove(m_palette);
    m_palette.clear();
    if(!m_annotationFile.isEmpty())QFile::remove(m_annotationFile);
    m_annotationFile.clear();
}
void VideoClipJob::stop() {
    ++m_epoch; m_timeout.stop(); m_stage=None;
    if(m_process) {
        auto process=m_process.data(); const auto temporary=m_temp;const auto palette=m_palette;const auto annotation=m_annotationFile;
        disconnect(process,nullptr,this,nullptr);
        connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),process,[process,temporary,palette,annotation]{if(!temporary.isEmpty())QFile::remove(temporary);if(!palette.isEmpty())QFile::remove(palette);if(!annotation.isEmpty())QFile::remove(annotation);process->deleteLater();});
        process->kill();
        if(process->state()==QProcess::NotRunning) {if(!temporary.isEmpty())QFile::remove(temporary);if(!palette.isEmpty())QFile::remove(palette);if(!annotation.isEmpty())QFile::remove(annotation);process->deleteLater();}
        m_temp.clear();m_palette.clear();m_annotationFile.clear(); // The finished handler owns cleanup until the killed process exits.
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
    stop();m_ready=false;m_mediaValid=false;m_encodersChecked=false;m_duration=0;m_audio=false;m_source=QUrl();m_output=QUrl();m_error.clear();m_progress=0;
    if(!m_annotationOverlay.isNull()){m_annotationOverlay=QImage();++m_annotationRevision;}m_overlayAvailable=false;
    m_exportSpeed=1.;m_atempoAvailable=false;m_start=0;m_end=0;
    m_outputResolution=QStringLiteral("original");
    m_width=0;m_height=0;m_squarePixels=false;m_privacyEnabled=false;m_drawboxAvailable=false;m_privacyRect={};m_cropPosition=QPoint(-1,-1);m_cropAspect=QStringLiteral("original");
    const QFileInfo f(source.toLocalFile());
    m_webm=f.suffix().compare(QStringLiteral("webm"),Qt::CaseInsensitive)==0;
    if(!source.isLocalFile()||!f.isAbsolute()||!f.isFile()||!f.isReadable()||(!m_webm&&f.suffix().compare(QStringLiteral("mp4"),Qt::CaseInsensitive))||(m_webm&&!NkShot::LocalVideoInput::hasWebMDocType(f.absoluteFilePath()))||f.size()<=0||f.size()>(m_webm?qint64(256)*1024*1024:qint64(2)*1024*1024*1024)) {
        fail(tr("Choose a readable local MP4 up to 2 GiB or VP9 WebM up to 256 MiB. The original is unchanged."));return;
    }
    m_source=QUrl::fromLocalFile(f.canonicalFilePath());m_sourceSize=f.size();m_sourceModified=f.lastModified();
    struct stat st{};
    if(::stat(QFile::encodeName(m_source.toLocalFile()).constData(),&st)!=0){fail(tr("The source recording could not be inspected."));return;}
    m_sourceDevice=st.st_dev;m_sourceInode=st.st_ino;
    m_ffmpeg=QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));m_ffprobe=QStandardPaths::findExecutable(QStringLiteral("ffprobe"));
    if(m_ffmpeg.isEmpty()||m_ffprobe.isEmpty()) {fail(tr("Clip export needs FFmpeg and ffprobe installed by your system package manager. No software was installed."));return;}
    m_busy=true;const auto epoch=m_epoch;Q_EMIT changed();if(epoch==m_epoch&&m_busy)launch(m_ffprobe,probeArgs(m_source.toLocalFile(),m_webm),Inspect,15000);
}
void VideoClipJob::launch(const QString &program,const QStringList &args,Stage stage,int timeoutMs) {
    m_stage=stage; m_stdout.clear();m_stderr.clear();m_progressBuffer.clear();
    auto process=new QProcess(this);m_process=process;const auto epoch=m_epoch;
    connect(process,&QProcess::readyReadStandardOutput,this,[this,process,epoch,stage]{
        if(epoch!=m_epoch||m_process!=process)return;
        auto bytes=process->readAllStandardOutput();
        if(stage==Encode||stage==Palette) {
            m_progressBuffer+=bytes;
            if(m_progressBuffer.size()>65536){fail(tr("The encoder returned invalid progress data."));return;}
            while(m_progressBuffer.contains('\n')) {
                const auto line=m_progressBuffer.left(m_progressBuffer.indexOf('\n'));m_progressBuffer.remove(0,line.size()+1);
                if(line.startsWith("out_time_us=")) {bool ok=false;const auto us=line.mid(12).toLongLong(&ok);if(ok&&m_clipDuration>0){const double fraction=qBound(0.,double(us)/(m_clipDuration*1000.),1.);m_progress=stage==Palette?fraction*.35:(m_format==QStringLiteral("gif")?.35+fraction*.55:fraction*.9);Q_EMIT changed();if(epoch!=m_epoch||m_process!=process)return;}}
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
        const auto info=parseMedia(m_stdout,m_webm);
        if(!info.valid){fail(tr("This clip needs one SDR H.264 MP4 or VP9 WebM video track and at most one mono/stereo audio track (Opus for WebM), with even dimensions up to 4096 pixels and duration up to two hours (WebM: ten minutes). Rotated, HDR and extra-track files are not supported yet."));return;}
        m_mediaValid=true;m_squarePixels=info.squarePixels;m_duration=info.duration;m_audio=info.audio;m_width=info.width;m_height=info.height;m_frameMs=info.frameMs;m_start=0;m_end=m_duration;
        launch(m_ffmpeg,{QStringLiteral("-hide_banner"),QStringLiteral("-encoders")},Encoders,15000);
    } else if(stage==Encoders) {
        if(m_format==QStringLiteral("gif")) {
            if(!m_stdout.contains(" gif ")||!m_stdout.contains(" png ")){fail(tr("This FFmpeg installation lacks the GIF or PNG encoder needed for GIF export."));return;}
            launch(m_ffmpeg,{QStringLiteral("-hide_banner"),QStringLiteral("-filters")},Filters,15000);return;
        }
        m_encodersChecked=true;m_x264Available=m_stdout.contains(" libx264 ");m_aacAvailable=m_stdout.contains(" aac ");
        launch(m_ffmpeg,{QStringLiteral("-hide_banner"),QStringLiteral("-filters")},Filters,15000);
    } else if(stage==Filters) {
        m_atempoAvailable=m_stdout.contains(" atempo ");
        m_overlayAvailable=m_stdout.contains(" overlay ");
        if(!m_annotationOverlay.isNull()&&!m_overlayAvailable){fail(tr("This FFmpeg installation lacks annotation overlay support. Reopen clip export."));return;}
        m_drawboxAvailable=m_stdout.contains(" drawbox ");
        if(m_privacyEnabled&&!m_drawboxAvailable){fail(tr("This FFmpeg installation lacks the opaque privacy-cover filter. Reopen clip export or install a build with drawbox."));return;}
        if(m_format==QStringLiteral("gif")) {
            if(!m_stdout.contains(" palettegen ")||!m_stdout.contains(" paletteuse ")){fail(tr("This FFmpeg installation lacks the palette filters needed for GIF export."));return;}
        } else if(!m_x264Available||(m_audio&&m_includeAudio&&!m_aacAvailable)) {
            fail(tr("This FFmpeg installation lacks the H.264 or AAC encoder needed for clip export."));return;
        }
        m_ready=true;m_busy=false;m_stage=None;Q_EMIT changed();
    } else if(stage==Palette) {
        if(QFileInfo(m_palette).size()<=0||QFileInfo(m_palette).size()>262144){fail(tr("A safe GIF palette could not be created."));return;}
        m_progress=.35;const auto epoch=m_epoch;Q_EMIT changed();if(epoch==m_epoch&&m_busy)launchGifPass(false);
    } else if(stage==Encode) {
        const qint64 limit=m_format==QStringLiteral("gif")?20*1024*1024:qint64(2)*1024*1024*1024;
        if(QFileInfo(m_temp).size()<=0||QFileInfo(m_temp).size()>limit){fail(m_format==QStringLiteral("gif")?tr("The GIF exceeds 20 MiB. Choose a shorter range."):tr("The encoded clip is empty or exceeds the 2 GiB limit."));return;}
        launch(m_ffprobe,m_format==QStringLiteral("gif")?gifProbeArgs(m_temp):probeArgs(m_temp),Verify,15000);
    } else if(stage==Verify) {
        if(m_format==QStringLiteral("gif")) {
            if(!validGif(m_stdout,m_clipDuration,cropRect().width(),cropRect().height())){fail(tr("The GIF could not be verified. Choose a shorter range with at least two frames."));return;}
            launch(m_ffmpeg,{QStringLiteral("-hide_banner"),QStringLiteral("-loglevel"),QStringLiteral("error"),QStringLiteral("-nostdin"),QStringLiteral("-xerror"),QStringLiteral("-protocol_whitelist"),QStringLiteral("file"),QStringLiteral("-f"),QStringLiteral("gif"),QStringLiteral("-ignore_loop"),QStringLiteral("1"),QStringLiteral("-threads"),QStringLiteral("2"),QStringLiteral("-filter_threads"),QStringLiteral("2"),QStringLiteral("-i"),m_temp,QStringLiteral("-map"),QStringLiteral("0:v:0"),QStringLiteral("-f"),QStringLiteral("null"),QStringLiteral("-")},Decode,60000);return;
        }
        const auto info=parseMedia(m_stdout);
        if(!info.valid||info.audio!=(m_audio&&m_includeAudio)||(info.audio&&info.audioRate!=48000)||info.width!=outputSize().width()||info.height!=outputSize().height()||qAbs(info.duration-m_clipDuration)>qMax(m_frameMs, qreal(m_audio&&m_includeAudio ? 24 : 0))+3){fail(tr("The exported clip could not be verified. The destination is unchanged."));return;}
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
    if(m_format==QStringLiteral("mp4")&&outputSize().isEmpty()) {
        fail(tr("This aspect ratio cannot be preserved with the selected size preset. Choose Original."));return;
    }
    if(m_exportSpeed!=1.&&m_format==QStringLiteral("mp4")&&m_audio&&m_includeAudio&&!m_atempoAvailable) {
        fail(tr("This FFmpeg installation lacks pitch-preserving audio speed support. Choose 1× or remove audio."));return;
    }
    if(startMs<0||startMs>m_duration||endMs<startMs||endMs>m_duration||durationForRange(startMs,endMs)<(m_format==QStringLiteral("gif")?200:100)){fail(m_format==QStringLiteral("gif")?tr("Choose a range inside the recording that produces at least 0.2 seconds at this export speed."):tr("Choose a range inside the recording that produces at least 0.1 seconds at this export speed."));return;}
    if(m_format==QStringLiteral("gif")&&endMs-startMs>15000){fail(tr("GIF export is limited to 15 seconds. Choose a shorter range."));return;}
    QFileInfo f(target.toLocalFile());const QFileInfo source(m_source.toLocalFile());
    if(!target.isLocalFile()||!f.isAbsolute()||f.suffix().compare(m_format,Qt::CaseInsensitive)||f.isSymLink()||!f.dir().exists()||(f.exists()&&!f.isFile())||f.absoluteFilePath()==source.absoluteFilePath()||(!f.canonicalFilePath().isEmpty()&&f.canonicalFilePath()==source.canonicalFilePath())) {fail(tr("Choose a new local file with the selected format’s extension, not the original recording."));return;}
    struct stat inputStat{},outputStat{};
    if(::stat(QFile::encodeName(source.absoluteFilePath()).constData(),&inputStat)==0
        && ::stat(QFile::encodeName(f.absoluteFilePath()).constData(),&outputStat)==0
        && inputStat.st_dev==outputStat.st_dev && inputStat.st_ino==outputStat.st_ino) {
        fail(tr("Choose a different file, not a hard link to the original recording."));return;
    }
    if(f.exists()){fail(tr("The destination already exists. Choose a new filename."));return;}
    m_target=f.absoluteFilePath();m_start=startMs;m_end=endMs;m_clipDuration=outputDurationMs();
    QTemporaryFile temp(f.dir().filePath(QStringLiteral(".nkshot-clip-XXXXXX.%1").arg(m_format)));temp.setAutoRemove(false);
    if(!temp.open()){fail(tr("A temporary clip cannot be created in this folder."));return;}m_temp=temp.fileName();temp.close();
    if(!m_annotationOverlay.isNull()) {
        QTemporaryFile annotation(f.dir().filePath(QStringLiteral(".nkshot-clip-annotation-XXXXXX.png")));
        annotation.setAutoRemove(false);
        if(!annotation.open()){fail(tr("A private annotation image cannot be created."));return;}
        m_annotationFile=annotation.fileName();
        if(!annotation.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner)
            ||!m_annotationOverlay.save(&annotation,"PNG")){annotation.close();fail(tr("The private annotation image could not be saved."));return;}
        annotation.close();
    }
    if(m_format==QStringLiteral("gif")) {
        QTemporaryFile palette(f.dir().filePath(QStringLiteral(".nkshot-clip-palette-XXXXXX.png")));palette.setAutoRemove(false);
        if(!palette.open()){fail(tr("A private GIF palette cannot be created in this folder."));return;}m_palette=palette.fileName();palette.close();
        m_busy=true;const auto epoch=m_epoch;Q_EMIT changed();if(epoch==m_epoch&&m_busy)launchGifPass(true);return;
    }
    QStringList args{QStringLiteral("-hide_banner"),QStringLiteral("-loglevel"),QStringLiteral("error"),QStringLiteral("-nostdin"),QStringLiteral("-xerror"),QStringLiteral("-y"),QStringLiteral("-protocol_whitelist"),QStringLiteral("file"),QStringLiteral("-f"),QStringLiteral("mov"),QStringLiteral("-enable_drefs"),QStringLiteral("0"),QStringLiteral("-use_absolute_path"),QStringLiteral("0"),QStringLiteral("-threads"),QStringLiteral("2"),QStringLiteral("-filter_threads"),QStringLiteral("2"),QStringLiteral("-i"),m_source.toLocalFile(),QStringLiteral("-t"),seconds(m_clipDuration),QStringLiteral("-vf"),QStringLiteral("trim=start=%1:end=%2,setpts=PTS-STARTPTS").arg(seconds(startMs),seconds(endMs)),QStringLiteral("-map"),QStringLiteral("0:v:0")};
    if(m_audio&&m_includeAudio)args<<QStringLiteral("-map")<<QStringLiteral("0:a:0")<<QStringLiteral("-af")<<QStringLiteral("atrim=start=%1:end=%2,asetpts=PTS-STARTPTS").arg(seconds(startMs),seconds(endMs));
    args<<QStringLiteral("-map_metadata")<<QStringLiteral("-1")<<QStringLiteral("-map_chapters")<<QStringLiteral("-1")<<QStringLiteral("-c:v")<<QStringLiteral("libx264")<<QStringLiteral("-preset")<<QStringLiteral("veryfast")<<QStringLiteral("-crf")<<QStringLiteral("20")<<QStringLiteral("-pix_fmt")<<QStringLiteral("yuv420p")<<QStringLiteral("-threads")<<QStringLiteral("2");
    if(m_audio&&m_includeAudio)args<<QStringLiteral("-c:a")<<QStringLiteral("aac")<<QStringLiteral("-b:a")<<QStringLiteral("160k")<<QStringLiteral("-ar")<<QStringLiteral("48000");
    args<<QStringLiteral("-movflags")<<QStringLiteral("+faststart")<<QStringLiteral("-progress")<<QStringLiteral("pipe:1")<<QStringLiteral("-nostats")<<QStringLiteral("-fs")<<QStringLiteral("2147483648")<<QStringLiteral("-f")<<QStringLiteral("mp4")<<m_temp;
    if(m_webm) {
        const int inputEnd=args.indexOf(QStringLiteral("-i"));
        QStringList prefix{QStringLiteral("-hide_banner"),QStringLiteral("-loglevel"),QStringLiteral("error"),QStringLiteral("-nostdin"),QStringLiteral("-xerror"),QStringLiteral("-y"),QStringLiteral("-copyts")};
        prefix+=NkShot::LocalVideoInput::demuxArguments(NkShot::LocalVideoInput::Container::WebM);
        prefix<<QStringLiteral("-threads")<<QStringLiteral("2")<<QStringLiteral("-filter_threads")<<QStringLiteral("2");
        args=prefix+args.mid(inputEnd);
        args[args.indexOf(QStringLiteral("-vf"))+1]=webmVideoFilter(30);
        if(m_audio&&m_includeAudio)args[args.indexOf(QStringLiteral("-af"))+1]=QStringLiteral("atrim=start=%1:end=%2,asetpts=PTS-%1/TB,apad=whole_dur=%3").arg(seconds(m_start),seconds(m_end),seconds(m_end-m_start));
    }
    if(m_exportSpeed!=1.) {
        args[args.indexOf(QStringLiteral("-vf"))+1]+=speedVideoFilter(m_webm?30.:1000./m_frameMs);
        if(m_audio&&m_includeAudio)args[args.indexOf(QStringLiteral("-af"))+1]+=QStringLiteral(",apad=whole_dur=%1,atrim=duration=%1,atempo=%2,apad=whole_dur=%3,atrim=duration=%3,asetpts=PTS-STARTPTS")
            .arg(seconds(m_end-m_start),QString::number(m_exportSpeed,'f',1),seconds(m_clipDuration));
    }
    const int vf=args.indexOf(QStringLiteral("-vf"));
    if(m_annotationOverlay.isNull())args[vf+1]+=privacyFilter()+cropFilter()+mp4ScaleFilter();
    else {
        const auto time=args[vf+1];args.removeAt(vf);args.removeAt(vf);
        const int map=args.indexOf(QStringLiteral("-map"));args[map+1]=QStringLiteral("[annotated]");
        QStringList input;appendAnnotationInput(input);
        const int afterInput=args.indexOf(QStringLiteral("-i"))+2;
        for(int i=0;i<input.size();++i)args.insert(afterInput+i,input[i]);
        args.insert(args.size()-1,QStringLiteral("-filter_complex_threads"));args.insert(args.size()-1,QStringLiteral("2"));
        args.insert(args.size()-1,QStringLiteral("-filter_complex"));
        args.insert(args.size()-1,annotatedFilter(time,privacyFilter()+cropFilter()+mp4ScaleFilter())+QStringLiteral("[annotated]"));
    }
    m_busy=true;const auto epoch=m_epoch;Q_EMIT changed();if(epoch==m_epoch&&m_busy)launch(m_ffmpeg,args,Encode,30*60*1000);
}

void VideoClipJob::setIncludeAudio(bool include) {
    if(m_busy || include==m_includeAudio)return;
    m_includeAudio=include;m_output=QUrl();m_progress=0;m_error.clear();
    if(m_mediaValid&&m_encodersChecked&&m_format==QStringLiteral("mp4")) {
        m_ready=m_x264Available&&(!m_audio||!m_includeAudio||m_aacAvailable)
            &&(!m_privacyEnabled||m_drawboxAvailable)&&(m_annotationOverlay.isNull()||m_overlayAvailable);
        if(!m_ready)m_error=tr("This FFmpeg installation lacks the H.264 or AAC encoder needed for clip export.");
    }
    Q_EMIT changed();
}
qint64 VideoClipJob::durationForRange(qint64 startMs,qint64 endMs) const {
    if(startMs<0||endMs<startMs||endMs>m_duration)return 0;
    return qRound64(double(endMs-startMs)/m_exportSpeed);
}
qint64 VideoClipJob::outputDurationMs() const { return durationForRange(m_start,m_end); }
void VideoClipJob::setExportRange(qint64 startMs,qint64 endMs) {
    if(m_busy||!m_ready||startMs<0||endMs<=startMs||endMs>m_duration||(startMs==m_start&&endMs==m_end))return;
    m_start=startMs;m_end=endMs;m_output=QUrl();m_progress=0;m_error.clear();Q_EMIT changed();
}
void VideoClipJob::setExportSpeed(double speed) {
    if(m_busy||!m_ready||speed==m_exportSpeed||(speed!=1.&&speed!=1.5&&speed!=2.))return;
    m_exportSpeed=speed;m_output=QUrl();m_progress=0;m_error.clear();Q_EMIT changed();
}
QString VideoClipJob::speedVideoFilter(double fps) const {
    if(m_exportSpeed==1.)return {};
    return QStringLiteral(",setpts=PTS/%1,tpad=stop_mode=clone:stop_duration=0.2,fps=%2,trim=duration=%3,setpts=PTS-STARTPTS")
        .arg(QString::number(m_exportSpeed,'f',1),QString::number(fps,'g',12),seconds(m_clipDuration));
}
void VideoClipJob::setOutputResolution(const QString &resolution) {
    if(m_busy||!m_ready||resolution==m_outputResolution)return;
    if(resolution!=QStringLiteral("original")&&resolution!=QStringLiteral("1080p")&&resolution!=QStringLiteral("720p"))return;
    m_outputResolution=resolution;m_output=QUrl();m_progress=0;m_error.clear();Q_EMIT changed();
}
QSize VideoClipJob::outputSize() const {
    const auto size=cropRect().size();
    if(!m_mediaValid||size.isEmpty())return {};
    const int longest=qMax(size.width(),size.height());
    if(m_format==QStringLiteral("gif")) {
        const double factor=qMin(1.,640./longest);
        return QSize(qMax(1,qRound(size.width()*factor)),qMax(1,qRound(size.height()*factor)));
    }
    const int limit=m_outputResolution==QStringLiteral("720p")?1280:m_outputResolution==QStringLiteral("1080p")?1920:longest;
    if(longest<=limit)return size;
    // Integer arithmetic rounds inward to legal 4:2:0 dimensions, never upscales.
    const int width=int(qint64(size.width())*limit/longest)&~1;
    const int height=int(qint64(size.height())*limit/longest)&~1;
    // Never stretch an extreme aspect ratio merely to meet encoder minima.
    if(width<2||height<2)return {};
    return QSize(width,height);
}
QString VideoClipJob::mp4ScaleFilter() const {
    const auto size=outputSize();if(size==cropRect().size())return {};
    return QStringLiteral(",scale=%1:%2:flags=lanczos").arg(size.width()).arg(size.height())
        +(m_squarePixels?QStringLiteral(",setsar=1"):QString());
}
void VideoClipJob::setOutputFormat(const QString &format) {
    if(format!=QStringLiteral("mp4")&&format!=QStringLiteral("gif"))return;
    if(format==m_format)return;
    stop();m_format=format;m_ready=false;m_error.clear();m_output=QUrl();m_progress=0;
    if(m_mediaValid&&sourceUnchanged()) {
        m_busy=true;const auto epoch=m_epoch;Q_EMIT changed();if(epoch==m_epoch&&m_busy)launch(m_ffmpeg,{QStringLiteral("-hide_banner"),QStringLiteral("-encoders")},Encoders,15000);
    } else if(!m_source.isEmpty()) {const auto source=m_source;inspect(source);}
    else Q_EMIT changed();
}
bool VideoClipJob::setAnnotationOverlay(const QImage &image) {
    if(m_busy||!m_ready)return false;
    if(!image.isNull()&&(!annotationAvailable()||image.size()!=sourceSize()))return false;
    QImage normalized;
    if(!image.isNull()) {normalized=image.convertToFormat(QImage::Format_ARGB32_Premultiplied).copy();normalized.setDevicePixelRatio(1);if(normalized.isNull())return false;}
    if(normalized==m_annotationOverlay)return true;
    m_annotationOverlay=normalized;++m_annotationRevision;m_output=QUrl();m_progress=0;m_error.clear();Q_EMIT changed();return true;
}
void VideoClipJob::appendAnnotationInput(QStringList &args) const {
    args<<QStringLiteral("-protocol_whitelist")<<QStringLiteral("file")<<QStringLiteral("-f")<<QStringLiteral("image2")
        <<QStringLiteral("-pattern_type")<<QStringLiteral("none")<<QStringLiteral("-c:v")<<QStringLiteral("png")
        <<QStringLiteral("-threads")<<QStringLiteral("2")<<QStringLiteral("-i")<<m_annotationFile;
}
QString VideoClipJob::annotatedFilter(const QString &time,const QString &tail) const {
    return QStringLiteral("[0:v:0]")+time+QStringLiteral("[base];[base][1:v:0]overlay=eof_action=repeat:repeatlast=1:format=auto")+tail;
}
void VideoClipJob::setPrivacyEnabled(bool enabled) {
    if(m_busy||!m_ready||enabled==m_privacyEnabled)return;
    if(enabled&&!privacyAvailable()) {
        m_error=tr("Opaque privacy cover needs square pixels and the FFmpeg drawbox filter.");Q_EMIT changed();return;
    }
    if(enabled&&m_privacyRect.isEmpty()) {
        const int width=qMax(2,(m_width/3)&~1),height=qMax(2,(m_height/3)&~1);
        m_privacyRect=QRect(((m_width-width)/2)&~1,((m_height-height)/2)&~1,width,height);
    }
    m_privacyEnabled=enabled;m_output=QUrl();m_progress=0;m_error.clear();Q_EMIT changed();
}
void VideoClipJob::setPrivacyRect(int x,int y,int width,int height) {
    if(m_busy||!m_ready||!m_privacyEnabled||!privacyAvailable())return;
    // Clamp coordinates before subtraction/addition, including INT extrema.
    const int left=qBound(0,x,m_width-2)&~1,top=qBound(0,y,m_height-2)&~1;
    const QRect rect(left,top,qBound(2,width,m_width-left)&~1,qBound(2,height,m_height-top)&~1);
    if(rect==m_privacyRect)return;
    m_privacyRect=rect;m_output=QUrl();m_progress=0;m_error.clear();Q_EMIT changed();
}
QString VideoClipJob::privacyFilter() const {
    if(!m_privacyEnabled)return {};
    return QStringLiteral(",drawbox=x=%1:y=%2:w=%3:h=%4:color=black@1:t=fill")
        .arg(m_privacyRect.x()).arg(m_privacyRect.y()).arg(m_privacyRect.width()).arg(m_privacyRect.height());
}
QRect VideoClipJob::centeredCropRect() const {
    if(!m_mediaValid||m_width<2||m_height<2)return {};
    if(m_cropAspect==QStringLiteral("original"))return QRect(0,0,m_width,m_height);
    int numerator=1,denominator=1;
    if(m_cropAspect==QStringLiteral("portrait")){numerator=9;denominator=16;}
    else if(m_cropAspect==QStringLiteral("wide")){numerator=16;denominator=9;}
    int width=m_width,height=m_height;
    // Keep source pixels, never stretch. Round inward to even dimensions and
    // even offsets for YUV420 chroma alignment (centre bias at most one pixel).
    if(m_width*denominator>m_height*numerator)width=qBound(2,(m_height*numerator/denominator)&~1,m_width);
    else height=qBound(2,(m_width*denominator/numerator)&~1,m_height);
    return QRect(((m_width-width)/2)&~1,((m_height-height)/2)&~1,width,height);
}
QRect VideoClipJob::cropRect() const {
    auto rect=centeredCropRect();
    if(!rect.isEmpty()&&m_cropAspect!=QStringLiteral("original")&&m_cropPosition.x()>=0&&m_cropPosition.y()>=0)
        rect.moveTopLeft(m_cropPosition);
    return rect;
}
bool VideoClipJob::cropCentered() const { return cropRect()==centeredCropRect(); }
void VideoClipJob::setCropPosition(int x,int y) {
    if(m_busy||!m_ready||!cropAvailable()||m_cropAspect==QStringLiteral("original"))return;
    const auto rect=cropRect();
    // Clamp before arithmetic, including INT_MIN/INT_MAX input from callers.
    // The final even offsets keep both image and YUV420 chroma inside source.
    const QPoint position(qBound(0,x,m_width-rect.width())&~1,qBound(0,y,m_height-rect.height())&~1);
    if(position==rect.topLeft())return;
    m_cropPosition=position==centeredCropRect().topLeft()?QPoint(-1,-1):position;
    m_output=QUrl();m_progress=0;m_error.clear();Q_EMIT changed();
}
void VideoClipJob::resetCropPosition() {
    if(m_busy||!m_ready||!cropAvailable()||m_cropAspect==QStringLiteral("original")||cropCentered())return;
    m_cropPosition=QPoint(-1,-1);m_output=QUrl();m_progress=0;m_error.clear();Q_EMIT changed();
}
void VideoClipJob::setCropAspect(const QString &aspect) {
    if(m_busy||!m_ready||aspect==m_cropAspect)return;
    if(aspect!=QStringLiteral("original")&&!cropAvailable())return;
    if(aspect!=QStringLiteral("original")&&aspect!=QStringLiteral("square")
        &&aspect!=QStringLiteral("portrait")&&aspect!=QStringLiteral("wide"))return;
    m_cropAspect=aspect;m_cropPosition=QPoint(-1,-1);m_output=QUrl();m_progress=0;m_error.clear();Q_EMIT changed();
}
QString VideoClipJob::cropFilter() const {
    if(m_cropAspect==QStringLiteral("original"))return {};
    const auto rect=cropRect();
    return QStringLiteral(",crop=%1:%2:%3:%4").arg(rect.width()).arg(rect.height()).arg(rect.x()).arg(rect.y());
}
QString VideoClipJob::gifFilter() const {
    const auto rect=cropRect();
    const double factor=qMin(1.,640./qMax(rect.width(),rect.height()));
    const QString range=(m_webm ? webmVideoFilter(m_exportSpeed==1.?10:30) : QStringLiteral("trim=start=0:end=%1,setpts=PTS-STARTPTS").arg(seconds(m_end-m_start))
        +(m_exportSpeed==1.?QStringLiteral(",fps=10"):QString()))+speedVideoFilter(10);
    const auto tail=privacyFilter()+cropFilter()+QStringLiteral(",scale=%1:%2:flags=lanczos").arg(qMax(1,qRound(rect.width()*factor))).arg(qMax(1,qRound(rect.height()*factor)));
    return m_annotationOverlay.isNull()?range+tail:annotatedFilter(range,tail);
}
void VideoClipJob::launchGifPass(bool palette) {
    QStringList args{QStringLiteral("-hide_banner"),QStringLiteral("-loglevel"),QStringLiteral("error"),QStringLiteral("-nostdin"),QStringLiteral("-xerror"),QStringLiteral("-y"),QStringLiteral("-protocol_whitelist"),QStringLiteral("file"),QStringLiteral("-f"),QStringLiteral("mov"),QStringLiteral("-enable_drefs"),QStringLiteral("0"),QStringLiteral("-use_absolute_path"),QStringLiteral("0"),QStringLiteral("-threads"),QStringLiteral("2"),QStringLiteral("-filter_threads"),QStringLiteral("2"),QStringLiteral("-filter_complex_threads"),QStringLiteral("2"),QStringLiteral("-ss"),seconds(m_start),QStringLiteral("-accurate_seek"),QStringLiteral("-t"),seconds(m_end-m_start),QStringLiteral("-i"),m_source.toLocalFile()};
    if(m_webm) {
        args={QStringLiteral("-hide_banner"),QStringLiteral("-loglevel"),QStringLiteral("error"),QStringLiteral("-nostdin"),QStringLiteral("-xerror"),QStringLiteral("-y"),QStringLiteral("-copyts")};
        args+=NkShot::LocalVideoInput::demuxArguments(NkShot::LocalVideoInput::Container::WebM);
        args<<QStringLiteral("-threads")<<QStringLiteral("2")<<QStringLiteral("-filter_threads")<<QStringLiteral("2")<<QStringLiteral("-filter_complex_threads")<<QStringLiteral("2")<<QStringLiteral("-i")<<m_source.toLocalFile();
    }
    if(!m_annotationOverlay.isNull())appendAnnotationInput(args);
    if(palette) {
        args<<(m_annotationOverlay.isNull()?QStringLiteral("-vf"):QStringLiteral("-filter_complex"))<<(gifFilter()+QStringLiteral(",palettegen=max_colors=256:stats_mode=full:reserve_transparent=0"))<<QStringLiteral("-an")<<QStringLiteral("-frames:v")<<QStringLiteral("1")<<QStringLiteral("-c:v")<<QStringLiteral("png")<<QStringLiteral("-threads")<<QStringLiteral("2")<<QStringLiteral("-update")<<QStringLiteral("1")<<QStringLiteral("-f")<<QStringLiteral("image2");
    } else {
        args<<QStringLiteral("-protocol_whitelist")<<QStringLiteral("file")<<QStringLiteral("-f")<<QStringLiteral("image2")<<QStringLiteral("-c:v")<<QStringLiteral("png")<<QStringLiteral("-threads")<<QStringLiteral("2")<<QStringLiteral("-i")<<m_palette;
        args<<QStringLiteral("-filter_complex")<<((m_annotationOverlay.isNull()?QStringLiteral("[0:v:0]"):QString())+gifFilter()+QStringLiteral("[clip];[clip][%1:v:0]paletteuse=dither=sierra2_4a[gif]").arg(m_annotationOverlay.isNull()?1:2))<<QStringLiteral("-map")<<QStringLiteral("[gif]")<<QStringLiteral("-an")<<QStringLiteral("-t")<<seconds(m_clipDuration)<<QStringLiteral("-c:v")<<QStringLiteral("gif")<<QStringLiteral("-threads")<<QStringLiteral("2")<<QStringLiteral("-loop")<<QStringLiteral("0")<<QStringLiteral("-fs")<<QStringLiteral("20971520")<<QStringLiteral("-f")<<QStringLiteral("gif");
    }
    args<<QStringLiteral("-map_metadata")<<QStringLiteral("-1")<<QStringLiteral("-map_chapters")<<QStringLiteral("-1")<<QStringLiteral("-progress")<<QStringLiteral("pipe:1")<<QStringLiteral("-nostats")<<(palette?m_palette:m_temp);
    launch(m_ffmpeg,args,palette?Palette:Encode,5*60*1000);
}

QString VideoClipJob::webmVideoFilter(int fps) const {
    // Decode the prefix deliberately: the last frame before a range may still
    // be the current desktop image. Never discard it with accurate input seek.
    // Extend the final held frame, then resample on the common zero timeline.
    return QStringLiteral("tpad=stop_mode=clone:stop_duration=%1,fps=%2:start_time=0:round=down,trim=start=%3:end=%4,setpts=PTS-%3/TB")
        .arg(seconds(m_end)).arg(fps).arg(seconds(m_start),seconds(m_end));
}
