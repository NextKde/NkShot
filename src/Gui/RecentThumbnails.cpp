// SPDX-License-Identifier: GPL-3.0-only
#include "RecentThumbnails.h"
#include "Imaging/MediaThumbnail.h"
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QFutureWatcher>
#include <QtConcurrentRun>
#include <QMutex>
#include <QMutexLocker>
#include <QFileInfo>
#include <QSet>
#include <QThreadPool>
namespace {
QThreadPool *thumbnailPool() {
    static QThreadPool pool;
    static const bool configured = [] { pool.setMaxThreadCount(1); return true; }();
    Q_UNUSED(configured);
    return &pool;
}
}

class RecentThumbnailProvider final : public QQuickImageProvider {
public:
    RecentThumbnailProvider() : QQuickImageProvider(Image) {}
    void clear() { QMutexLocker lock(&mutex); images.clear(); }
    void put(const QString &id, const QImage &image) { QMutexLocker lock(&mutex); images.insert(id,image); }
    QImage requestImage(const QString &id, QSize *size, const QSize &) override {
        QMutexLocker lock(&mutex);
        const auto image=images.value(id.startsWith(QLatin1Char('/'))?id.mid(1):id);
        if(size)*size=image.size();
        return image;
    }
private:
    QMutex mutex;
    QHash<QString,QImage> images;
};
RecentThumbnails::RecentThumbnails(QQmlEngine *engine,QObject *parent) : QObject(parent),m_engine(engine) {
    static quint64 next=0;
    m_providerId=QStringLiteral("nkshotrecent%1").arg(++next);
    if(engine) {m_provider=new RecentThumbnailProvider;engine->addImageProvider(m_providerId,m_provider);}
}
RecentThumbnails::~RecentThumbnails() { if(m_cancelled)m_cancelled->store(true); if(m_engine)m_engine->removeImageProvider(m_providerId); }
void RecentThumbnails::clear() {
    if(m_cancelled)m_cancelled->store(true);
    ++m_epoch;++m_revision;m_active=false;m_ready.clear();m_pending.clear();
    if(m_engine&&m_provider)m_provider->clear();
    Q_EMIT changed();
}
void RecentThumbnails::begin(const QList<QUrl> &urls) {
    const QPointer<RecentThumbnails> guard(this);
    clear();
    if(!guard||!m_engine)return;
    m_active=true;
    QSet<QUrl> seen;
    for(const auto &url:urls) {
        if(seen.contains(url))continue;
        seen.insert(url);
        const auto suffix=QFileInfo(url.toLocalFile()).suffix().toLower();
        if(suffix!=QStringLiteral("png")&&suffix!=QStringLiteral("jpg")&&suffix!=QStringLiteral("jpeg")&&suffix!=QStringLiteral("webp"))continue;
        m_pending.append(url);
        if(m_pending.size()==20)break;
    }
    startNext();
}
QUrl RecentThumbnails::urlFor(const QUrl &source) const {
    const auto id=m_ready.value(source);
    return m_engine&&m_active&&!id.isEmpty()?QUrl(QStringLiteral("image://%1/%2").arg(m_providerId,id)):QUrl();
}
void RecentThumbnails::startNext() {
    if(!m_engine||!m_active||m_running||m_pending.isEmpty())return;
    const auto source=m_pending.takeFirst();const auto epoch=m_epoch;
    m_running=true;
    m_cancelled=std::make_shared<std::atomic_bool>(false);
    const auto cancelled=m_cancelled;
    auto watcher=new QFutureWatcher<QImage>(this);
    connect(watcher,&QFutureWatcherBase::finished,this,[this,watcher,source,epoch] {
        const QPointer<RecentThumbnails> guard(this);
        const auto image=watcher->result();watcher->deleteLater();m_running=false;
        if(m_engine&&m_active&&epoch==m_epoch&&!image.isNull()) {
            const auto id=QString::number(++m_imageId);
            m_provider->put(id,image);m_ready.insert(source,id);++m_revision;Q_EMIT changed();
        }
        if(guard)startNext();
    });
    // A dedicated single-worker pool bounds decoding across replaced windows.
    // Cancellation skips queued reads and invalidates in-flight results;
    // it does not pretend to forcibly interrupt an image codec already running.
    watcher->setFuture(QtConcurrent::run(thumbnailPool(),[source,cancelled] {
        if(cancelled->load())return QImage();
        return NkShot::Imaging::mediaThumbnail(source,160);
    }));
}
