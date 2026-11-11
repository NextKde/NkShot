// SPDX-License-Identifier: GPL-3.0-only
#include "TextCoverSession.h"
#include "Imaging/TextCover.h"
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QFutureWatcher>
#include <QtConcurrentRun>
#include <QColorSpace>
#include <QPainter>
#include <QMutex>
#include <QMutexLocker>
#include <KLocalizedString>
class TextCoverImageProvider final : public QQuickImageProvider {
public:
    TextCoverImageProvider():QQuickImageProvider(Image){}
    void setImage(QImage image,quint64 epoch){QMutexLocker lock(&mutex);current=std::move(image);key=QString::number(epoch);}
    QImage requestImage(const QString &id,QSize *size,const QSize &) override {
        QMutexLocker lock(&mutex);const auto result=(id.startsWith(QLatin1Char('/'))?id.mid(1):id)==key?current:QImage();
        if(size)*size=result.size();return result;
    }
private: QMutex mutex;QImage current;QString key;
};
TextCoverSession::TextCoverSession(QQmlEngine *engine,QObject *parent):QObject(parent),m_engine(engine){
    static quint64 next=0;m_providerId=QStringLiteral("nkshottextcover%1").arg(++next);
    if(engine){m_provider=new TextCoverImageProvider;engine->addImageProvider(m_providerId,m_provider);}
}
TextCoverSession::~TextCoverSession(){if(m_cancelled)m_cancelled->store(true);if(m_engine)m_engine->removeImageProvider(m_providerId);}
void TextCoverSession::clear(){
    if(m_cancelled)m_cancelled->store(true);++m_epoch;m_active=false;m_ready=false;m_source={};m_words.clear();m_matches.clear();m_error.clear();
    if(m_engine&&m_provider)m_provider->setImage({},m_epoch);
    notifyCandidatesChanged();
}
bool TextCoverSession::begin(const QImage &image,const QStringList &languages){
    if(m_running)return false;
    QPointer<TextCoverSession> guard(this);clear();if(!guard)return false;
    if(!m_engine||image.isNull()||image.width()>8192||image.height()>32000||qint64(image.width())*image.height()>20000000){
        m_error=i18nc("@info","Use an image within 20 megapixels, 8192 pixels wide and 32000 pixels tall.");Q_EMIT changed();return false;
    }
    m_source=image.copy();
    if(m_source.isNull()){m_error=i18nc("@info","The image could not be prepared.");Q_EMIT changed();return false;}
    auto small=m_source.colorSpace().isValid()?m_source.convertedToColorSpace(QColorSpace::SRgb):m_source;
    if(small.isNull()){m_source={};m_error=i18nc("@info","The image preview could not be prepared.");Q_EMIT changed();return false;}
    small.setDevicePixelRatio(1);
    if(qMax(small.width(),small.height())>1024)small=small.scaled(small.size().scaled(1024,1024,Qt::KeepAspectRatio).expandedTo(QSize(1,1)),Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
    QImage preview(small.size(),QImage::Format_RGB32);
    if(preview.isNull()||small.isNull()){m_source={};m_error=i18nc("@info","The image preview could not be prepared.");Q_EMIT changed();return false;}
    preview.fill(Qt::white);
    {QPainter painter(&preview);painter.drawImage(0,0,small);}
    preview.setColorSpace(QColorSpace::SRgb);preview.setDevicePixelRatio(1);m_provider->setImage(preview,m_epoch);
    m_active=true;m_running=true;m_cancelled=std::make_shared<std::atomic_bool>(false);
    const auto epoch=m_epoch;const auto snapshot=m_source;const auto token=m_cancelled;
    auto watcher=new QFutureWatcher<NkShot::Imaging::TextExtractionResult>(this);
    connect(watcher,&QFutureWatcherBase::finished,this,[this,watcher,epoch]{
        const auto result=watcher->result();watcher->deleteLater();m_running=false;
        if(m_active&&m_engine&&epoch==m_epoch){
            using Error=NkShot::Imaging::TextExtractionError;
            if(result.error!=Error::None){
                if(result.error==Error::Unavailable||result.error==Error::InvalidLanguage)m_error=i18nc("@info","Local OCR or the selected language is unavailable. Install the language data or use manual cover tools.");
                else if(result.error==Error::Busy)m_error=i18nc("@info","Local OCR is busy. Close this panel and try again.");
                else if(result.error==Error::Timeout)m_error=i18nc("@info","Text recognition timed out. Try a smaller image or use manual cover tools.");
                else m_error=i18nc("@info","Text recognition did not complete. Try again or use manual cover tools.");
            }else if(!result.wordsComplete)m_error=i18nc("@info","OCR word positions are incomplete. Use a smaller image or manual cover tools.");
            else if(result.words.isEmpty())m_error=i18nc("@info","No readable text was found. Use manual cover tools for anything private.");
            else{m_words=result.words;m_ready=true;}
        }
        Q_EMIT changed();
    });
    watcher->setFuture(QtConcurrent::run([snapshot,languages,token]{return NkShot::Imaging::extractLocalText(snapshot,languages,token,15000);}));
    Q_EMIT changed();return true;
}
QUrl TextCoverSession::imageSource() const{return m_active&&!m_source.isNull()&&m_engine?QUrl(QStringLiteral("image://%1/%2").arg(m_providerId).arg(m_epoch)):QUrl();}
void TextCoverSession::search(const QString &query){
    if(!m_ready||m_running)return;
    m_matches.clear();m_error.clear();
    const auto result=NkShot::Imaging::findTextCoverMatches(m_words,m_source.size(),query);
    if(result.overflow)m_error=i18nc("@info","Too many matches. Use a more specific search before covering text.");
    else if(!result.valid)m_error=i18nc("@info","Enter a valid search of up to 256 characters.");
    else{
        for(const auto &match:result.matches){QVariantList regions;for(const auto &rect:match.regions)regions.append(rect);
            m_matches.append(QVariantMap{{QStringLiteral("text"),match.text},{QStringLiteral("regions"),regions},{QStringLiteral("selected"),true}});}
        if(m_matches.isEmpty())m_error=i18nc("@info","No matching text found. OCR may have missed or misread it.");
    }
    notifyCandidatesChanged();
}
void TextCoverSession::clearMatches(){m_matches.clear();if(m_ready)m_error.clear();notifyCandidatesChanged();}
void TextCoverSession::notifyCandidatesChanged(){
    QPointer<TextCoverSession> guard(this);
    Q_EMIT candidatesChanged();
    if(guard)Q_EMIT changed();
}
QVariantList TextCoverSession::candidates() const{
    QVariantList result;
    for(const auto &value:m_matches){auto candidate=value.toMap();candidate.remove(QStringLiteral("selected"));result.append(candidate);}
    return result;
}
bool TextCoverSession::isSelected(int index) const{
    return index>=0&&index<m_matches.size()&&m_matches.at(index).toMap().value(QStringLiteral("selected")).toBool();
}
void TextCoverSession::setSelected(int index,bool selected){
    if(!m_ready||index<0||index>=m_matches.size())return;auto match=m_matches[index].toMap();
    if(match.value(QStringLiteral("selected")).toBool()==selected)return;
    match.insert(QStringLiteral("selected"),selected);m_matches[index]=match;Q_EMIT changed();
}
int TextCoverSession::selectedCount() const{int count=0;for(const auto &value:m_matches)if(value.toMap().value(QStringLiteral("selected")).toBool())++count;return count;}
QList<QRect> TextCoverSession::selectedRegions() const{
    QList<QRect> regions;for(const auto &value:m_matches){const auto match=value.toMap();if(!match.value(QStringLiteral("selected")).toBool())continue;
        for(const auto &valueRect:match.value(QStringLiteral("regions")).toList()){
            const auto rect=valueRect.toRect();if(!regions.contains(rect))regions.append(rect);
        }}return regions;
}
