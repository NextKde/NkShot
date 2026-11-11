// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QByteArray>
#include <QFile>
#include <QStringList>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
namespace NkShot::LocalVideoInput {
enum class Container { Mp4, WebM };
// Read only the finite EBML header, not arbitrary substring matches in a file.
// The shared Matroska demuxer name and a .webm suffix do not establish DocType.
inline bool hasWebMDocType(const QString &path)
{
    const int fd=::open(QFile::encodeName(path).constData(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
    if(fd<0) return false;
    struct stat st{};
    if(::fstat(fd,&st)!=0 || !S_ISREG(st.st_mode) || st.st_size<8) { ::close(fd); return false; }
    char buffer[4096];
    const auto count=::read(fd,buffer,sizeof(buffer));
    ::close(fd);
    if(count<8) return false;
    const QByteArray bytes(buffer,int(count));
    if(!bytes.startsWith(QByteArray::fromHex("1a45dfa3"))) return false;
    qsizetype position=4;
    const auto vint=[&](quint64 &value,bool identifier) {
        if(position>=bytes.size()) return false;
        const quint8 first=quint8(bytes[position]);
        quint8 mask=0x80; int length=1;
        while(mask && !(first&mask)) { mask>>=1; ++length; }
        if(!mask || length>(identifier?4:8) || length>bytes.size()-position) return false;
        value=identifier ? first : first&(mask-1);
        for(int i=1;i<length;++i) value=(value<<8)|quint8(bytes[position+i]);
        position+=length;
        // Unknown-sized EBML headers/elements are not safe bounded headers.
        return identifier || value!=((quint64(1)<<(7*length))-1);
    };
    quint64 headerSize=0;
    if(!vint(headerSize,false) || headerSize>quint64(bytes.size()-position)) return false;
    const auto end=position+qsizetype(headerSize);
    bool found=false;
    while(position<end) {
        quint64 id=0,size=0;
        if(!vint(id,true) || !vint(size,false) || position>end || size>quint64(end-position)) return false;
        if(id==0x4282) {
            if(found || size!=4 || bytes.mid(position,4)!="webm") return false;
            found=true;
        }
        position+=qsizetype(size);
    }
    return found && position==end;
}
inline QStringList demuxArguments(Container type)
{
    QStringList args{QStringLiteral("-protocol_whitelist"),QStringLiteral("file"),QStringLiteral("-f")};
    if(type==Container::WebM) args<<QStringLiteral("matroska");
    else args<<QStringLiteral("mov")<<QStringLiteral("-enable_drefs")<<QStringLiteral("0")
             <<QStringLiteral("-use_absolute_path")<<QStringLiteral("0");
    return args;
}
}
