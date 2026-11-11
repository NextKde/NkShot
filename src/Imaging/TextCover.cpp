// SPDX-License-Identifier: GPL-3.0-only
#include "TextCover.h"
#include <QColorSpace>
#include <QPainter>
#include <algorithm>
#include <cmath>
namespace NkShot::Imaging {
namespace {
bool validSize(const QSize &s) {
    return s.width()>0 && s.height()>0 && s.width()<=8192 && s.height()<=32000
        && qint64(s.width())*s.height()<=20000000;
}
bool validRect(const QRect &r,const QSize &s) {
    return r.x()>=0 && r.y()>=0 && r.width()>0 && r.height()>0
        && qint64(r.x())+r.width()<=s.width() && qint64(r.y())+r.height()<=s.height();
}
struct Normalized {
    QString text;
    QList<int> owners;
    bool pendingSpace=false;
    int spaceOwner=-1;
    void append(const QString &value,int owner) {
        for(qsizetype i=0;i<value.size();) {
            const auto ch=value.at(i);
            const qsizetype units=ch.isHighSurrogate() && i+1<value.size() && value.at(i+1).isLowSurrogate()?2:1;
            if(ch.isSpace()) {
                if(!text.isEmpty()){pendingSpace=true;spaceOwner=owner;}
            } else {
                if(pendingSpace){text+=QChar(u' ');owners.append(spaceOwner);pendingSpace=false;}
                const auto folded=value.mid(i,units).toCaseFolded();
                text+=folded;
                for(qsizetype n=0;n<folded.size();++n)owners.append(owner);
            }
            i+=units;
        }
    }
};
}
TextCoverMatches findTextCoverMatches(const QList<TextWord> &words,const QSize &size,const QString &query) {
    TextCoverMatches result;
    if(!validSize(size)||words.size()>2000||query.isEmpty()||query.size()>256||query.contains(QChar(u'\0')))return result;
    Normalized needle;needle.append(query,-1);
    if(needle.text.isEmpty())return result;
    Normalized haystack;
    qsizetype budget=0;
    for(qsizetype i=0;i<words.size();++i) {
        const auto &word=words.at(i);
        budget+=word.text.size()+word.separatorBefore.size();
        if(budget>128*1024||word.text.trimmed().isEmpty()||word.text.contains(QChar(u'\0'))
            ||!validRect(word.bounds,size))return result;
        // LocalTextExtraction defines separators as spacing only. Refuse forged
        // separator text because it has no associated box to cover.
        for(const auto ch:word.separatorBefore)if(!ch.isSpace())return result;
        if(i)haystack.append(word.separatorBefore,-1);
        haystack.append(word.text,int(i));
    }
    result.valid=true;
    qsizetype offset=0;
    while((offset=haystack.text.indexOf(needle.text,offset))>=0) {
        if(result.matches.size()==100){result.matches.clear();result.valid=false;result.overflow=true;return result;}
        QList<int> indices;
        for(qsizetype i=offset;i<offset+needle.text.size();++i) {
            const int owner=haystack.owners.at(i);
            if(owner>=0 && (indices.isEmpty()||indices.last()!=owner))indices.append(owner);
        }
        if(indices.isEmpty()){result.matches.clear();result.valid=false;return result;}
        TextCoverMatch match;
        for(const int index:indices) {
            const auto &word=words.at(index);
            if(!match.text.isEmpty()&&match.text.size()<512)match.text+=word.separatorBefore.left(512-match.text.size());
            if(match.text.size()<512)match.text+=word.text.left(512-match.text.size());
            const int x=qMax(0,word.bounds.x()-2),y=qMax(0,word.bounds.y()-2);
            const int right=int(qMin(qint64(size.width()),qint64(word.bounds.x())+word.bounds.width()+2));
            const int bottom=int(qMin(qint64(size.height()),qint64(word.bounds.y())+word.bounds.height()+2));
            match.regions.append(QRect(x,y,right-x,bottom-y));
        }
        match.text=match.text.left(512);
        result.matches.append(match);
        offset+=needle.text.size();
    }
    return result;
}
QImage coveredTextCopy(const QImage &source,const QList<QRect> &regions) {
    if(!validSize(source.size())||regions.isEmpty()||regions.size()>2000
        ||!std::isfinite(source.devicePixelRatio())||source.devicePixelRatio()<=0)return {};
    for(const auto &rect:regions)if(!validRect(rect,source.size()))return {};
    auto normalized=source.colorSpace().isValid()?source.convertedToColorSpace(QColorSpace::SRgb):source;
    if(normalized.isNull())return {};
    normalized.setDevicePixelRatio(1);
    QImage result(source.size(),QImage::Format_RGB32);
    if(result.isNull())return {};
    result.fill(Qt::white);
    {
        QPainter painter(&result);
        if(!painter.isActive())return {};
        painter.setRenderHint(QPainter::Antialiasing,false);
        painter.drawImage(QPoint(),normalized);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        for(const auto &rect:regions)painter.fillRect(rect,Qt::black);
    }
    result.setColorSpace(QColorSpace::SRgb);
    result.setDevicePixelRatio(source.devicePixelRatio());
    return result;
}
}
