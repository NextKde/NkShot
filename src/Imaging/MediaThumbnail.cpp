// SPDX-License-Identifier: GPL-3.0-only
#include "MediaThumbnail.h"
#include <QBuffer>
#include <QColorSpace>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QtEndian>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
namespace NkShot::Imaging {
namespace {
constexpr qint64 MaxBytes = 64LL * 1024 * 1024;
bool same(const struct stat &a,const struct stat &b) {
    return a.st_dev==b.st_dev&&a.st_ino==b.st_ino&&a.st_size==b.st_size
        &&a.st_mtim.tv_sec==b.st_mtim.tv_sec&&a.st_mtim.tv_nsec==b.st_mtim.tv_nsec
        &&a.st_ctim.tv_sec==b.st_ctim.tv_sec&&a.st_ctim.tv_nsec==b.st_ctim.tv_nsec;
}
bool animated(const QByteArray &bytes) {
    if(bytes.startsWith(QByteArray::fromHex("89504e470d0a1a0a"))) {
        qsizetype offset=8;
        while(offset+12<=bytes.size()) {
            const auto length=qFromBigEndian<quint32>(bytes.constData()+offset);
            if(quint64(length)+12>quint64(bytes.size()-offset))return true;
            const auto type=bytes.mid(offset+4,4);
            if(type=="acTL")return true;
            offset+=qsizetype(length)+12;if(type=="IEND")break;
        }
    } else if(bytes.startsWith("RIFF")&&bytes.mid(8,4)=="WEBP") {
        qsizetype offset=12;
        while(offset+8<=bytes.size()) {
            const auto length=qFromLittleEndian<quint32>(bytes.constData()+offset+4);
            if(quint64(length)+8>quint64(bytes.size()-offset))return true;
            const auto type=bytes.mid(offset,4);if(type=="ANIM"||type=="ANMF")return true;
            offset+=8+qsizetype(length)+(length&1);
        }
    }
    return false;
}
}
QImage mediaThumbnail(const QUrl &url,int edge) {
    if(edge<1||edge>320||!url.isValid()||!url.isLocalFile()||!url.host().isEmpty()
        ||!url.userInfo().isEmpty()||url.port()!=-1||url.hasQuery()||url.hasFragment())return {};
    const auto path=url.toLocalFile();
    if(path.isEmpty()||!QDir::isAbsolutePath(path)||path.startsWith(QStringLiteral("//"))||path.contains(QChar(u'\0')))return {};
    const auto encoded=QFile::encodeName(path);struct stat initial{};
    if(::lstat(encoded.constData(),&initial)!=0||!S_ISREG(initial.st_mode)||initial.st_size<=0||initial.st_size>MaxBytes)return {};
    const int fd=::open(encoded.constData(),O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
    if(fd<0)return {};
    struct stat opened{};
    if(::fstat(fd,&opened)!=0||!S_ISREG(opened.st_mode)||!same(initial,opened)){::close(fd);return {};}
    QFile file;
    if(!file.open(fd,QIODevice::ReadOnly,QFileDevice::AutoCloseHandle)){::close(fd);return {};}
    const auto bytes=file.read(MaxBytes+1);
    if(bytes.size()!=opened.st_size||file.error()!=QFileDevice::NoError||animated(bytes))return {};
    QBuffer buffer;buffer.setData(bytes);if(!buffer.open(QIODevice::ReadOnly))return {};
    QImageReader reader(&buffer);reader.setDecideFormatFromContent(true);reader.setAutoTransform(true);
    const auto format=reader.format().toLower();
    if(format!="png"&&format!="jpg"&&format!="jpeg"&&format!="webp")return {};
    const auto size=reader.size();
    if(size.width()<1||size.height()<1||size.width()>32768||size.height()>32768
        ||qint64(size.width())*size.height()>40000000||reader.imageCount()>1)return {};
    if(qMax(size.width(),size.height())>edge)reader.setScaledSize(size.scaled(edge,edge,Qt::KeepAspectRatio).expandedTo(QSize(1,1)));
    auto image=reader.read();if(image.isNull()||image.sizeInBytes()>192LL*1024*1024)return {};
    if(qMax(image.width(),image.height())>edge)image=image.scaled(edge,edge,Qt::KeepAspectRatio,Qt::SmoothTransformation);
    if(image.isNull())return {};
    if(image.colorSpace().isValid())image=image.convertedToColorSpace(QColorSpace::SRgb);
    if(image.isNull())return {};
    image.setColorSpace(QColorSpace::SRgb);image.setDevicePixelRatio(1);
    struct stat after{},named{};
    if(::fstat(fd,&after)!=0||!same(opened,after)||::lstat(encoded.constData(),&named)!=0
        ||!S_ISREG(named.st_mode)||!same(opened,named))return {};
    return image;
}
}
