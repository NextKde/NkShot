// SPDX-License-Identifier: GPL-3.0-only
#include "ImageAdjustmentsPreview.h"
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QColorSpace>
#include <QMutex>
#include <QMutexLocker>
#include <KLocalizedString>
class AdjustmentsImageProvider final : public QQuickImageProvider {
public:
    AdjustmentsImageProvider() : QQuickImageProvider(Image) {}
    void setImage(const QImage &image, quint64 revision) { QMutexLocker lock(&mutex); current=image; currentId=QString::number(revision); }
    QImage requestImage(const QString &id, QSize *size, const QSize &) override {
        QMutexLocker lock(&mutex);
        const auto key=id.startsWith(QLatin1Char('/'))?id.mid(1):id;
        const auto result=key==currentId?current:QImage();
        if(size)*size=result.size();
        return result;
    }
private:
    QMutex mutex;
    QImage current;
    QString currentId;
};
ImageAdjustmentsPreview::ImageAdjustmentsPreview(QQmlEngine *engine,QObject *parent)
    : QObject(parent),m_engine(engine) {
    static quint64 next=0;
    m_providerId=QStringLiteral("nkshotadjustments%1").arg(++next);
    if(engine){m_provider=new AdjustmentsImageProvider;engine->addImageProvider(m_providerId,m_provider);}
}
ImageAdjustmentsPreview::~ImageAdjustmentsPreview() { if(m_engine)m_engine->removeImageProvider(m_providerId); }
bool ImageAdjustmentsPreview::begin(const QImage &source) {
    clear();
    if(!m_engine||source.isNull()||source.width()>8192||source.height()>32000||qint64(source.width())*source.height()>40000000)return false;
    m_source=source.copy();
    if(m_source.isNull())return false;
    m_previewSource=m_source.colorSpace().isValid()&&m_source.colorSpace()!=QColorSpace(QColorSpace::SRgb)
        ?m_source.convertedToColorSpace(QColorSpace::SRgb):m_source;
    if(m_previewSource.isNull()){clear();return false;}
    m_previewSource.setColorSpace(QColorSpace::SRgb);
    m_previewSource.setDevicePixelRatio(1);
    if(qMax(m_previewSource.width(),m_previewSource.height())>1024){
        const auto size=m_previewSource.size().scaled(1024,1024,Qt::KeepAspectRatio).expandedTo(QSize(1,1));
        m_previewSource=m_previewSource.scaled(size,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
    }
    if(m_previewSource.isNull()){clear();return false;}
    m_options={};Q_EMIT changed();return true;
}
void ImageAdjustmentsPreview::clear() {
    m_source={};m_previewSource={};m_options={};m_ready=false;m_error.clear();++m_revision;
    if(m_engine&&m_provider)m_provider->setImage({},m_revision);
    Q_EMIT changed();
}
QUrl ImageAdjustmentsPreview::imageSource() const {
    return m_ready?QUrl(QStringLiteral("image://%1/%2").arg(m_providerId).arg(m_revision)):QUrl();
}
void ImageAdjustmentsPreview::update(int brightness,int contrast,int saturation) {
    if(!m_engine||m_previewSource.isNull())return;
    m_options={brightness,contrast,saturation};
    auto image=NkShot::Imaging::adjustedCopy(m_previewSource,m_options);
    if(!image.isNull())image.setDevicePixelRatio(1);
    m_ready=!image.isNull();
    m_error=m_ready?QString():i18nc("@info","The adjustment preview could not be created. Reset the values and try again.");
    m_provider->setImage(image,++m_revision);Q_EMIT changed();
}
