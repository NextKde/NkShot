// SPDX-License-Identifier: GPL-3.0-only
#include "LocalMediaLoader.h"
#include "Media/LocalVideoInput.h"
#include <KLocalizedString>
#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>
#include <QtEndian>
#include <atomic>
#include <cmath>
#include <memory>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
namespace {
constexpr qint64 ImageBytes = 64LL * 1024 * 1024;
constexpr qint64 VideoBytes = 256LL * 1024 * 1024;
constexpr qint64 ImagePixels = 40000000;
// QImageReader cannot be forcibly interrupted safely. A cancelled read may
// finish in the background, but can never accumulate parallel decoder workers.
std::atomic_bool imageWorkerActive = false;
qint64 modifiedNs(const struct stat &st) { return qint64(st.st_mtim.tv_sec)*1000000000LL+st.st_mtim.tv_nsec; }
bool regular(const QString &path, struct stat &st) {
    return ::lstat(QFile::encodeName(path).constData(),&st)==0 && S_ISREG(st.st_mode) && st.st_size>0;
}
bool animatedContainer(const QByteArray &bytes) {
    if (bytes.startsWith(QByteArray::fromHex("89504e470d0a1a0a"))) {
        qsizetype offset=8;
        while (offset+12<=bytes.size()) {
            const auto length=qFromBigEndian<quint32>(bytes.constData()+offset);
            if (quint64(length)+12>quint64(bytes.size()-offset)) return true;
            const auto type=bytes.mid(offset+4,4);
            if(type=="acTL") return true;
            offset+=qsizetype(length)+12;
            if(type=="IEND") break;
        }
    } else if(bytes.startsWith("RIFF") && bytes.mid(8,4)=="WEBP") {
        qsizetype offset=12;
        while(offset+8<=bytes.size()) {
            const auto length=qFromLittleEndian<quint32>(bytes.constData()+offset+4);
            if(quint64(length)+8>quint64(bytes.size()-offset)) return true;
            const auto type=bytes.mid(offset,4);
            if(type=="ANIM" || type=="ANMF") return true;
            offset+=8+qsizetype(length)+(length&1);
        }
    }
    return false;
}
struct ImageWork {
    QImage image;
    QString error;
    QString path;
    quint64 device=0,inode=0;
    qint64 size=0,modified=0;
};
}
namespace NkShot {
LocalMediaLoader::LocalMediaLoader(QObject *parent):QObject(parent) {
    m_timeout.setSingleShot(true);
    connect(&m_timeout,&QTimer::timeout,this,[this] { fail(i18nc("@info local media opening error", "Opening this file took too long. Try a smaller file.")); });
}
LocalMediaLoader::~LocalMediaLoader() { stop(); }
void LocalMediaLoader::stop() {
    ++m_epoch;
    m_validated=false;
    m_timeout.stop();
    if(m_process) {
        auto process=m_process.data(); m_process=nullptr;
        process->disconnect(this);
        // QProcess must outlive kill completion; do not block the GUI waiting.
        process->setParent(nullptr);
        connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),process,&QObject::deleteLater);
        if(process->state()==QProcess::NotRunning) process->deleteLater();
        else process->kill();
    }
    m_busy=false;
}
void LocalMediaLoader::cancel() {
    stop(); m_error.clear(); m_source=QUrl();
    Q_EMIT changed();
}
void LocalMediaLoader::fail(const QString &message) {
    stop(); m_error=message;
    Q_EMIT changed();
}
bool LocalMediaLoader::validatedSourceUnchanged() const {
    return m_validated && !m_busy && unchanged();
}
bool LocalMediaLoader::unchanged() const {
    struct stat st{};
    return regular(m_source.toLocalFile(),st) && quint64(st.st_dev)==m_device && quint64(st.st_ino)==m_inode
        && st.st_size==m_size && modifiedNs(st)==m_modifiedNs;
}
void LocalMediaLoader::load(const QUrl &url) {
    stop(); m_error.clear(); m_source=QUrl(); m_output.clear(); m_diagnostics.clear();
    m_durationMs=0; m_audio=false; m_dimensions={}; m_fps=0; m_webm=false; m_startSeconds=0;
    if(!url.isValid() || !url.isLocalFile() || !url.host().isEmpty() || url.hasQuery() || url.hasFragment()
        || !QFileInfo(url.toLocalFile()).isAbsolute() || url.toLocalFile().contains(QChar::Null)) {
        fail(i18nc("@info local media opening error", "Choose a local PNG, JPEG, static WebP, H.264 MP4 or VP9 WebM file.")); return;
    }
    // Reject special files before any read. Canonicalization allows a selected
    // symlink, but subsequent identity checks refer to its resolved regular file.
    const auto path=QFileInfo(url.toLocalFile()).canonicalFilePath();
    struct stat st{};
    if(path.isEmpty() || !regular(path,st) || st.st_size>VideoBytes) {
        fail(i18nc("@info local media opening error", "Choose a readable regular file, up to 64 MiB for images or 256 MiB for video.")); return;
    }
    m_source=QUrl::fromLocalFile(path); m_device=st.st_dev; m_inode=st.st_ino;
    m_size=st.st_size; m_modifiedNs=modifiedNs(st);
    const auto suffix=QFileInfo(path).suffix().toLower();
    m_webm=suffix==QStringLiteral("webm");
    const bool video=m_webm || suffix==QStringLiteral("mp4");
    if(m_webm && !LocalVideoInput::hasWebMDocType(path)) {
        fail(i18nc("@info local media opening error", "Choose a genuine WebM file. Renamed Matroska files are not supported.")); return;
    }
    if(!video && st.st_size>ImageBytes) { fail(i18nc("@info local media opening error", "Images must be 64 MiB or smaller.")); return; }
    m_busy=true;
    const auto epoch=m_epoch;
    const QPointer<LocalMediaLoader> guard(this);
    Q_EMIT changed();
    if(!guard || epoch!=m_epoch || !m_busy) return;
    if(!video) { startImage(epoch); return; }
    m_ffmpeg=QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    const auto probe=QStandardPaths::findExecutable(QStringLiteral("ffprobe"));
    if(m_ffmpeg.isEmpty() || probe.isEmpty()) {
        fail(i18nc("@info local media opening error", "Opening video needs FFmpeg and ffprobe from your system package manager. No software was installed.")); return;
    }
    QStringList args{QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-max_alloc"),QStringLiteral("134217728"),QStringLiteral("-threads"),QStringLiteral("2")}; args+=LocalVideoInput::demuxArguments(m_webm ? LocalVideoInput::Container::WebM : LocalVideoInput::Container::Mp4);
    args << QStringLiteral("-show_streams") << QStringLiteral("-show_format") << QStringLiteral("-of") << QStringLiteral("json") << path;
    launch(probe,args,Stage::Probe);
}
void LocalMediaLoader::startImage(quint64 epoch) {
    if(imageWorkerActive.exchange(true)) {
        fail(i18nc("@info local media opening error", "The previous image is still finishing validation. Try opening the file again in a moment.")); return;
    }
    auto work=std::make_shared<ImageWork>();
    work->path=m_source.toLocalFile(); work->device=m_device; work->inode=m_inode;
    work->size=m_size; work->modified=m_modifiedNs;
    auto worker=QThread::create([work] {
        struct Reset { ~Reset() { imageWorkerActive=false; } } reset;
        const int fd=::open(QFile::encodeName(work->path).constData(),O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
        struct stat st{};
        if(fd<0 || ::fstat(fd,&st)!=0 || !S_ISREG(st.st_mode) || quint64(st.st_dev)!=work->device
            || quint64(st.st_ino)!=work->inode || st.st_size!=work->size || modifiedNs(st)!=work->modified) {
            if(fd>=0) ::close(fd);
            work->error=QStringLiteral("changed"); return;
        }
        QFile file;
        if(!file.open(fd,QIODevice::ReadOnly,QFileDevice::AutoCloseHandle)) { ::close(fd); work->error=QStringLiteral("read"); return; }
        auto bytes=file.read(ImageBytes+1);
        if(bytes.size()!=work->size || file.error()!=QFileDevice::NoError) { work->error=QStringLiteral("read"); return; }
        if(animatedContainer(bytes)) { work->error=QStringLiteral("animated"); return; }
        QBuffer buffer(&bytes); buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer); reader.setDecideFormatFromContent(true); reader.setAutoTransform(true);
        const auto format=reader.format().toLower();
        if(format!="png" && format!="jpeg" && format!="jpg" && format!="webp") { work->error=QStringLiteral("format"); return; }
        const auto size=reader.size();
        if(size.width()<1 || size.height()<1 || size.width()>32768 || size.height()>32768
            || qint64(size.width())*size.height()>ImagePixels) { work->error=QStringLiteral("size"); return; }
        if(reader.imageCount()>1) { work->error=QStringLiteral("animated"); return; }
        // Respect Qt's existing process allocation policy, including
        // QT_IMAGEIO_MAXALLOC. Never mutate it from this background worker.
        work->image=reader.read();
        if(work->image.isNull()) work->error=QStringLiteral("decode");
        else if(work->image.sizeInBytes()>192LL*1024*1024) { work->image={}; work->error=QStringLiteral("size"); }
    });
    connect(worker,&QThread::finished,worker,&QObject::deleteLater);
    connect(worker,&QThread::finished,this,[this,work,epoch] {
        if(epoch!=m_epoch || !m_busy) return;
        m_timeout.stop();
        if(!unchanged()) { fail(i18nc("@info local media opening error", "The file changed while it was being opened. Select it again.")); return; }
        if(!work->error.isEmpty()) {
            if(work->error==QStringLiteral("animated")) fail(i18nc("@info local media opening error", "Animated images are not supported here. Choose a still PNG, JPEG or WebP."));
            else if(work->error==QStringLiteral("size")) fail(i18nc("@info local media opening error", "Choose an image up to 40 megapixels and 32,768 pixels on either side, using at most 192 MiB decoded."));
            else fail(i18nc("@info local media opening error", "This file could not be decoded as a still PNG, JPEG or WebP image."));
            return;
        }
        m_busy=false; m_validated=true;
        const auto source=m_source; const auto image=work->image;
        const QPointer<LocalMediaLoader> guard(this);
        Q_EMIT changed();
        if(guard && epoch==m_epoch) Q_EMIT imageReady(image,source);
    });
    m_timeout.start(15000);
    worker->start();
}
void LocalMediaLoader::launch(const QString &program,const QStringList &arguments,Stage stage) {
    m_output.clear(); m_diagnostics.clear();
    auto process=new QProcess(this); m_process=process;
    const auto epoch=m_epoch;
    connect(process,&QProcess::readyReadStandardOutput,this,[this,process,epoch] {
        if(epoch!=m_epoch || m_process!=process) return;
        m_output+=process->readAllStandardOutput();
        if(m_output.size()>1024*1024) fail(i18nc("@info local media opening error", "This file returned too much media metadata to open safely."));
    });
    connect(process,&QProcess::readyReadStandardError,this,[this,process,epoch] {
        if(epoch!=m_epoch || m_process!=process) return;
        m_diagnostics+=process->readAllStandardError();
        if(m_diagnostics.size()>16384) m_diagnostics=m_diagnostics.right(16384);
    });
    connect(process,&QProcess::errorOccurred,this,[this,process,epoch](QProcess::ProcessError error) {
        if(epoch==m_epoch && m_process==process && error==QProcess::FailedToStart)
            fail(i18nc("@info local media opening error", "The media validation tool could not start. Check your FFmpeg installation."));
    });
    connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,process,epoch,stage](int code,QProcess::ExitStatus status) {
        if(epoch!=m_epoch || m_process!=process) return;
        m_output+=process->readAllStandardOutput();
        m_diagnostics+=process->readAllStandardError();
        m_timeout.stop(); m_process=nullptr; process->deleteLater();
        if(code!=0 || status!=QProcess::NormalExit || m_output.size()>1024*1024) {
            fail(i18nc("@info local media opening error", "The video could not be fully decoded. Choose another local H.264 MP4 or VP9 WebM recording.")); return;
        }
        completed(stage);
    });
    m_timeout.start(stage==Stage::Probe ? 15000 : 60000);
    process->start(program,arguments,QIODevice::ReadOnly);
}
void LocalMediaLoader::completed(Stage stage) {
    if(!unchanged()) { fail(i18nc("@info local media opening error", "The file changed while it was being opened. Select it again.")); return; }
    if(stage==Stage::Probe) {
        const auto root=QJsonDocument::fromJson(m_output).object();
        const auto format=root.value(QStringLiteral("format")).toObject();
        bool durationOk=false;
        const double duration=format.value(QStringLiteral("duration")).toString().toDouble(&durationOk);
        bool valid=durationOk && std::isfinite(duration) && duration>0 && duration<=600
            && format.value(QStringLiteral("format_name")).toString().split(u',').contains(m_webm ? QStringLiteral("webm") : QStringLiteral("mp4"));
        bool startOk=false;
        const auto start=format.value(QStringLiteral("start_time")).toString().toDouble(&startOk);
        m_startSeconds=start;
        valid=valid && startOk && std::isfinite(start)
            && (m_webm ? start>=-.080 && start<=.001 : std::abs(start)<=.001);
        int video=0,audio=0;
        for(const auto &item:root.value(QStringLiteral("streams")).toArray()) {
            const auto stream=item.toObject();
            const auto type=stream.value(QStringLiteral("codec_type")).toString();
            bool offsetOk=false;
            const double offset=stream.value(QStringLiteral("start_time")).toString().toDouble(&offsetOk);
            valid=valid && offsetOk && std::isfinite(offset);
            if(!m_webm) valid=valid && std::abs(offset)<=.001;
            if(type==QStringLiteral("video")) {
                ++video;
                if(m_webm) valid=valid && offset>=-.001 && offset<=.080;
                m_dimensions={stream.value(QStringLiteral("width")).toInt(),stream.value(QStringLiteral("height")).toInt()};
                const auto pixel=stream.value(QStringLiteral("pix_fmt")).toString();
                const auto rate=stream.value(QStringLiteral("avg_frame_rate")).toString().split(u'/');
                m_fps=rate.size()==2 && rate[1].toDouble()>0 ? rate[0].toDouble()/rate[1].toDouble() : 0;
                bool webmRatesValid=true;
                if(m_webm) {
                    // KPipeWire emits sparse/VFR WebM. Unknown 0/0 is not a
                    // malformed file and nominal fps does not imply a CFR count.
                    for(const auto &key:{QStringLiteral("avg_frame_rate"),QStringLiteral("r_frame_rate")}) {
                        const auto text=stream.value(key).toString();
                        if(text.isEmpty() || text==QStringLiteral("0/0")) continue;
                        const auto parts=text.split(u'/');
                        bool numeratorOk=false,denominatorOk=false;
                        const double numerator=parts.value(0).toDouble(&numeratorOk);
                        const double denominator=parts.value(1).toDouble(&denominatorOk);
                        if(parts.size()!=2 || !numeratorOk || !denominatorOk || !std::isfinite(numerator)
                            || !std::isfinite(denominator) || numerator<0 || denominator<=0 || numerator/denominator>60)
                            webmRatesValid=false;
                    }
                }
                valid=valid && stream.value(QStringLiteral("codec_name")).toString()==(m_webm ? QStringLiteral("vp9") : QStringLiteral("h264"))
                    && (pixel==QStringLiteral("yuv420p") || (!m_webm && pixel==QStringLiteral("yuvj420p")))
                    && m_dimensions.width()>=2 && m_dimensions.height()>=2 && m_dimensions.width()<=4096 && m_dimensions.height()<=4096
                    && m_dimensions.width()%2==0 && m_dimensions.height()%2==0
                    && qint64(m_dimensions.width())*m_dimensions.height()<=8294400
                    && (m_webm ? webmRatesValid : std::isfinite(m_fps) && m_fps>0 && m_fps<=60)
                    && stream.value(QStringLiteral("disposition")).toObject().value(QStringLiteral("attached_pic")).toInt()==0;
                const auto transfer=stream.value(QStringLiteral("color_transfer")).toString();
                valid=valid && transfer!=QStringLiteral("smpte2084") && transfer!=QStringLiteral("arib-std-b67");
                for(const auto &side:stream.value(QStringLiteral("side_data_list")).toArray())
                    if(side.toObject().value(QStringLiteral("rotation")).toDouble()!=0) valid=false;
            } else if(type==QStringLiteral("audio")) {
                ++audio;
                if(m_webm) valid=valid && offset>=-.080 && offset<=.001;
                const int channels=stream.value(QStringLiteral("channels")).toInt();
                const int rate=stream.value(QStringLiteral("sample_rate")).toString().toInt();
                valid=valid && stream.value(QStringLiteral("codec_name")).toString()==(m_webm ? QStringLiteral("opus") : QStringLiteral("aac"))
                    && channels>=1 && channels<=2 && (m_webm ? rate==48000 : rate>=8000 && rate<=192000);
                if(m_webm) {
                    const int padding=stream.value(QStringLiteral("initial_padding")).toInt(0);
                    valid=valid && padding>=0 && padding<=3840;
                }
            } else valid=false;
        }
        if(!valid || video!=1 || audio>1) {
            fail(i18nc("@info local media opening error", "Choose an SDR H.264 MP4 with optional AAC audio, or a genuine VP9 WebM with optional Opus audio. Both need one video track and at most one mono/stereo audio track, up to 10 minutes, 60 fps and 4K. Rotated, HDR and extra-track files are not supported yet.")); return;
        }
        // FFmpeg normalizes negative Opus priming starts when decoding without
        // copyts. Account for that bounded shift; it is not a zero-start MP4.
        m_audio=audio==1; m_durationMs=qRound64(duration*1000);
        QStringList args{QStringLiteral("-hide_banner"),QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-nostdin"),QStringLiteral("-xerror"),QStringLiteral("-max_alloc"),QStringLiteral("134217728")};
        args+=LocalVideoInput::demuxArguments(m_webm ? LocalVideoInput::Container::WebM : LocalVideoInput::Container::Mp4);
        args<<QStringLiteral("-threads")<<QStringLiteral("2")<<QStringLiteral("-filter_threads")<<QStringLiteral("2")
            <<QStringLiteral("-i")<<m_source.toLocalFile()<<QStringLiteral("-map")<<QStringLiteral("0:v:0")
            <<QStringLiteral("-map")<<QStringLiteral("0:a:0?")<<QStringLiteral("-progress")<<QStringLiteral("pipe:1")
            <<QStringLiteral("-nostats")<<QStringLiteral("-fps_mode")<<QStringLiteral("passthrough")<<QStringLiteral("-f")<<QStringLiteral("null")<<QStringLiteral("-");
        launch(m_ffmpeg,args,Stage::Decode); return;
    }
    qint64 frames=0,us=-1;
    for(const auto &line:m_output.split('\n')) {
        if(line.startsWith("frame=")) frames=line.mid(6).trimmed().toLongLong();
        if(line.startsWith("out_time_us=")) us=line.mid(12).trimmed().toLongLong();
    }
    const double decodedMs=double(us)/1000;
    const double expectedMs=m_durationMs-(m_webm ? m_startSeconds*1000 : 0);
    const bool timingValid=m_webm
        ? decodedMs<=expectedMs+150 && (!m_audio || std::abs(decodedMs-expectedMs)<=150)
        : std::abs(decodedMs-expectedMs)<=qMax(150.,2000./m_fps);
    if(frames<1 || frames>36001 || us<0 || !timingValid) {
        fail(i18nc("@info local media opening error", "The decoded recording did not match its duration. Choose another MP4 or WebM.")); return;
    }
    m_busy=false; m_validated=true;
    const auto epoch=m_epoch; const auto source=m_source; const auto size=m_dimensions;
    const auto duration=m_durationMs; const auto audio=m_audio;
    const QPointer<LocalMediaLoader> guard(this);
    Q_EMIT changed();
    if(guard && epoch==m_epoch) Q_EMIT videoReady(source,duration,size,audio);
}
}
