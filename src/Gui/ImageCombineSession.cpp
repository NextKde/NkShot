// SPDX-License-Identifier: GPL-3.0-only
#include "ImageCombineSession.h"
#include "Imaging/VerticalStitcher.h"
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QFutureWatcher>
#include <QtConcurrentRun>
#include <QImageReader>
#include <QFileInfo>
#include <QFile>
#include <QBuffer>
#include <QCollator>
#include <QLocale>
#include <QtEndian>
#include <QColorSpace>
#include <QMutex>
#include <QMutexLocker>
#include <QThreadPool>
#include <QHash>
#include <algorithm>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
namespace {
QThreadPool *combinePool() {
    static QThreadPool pool;
    static const bool configured=[] {pool.setMaxThreadCount(1);return true;}();
    Q_UNUSED(configured);return &pool;
}
// Inspect bounded container chunks, not decoder animation capability (some PNG
// plugins silently return the APNG default image). Malformed chunk sizes fail closed.
bool animatedOrMalformed(const QByteArray &bytes) {
    if(bytes.startsWith(QByteArray::fromHex("89504e470d0a1a0a"))) {
        qsizetype offset=8;
        while(offset+12<=bytes.size()) {
            const auto length=qFromBigEndian<quint32>(bytes.constData()+offset);
            if(quint64(length)+12>quint64(bytes.size()-offset))return true;
            const auto type=bytes.mid(offset+4,4);
            if(type==QByteArrayLiteral("acTL"))return true;
            offset+=qsizetype(length)+12;
            if(type==QByteArrayLiteral("IEND"))return false;
        }
        return true;
    }
    if(bytes.startsWith(QByteArrayLiteral("RIFF"))&&bytes.mid(8,4)==QByteArrayLiteral("WEBP")) {
        if(bytes.size()<12||quint64(qFromLittleEndian<quint32>(bytes.constData()+4))+8!=quint64(bytes.size()))return true;
        qsizetype offset=12;
        while(offset+8<=bytes.size()) {
            const auto length=qFromLittleEndian<quint32>(bytes.constData()+offset+4);
            const quint64 chunkSize=quint64(length)+8+(length&1);
            if(chunkSize>quint64(bytes.size()-offset))return true;
            const auto type=bytes.mid(offset,4);
            if(type==QByteArrayLiteral("ANIM")||type==QByteArrayLiteral("ANMF"))return true;
            offset+=qsizetype(chunkSize);
        }
        return offset!=bytes.size();
    }
    return false;
}
QImage preview(const QImage &image,int bound) {
    auto p=image.colorSpace().isValid()?image.convertedToColorSpace(QColorSpace::SRgb):image;
    if(p.isNull())return {};
    p.setDevicePixelRatio(1);
    if(qMax(p.width(),p.height())>bound)p=p.scaled(bound,bound,Qt::KeepAspectRatio,Qt::SmoothTransformation);
    return p;
}
bool same(const struct stat &a,const struct stat &b) {
    return a.st_dev==b.st_dev&&a.st_ino==b.st_ino&&a.st_size==b.st_size
        &&a.st_mtim.tv_sec==b.st_mtim.tv_sec&&a.st_mtim.tv_nsec==b.st_mtim.tv_nsec
        &&a.st_ctim.tv_sec==b.st_ctim.tv_sec&&a.st_ctim.tv_nsec==b.st_ctim.tv_nsec;
}
}
class CombineImageProvider final:public QQuickImageProvider {
public:
    CombineImageProvider():QQuickImageProvider(Image){}
    void replace(QHash<QString,QImage> images){QMutexLocker lock(&mutex);data=std::move(images);}
    QImage requestImage(const QString &id,QSize *size,const QSize &) override {
        QMutexLocker lock(&mutex);auto image=data.value(id);if(size)*size=image.size();return image;
    }
private:QMutex mutex;QHash<QString,QImage> data;
};
ImageCombineSession::ImageCombineSession(QQmlEngine *engine,QObject *parent):QObject(parent),m_engine(engine) {
    static quint64 next=0;m_providerId=QStringLiteral("nkshotcombine%1").arg(++next);
    if(engine){m_provider=new CombineImageProvider;engine->addImageProvider(m_providerId,m_provider);}
}
ImageCombineSession::~ImageCombineSession(){if(m_cancel)m_cancel->store(true);if(m_engine)m_engine->removeImageProvider(m_providerId);}
void ImageCombineSession::clear(){
    if(m_cancel){m_cancel->store(true);}
    ++m_epoch;m_active=false;m_pending=false;m_paths.clear();m_entries.clear();m_result={};m_preview={};m_error.clear();
    if(m_engine&&m_provider){m_provider->replace({});}
    Q_EMIT changed();
}
bool ImageCombineSession::begin(const QList<QUrl> &paths){
    if(m_running)return false;
    QPointer<ImageCombineSession> guard(this);const auto epoch=m_epoch+1;clear();if(!guard||m_epoch!=epoch)return false;
    if(!m_engine||paths.size()<2||paths.size()>16){m_error=tr("Choose 2–16 local images.");Q_EMIT changed();return false;}
    m_paths=paths;
    QCollator collator(QLocale(QLocale::English));collator.setNumericMode(true);
    std::stable_sort(m_paths.begin(),m_paths.end(),[&](const QUrl&a,const QUrl&b){return collator.compare(QFileInfo(a.toLocalFile()).fileName(),QFileInfo(b.toLocalFile()).fileName())<0;});
    m_active=true;schedule();return true;
}
void ImageCombineSession::move(int from,int to){if(from<0||to<0||from>=m_entries.size()||to>=m_entries.size()||from==to)return;m_entries.move(from,to);schedule();}
void ImageCombineSession::remove(int index){if(index<0||index>=m_entries.size())return;m_entries.removeAt(index);schedule();}
void ImageCombineSession::setDirection(const QString &value){if((value!=QStringLiteral("vertical")&&value!=QStringLiteral("horizontal"))||value==m_direction)return;m_direction=value;if(m_active)schedule();else Q_EMIT changed();}
void ImageCombineSession::setKeepAll(bool value){if(value==m_keepAll)return;m_keepAll=value;if(m_active)schedule();else Q_EMIT changed();}
void ImageCombineSession::schedule(){
    if(m_cancel){m_cancel->store(true);}
    ++m_epoch;m_result={};m_preview={};m_error.clear();m_pending=true;publish();
    QPointer<ImageCombineSession> guard(this);const auto epoch=m_epoch;Q_EMIT changed();if(guard&&epoch==m_epoch&&!m_running)launch();
}
QVariantList ImageCombineSession::items() const {
    QVariantList out;for(const auto &e:m_entries)out.append(QVariantMap{{QStringLiteral("id"),e.id},{QStringLiteral("name"),e.name},{QStringLiteral("size"),e.image.size()},
        {QStringLiteral("thumbnailUrl"),QUrl(QStringLiteral("image://%1/%2/%3").arg(m_providerId,QString::number(m_epoch),e.id))}});return out;
}
QUrl ImageCombineSession::previewUrl() const {return ready()?QUrl(QStringLiteral("image://%1/%2/result").arg(m_providerId,QString::number(m_epoch))):QUrl();}
void ImageCombineSession::publish(){
    if(!m_engine||!m_provider)return;
    QHash<QString,QImage> data;
    for(const auto &e:m_entries)data.insert(QStringLiteral("%1/%2").arg(m_epoch).arg(e.id),e.thumbnail);
    if(!m_result.isNull())data.insert(QStringLiteral("%1/result").arg(m_epoch),m_preview);
    m_provider->replace(std::move(data));
}
void ImageCombineSession::launch(){
    if(!m_active||!m_pending||m_running)return;
    m_pending=false;m_running=true;m_cancel=std::make_shared<std::atomic_bool>(false);
    const auto token=m_cancel;const auto epoch=m_epoch;const auto paths=m_paths;const auto entries=m_entries;
    const bool horizontal=m_direction==QStringLiteral("horizontal");
    const bool keep=m_keepAll;
    auto watcher=new QFutureWatcher<Work>(this);
    connect(watcher,&QFutureWatcherBase::finished,this,[this,watcher,epoch]{
        const auto result=watcher->result();watcher->deleteLater();m_running=false;
        if(m_active&&m_engine&&epoch==m_epoch){
            if(result.loaded){m_entries=result.entries;m_paths.clear();}
            m_result=result.output;m_preview=result.preview;m_error=result.error;publish();
        }
        QPointer<ImageCombineSession> guard(this);Q_EMIT changed();if(guard&&m_pending)launch();
    });
    watcher->setFuture(QtConcurrent::run(combinePool(),[token,paths,entries,horizontal,keep]{
        Work work;work.entries=entries;
        if(token->load())return work;
        if(!paths.isEmpty()){
            work.entries.clear();qint64 bytesTotal=0,pixels=0;int id=0;
            for(const auto &url:paths){
                if(token->load())return Work();
                const auto path=url.toLocalFile();
                if(!url.isValid()||!url.isLocalFile()||!url.host().isEmpty()||!url.userInfo().isEmpty()||url.port()!=-1
                    ||url.hasQuery()||url.hasFragment()||path.isEmpty()||!QFileInfo(path).isAbsolute()
                    ||path.startsWith(QStringLiteral("//"))||path.contains(QChar(u'\0'))){work.error=tr("Use local PNG, JPEG, static WebP or BMP files.");return work;}
                const int fd=::open(QFile::encodeName(path).constData(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
                if(fd<0){work.error=tr("An image could not be opened safely.");return work;}
                QFile file;if(!file.open(fd,QIODevice::ReadOnly,QFileDevice::AutoCloseHandle)){::close(fd);work.error=tr("An image could not be read.");return work;}
                struct stat before{},after{},named{};
                if(::fstat(fd,&before)||!S_ISREG(before.st_mode)||before.st_size<=0||before.st_size>64LL*1024*1024||(bytesTotal+=before.st_size)>256LL*1024*1024){work.error=tr("Images must be regular files within 64 MiB each and 256 MiB total.");return work;}
                const auto bytes=file.read(before.st_size+1);
                if(bytes.size()!=before.st_size||file.error()!=QFileDevice::NoError||::fstat(fd,&after)||::lstat(QFile::encodeName(path).constData(),&named)||!same(before,after)||!same(before,named)){work.error=tr("An input changed while reading. Choose the files again.");return work;}
                if(animatedOrMalformed(bytes)){work.error=tr("Animated or malformed images cannot be combined. Choose static images.");return work;}
                // Apply EXIF orientation once while decoding; every later preview/join uses this immutable oriented snapshot.
                QBuffer buffer;buffer.setData(bytes);buffer.open(QIODevice::ReadOnly);QImageReader reader(&buffer);reader.setAutoTransform(true);
                const auto format=reader.format().toLower();const auto size=reader.size();
                if((format!=QByteArrayLiteral("png")&&format!=QByteArrayLiteral("jpeg")&&format!=QByteArrayLiteral("jpg")&&format!=QByteArrayLiteral("webp")&&format!=QByteArrayLiteral("bmp"))||reader.imageCount()>1||reader.supportsAnimation()
                    ||size.width()<1||size.height()<1||size.width()>32000||size.height()>32000||(pixels+=qint64(size.width())*size.height())>40000000){work.error=tr("Use static PNG, JPEG, WebP or BMP images within 40 megapixels total.");return work;}
                auto image=reader.read();if(image.isNull()||image.size().isEmpty()||qint64(image.width())*image.height()!=qint64(size.width())*size.height()){work.error=tr("An image could not be decoded completely.");return work;}
                const auto thumbnail=preview(image,160);
                work.entries.append({QString::number(++id),QFileInfo(path).fileName(),std::move(image),thumbnail});
            }
            work.loaded=true;
        }
        if(token->load())return Work();
        if(work.entries.size()<2){work.error=tr("Keep at least two images to combine.");return work;}
        QList<QImage> images;for(const auto&e:work.entries)images.append(e.image);
        using namespace NkShot::Imaging;
        auto result=horizontal?(keep?stackHorizontal(images):stitchHorizontal(images)):(keep?stackVertical(images):stitchVertical(images));
        if(token->load())return Work();
        switch(result.error){
        case StitchError::None:work.output=result.image;work.preview=preview(result.image,1400);break;
        case StitchError::IncompatibleFrames:work.error=tr("Image edges, pixel ratios or color profiles differ. Change direction or remove an image.");break;
        case StitchError::TooLarge:work.error=tr("The combined image exceeds the safe size limit. Remove an image.");break;
        default:work.error=tr("No unique overlap was found. Reorder images or choose Keep all pixels.");break;
        }
        return work;
    }));
}
