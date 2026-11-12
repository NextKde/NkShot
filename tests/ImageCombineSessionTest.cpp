// SPDX-License-Identifier: GPL-3.0-only
#include "Gui/ImageCombineSession.h"
#include <QtTest>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QColorSpace>
#include <QFile>
#include <QImageWriter>
#include <QtEndian>
#include <sys/stat.h>
#include <unistd.h>
class ImageCombineSessionTest:public QObject {
    Q_OBJECT
    QString save(const QString &path,QSize size,QColor color){QImage image(size,QImage::Format_ARGB32);image.fill(color);image.setColorSpace(QColorSpace::SRgb);return image.save(path)?path:QString();}
    QList<QUrl> urls(const QString&a,const QString&b){return {QUrl::fromLocalFile(a),QUrl::fromLocalFile(b)};}
private Q_SLOTS:
    void sortingOptionsAndSnapshots(){
        QTemporaryDir dir;QQmlEngine engine;ImageCombineSession session(&engine);
        const auto a=save(dir.filePath(QStringLiteral("shot2.png")),QSize(40,30),QColor(255,0,0,100));
        const auto b=save(dir.filePath(QStringLiteral("shot10.png")),QSize(40,30),Qt::blue);
        QVERIFY(session.begin(urls(b,a)));QTRY_VERIFY_WITH_TIMEOUT(!session.busy(),10000);QVERIFY2(session.ready(),qPrintable(session.error()));
        QCOMPARE(session.items().first().toMap().value(QStringLiteral("name")).toString(),QStringLiteral("shot2.png"));QCOMPARE(session.outputSize(),QSize(40,60));
        auto original=session.snapshot();QCOMPARE(original.pixelColor(0,0),QColor(255,0,0,100));QCOMPARE(original.colorSpace(),QColorSpace(QColorSpace::SRgb));
        QFile::remove(a);QFile::remove(b); // subsequent changes must use immutable decoded snapshots
        session.move(0,1);QVERIFY(session.snapshot().isNull());QTRY_VERIFY_WITH_TIMEOUT(!session.busy(),10000);QVERIFY(session.ready());QCOMPARE(session.snapshot().pixelColor(0,0),QColor(Qt::blue));
        session.setDirection(QStringLiteral("horizontal"));QTRY_VERIFY_WITH_TIMEOUT(!session.busy(),10000);QVERIFY(session.ready());QCOMPARE(session.outputSize(),QSize(80,30));
        session.setKeepAll(false);QTRY_VERIFY_WITH_TIMEOUT(!session.busy(),10000);QVERIFY(!session.ready());QVERIFY(!session.error().isEmpty());QCOMPARE(session.items().size(),2);
        session.setKeepAll(true);QTRY_VERIFY_WITH_TIMEOUT(!session.busy(),10000);QVERIFY(session.ready());
        session.remove(0);QVERIFY(session.snapshot().isNull());QTRY_VERIFY_WITH_TIMEOUT(!session.busy(),10000);QCOMPARE(session.items().size(),1);QVERIFY(!session.ready());
        QCOMPARE(original.pixelColor(0,0),QColor(255,0,0,100));
    }
    void incompatibleRecoveryAndLatestWins(){
        QTemporaryDir dir;QQmlEngine engine;ImageCombineSession s(&engine);
        const auto a=save(dir.filePath(QStringLiteral("1.png")),QSize(40,30),Qt::red),b=save(dir.filePath(QStringLiteral("2.png")),QSize(50,30),Qt::blue);
        QVERIFY(s.begin(urls(a,b)));QTRY_VERIFY_WITH_TIMEOUT(!s.busy(),10000);QVERIFY(!s.ready());QCOMPARE(s.items().size(),2);
        s.setDirection(QStringLiteral("horizontal"));s.setKeepAll(false);s.setKeepAll(true);QTRY_VERIFY_WITH_TIMEOUT(!s.busy(),10000);QVERIFY2(s.ready(),qPrintable(s.error()));QCOMPARE(s.outputSize(),QSize(90,30));
        s.move(0,1);s.clear();QVERIFY(s.items().isEmpty());QVERIFY(s.previewUrl().isEmpty());QTRY_VERIFY_WITH_TIMEOUT(!s.busy(),10000);QVERIFY(!s.ready());QVERIFY(s.items().isEmpty());
        QVERIFY(s.begin(urls(a,b)));s.setDirection(QStringLiteral("vertical"));s.setDirection(QStringLiteral("horizontal"));QTRY_VERIFY_WITH_TIMEOUT(!s.busy(),10000);QVERIFY(s.ready());
    }
    void rejectsNonregularAndOversize(){
        QTemporaryDir dir;QQmlEngine engine;ImageCombineSession s(&engine);
        const auto good=save(dir.filePath(QStringLiteral("good.png")),QSize(32,32),Qt::green);
        const auto fifo=dir.filePath(QStringLiteral("pipe.png"));QVERIFY(::mkfifo(QFile::encodeName(fifo).constData(),0600)==0);
        QVERIFY(s.begin(urls(good,fifo)));QTRY_VERIFY_WITH_TIMEOUT(!s.busy(),5000);QVERIFY(!s.ready());QVERIFY(!s.error().isEmpty());
        const auto link=dir.filePath(QStringLiteral("link.png"));QVERIFY(::symlink(QFile::encodeName(good).constData(),QFile::encodeName(link).constData())==0);
        QVERIFY(s.begin(urls(good,link)));QTRY_VERIFY_WITH_TIMEOUT(!s.busy(),5000);QVERIFY(!s.ready());
        QFile huge(dir.filePath(QStringLiteral("huge.png")));QVERIFY(huge.open(QIODevice::WriteOnly));QVERIFY(huge.resize(64LL*1024*1024+1));huge.close();
        QVERIFY(s.begin(urls(good,huge.fileName())));QTRY_VERIFY_WITH_TIMEOUT(!s.busy(),5000);QVERIFY(!s.ready());
        QVERIFY(!s.begin({QUrl::fromLocalFile(good)}));QVERIFY(s.snapshot().isNull());
    }
    void sharedQueueAndReentrantObservers(){
        QTemporaryDir dir;QQmlEngine engine;
        const auto a=save(dir.filePath(QStringLiteral("1.png")),QSize(40,30),Qt::red),b=save(dir.filePath(QStringLiteral("2.png")),QSize(40,30),Qt::blue);
        ImageCombineSession first(&engine),second(&engine);
        QVERIFY(first.begin(urls(a,b)));QVERIFY(second.begin(urls(a,b)));
        QTRY_VERIFY_WITH_TIMEOUT(!first.busy()&&!second.busy(),10000);QVERIFY(first.ready());QVERIFY(second.ready());
        QPointer<ImageCombineSession> killed=new ImageCombineSession(&engine);
        connect(killed,&ImageCombineSession::changed,this,[&]{delete killed.data();});
        QVERIFY(!killed->begin(urls(a,b)));QVERIFY(!killed);
        ImageCombineSession reentrant(&engine);bool replaced=false;
        const auto connection=connect(&reentrant,&ImageCombineSession::changed,this,[&]{if(!replaced){replaced=true;reentrant.setDirection(QStringLiteral("horizontal"));reentrant.begin(urls(a,b));}});
        QVERIFY(!reentrant.begin(urls(a,b)));disconnect(connection);
        QTRY_VERIFY_WITH_TIMEOUT(!reentrant.busy(),10000);QVERIFY(reentrant.ready());QCOMPARE(reentrant.outputSize(),QSize(80,30));
        bool cleared=false;const auto clearConnection=connect(&reentrant,&ImageCombineSession::changed,this,[&]{if(!cleared){cleared=true;reentrant.clear();}});
        reentrant.setDirection(QStringLiteral("vertical"));disconnect(clearConnection);QTRY_VERIFY_WITH_TIMEOUT(!reentrant.busy(),10000);QVERIFY(!reentrant.ready());QVERIFY(reentrant.items().isEmpty());
    }
    void supportedStaticFormats(){
        QTemporaryDir dir;QQmlEngine engine;
        for(const QByteArray &format:{QByteArray("png"),QByteArray("jpeg"),QByteArray("webp"),QByteArray("bmp")}){
            if(!QImageWriter::supportedImageFormats().contains(format))continue;
            QImage image(40,30,QImage::Format_RGB32);image.fill(Qt::red);
            const auto a=dir.filePath(QStringLiteral("1.")+QString::fromLatin1(format)),b=dir.filePath(QStringLiteral("2.")+QString::fromLatin1(format));
            QVERIFY(image.save(a,format.constData()));QVERIFY(image.save(b,format.constData()));
            ImageCombineSession session(&engine);QVERIFY(session.begin(urls(a,b)));QTRY_VERIFY_WITH_TIMEOUT(!session.busy(),10000);QVERIFY2(session.ready(),qPrintable(QString::fromLatin1(format)+QStringLiteral(": ")+session.error()));
        }
    }
    void animatedContainersAndStrictUrls(){
        QTemporaryDir dir;QQmlEngine engine;ImageCombineSession session(&engine);
        const auto good=save(dir.filePath(QStringLiteral("1.png")),QSize(40,30),Qt::red);
        const auto other=save(dir.filePath(QStringLiteral("2.png")),QSize(40,30),Qt::blue);
        const auto rejected=[&](const QUrl &bad){
            if(!session.begin(urls(good,other))||!QTest::qWaitFor([&]{return !session.busy();},10000)||!session.ready())return false;
            if(!session.begin({QUrl::fromLocalFile(good),bad})||!session.snapshot().isNull()||!session.previewUrl().isEmpty())return false;
            return QTest::qWaitFor([&]{return !session.busy();},10000)&&!session.ready()&&session.snapshot().isNull()&&!session.error().isEmpty();
        };
        auto url=QUrl::fromLocalFile(other);url.setQuery(QStringLiteral("x"));QVERIFY(rejected(url));
        url=QUrl::fromLocalFile(other);url.setFragment(QStringLiteral("x"));QVERIFY(rejected(url));
        url=QUrl::fromLocalFile(other);url.setHost(QStringLiteral("localhost"));QVERIFY(rejected(url));
        url=QUrl::fromLocalFile(other);url.setUserInfo(QStringLiteral("user"));QVERIFY(rejected(url));
        url=QUrl::fromLocalFile(other);url.setPort(123);QVERIFY(rejected(url));
        QVERIFY(rejected(QUrl::fromLocalFile(other+QChar(u'\0')+QStringLiteral("ignored"))));
        QVERIFY(rejected(QUrl(QStringLiteral("file:///tmp/bad%GG.png"),QUrl::StrictMode)));
        QVERIFY(rejected(QUrl(QStringLiteral("relative.png"))));
        QFile input(good);QVERIFY(input.open(QIODevice::ReadOnly));auto png=input.readAll();input.close();
        // Valid CRC: a static-only PNG decoder may otherwise ignore this acTL.
        QByteArray payload=QByteArray::fromHex("6163544c0000000200000000");quint32 crc=0xffffffffU;
        for(const char byte:payload){crc^=quint8(byte);for(int bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1)?0xedb88320U:0U);}crc^=0xffffffffU;
        QByteArray crcBytes(4,'\0');qToBigEndian(crc,crcBytes.data());
        png.insert(33,QByteArray::fromHex("00000008")+payload+crcBytes);
        const auto animated=dir.filePath(QStringLiteral("animated.png"));QFile output(animated);QVERIFY(output.open(QIODevice::WriteOnly));QCOMPARE(output.write(png),png.size());output.close();QVERIFY(rejected(QUrl::fromLocalFile(animated)));
        for(const QByteArray &type:{QByteArrayLiteral("ANIM"),QByteArrayLiteral("ANMF")}){
            auto bytes=QByteArray::fromHex("524946460c00000057454250")+type+QByteArray(4,'\0');
            const auto path=dir.filePath(QStringLiteral("animated.webp"));QFile webp(path);QVERIFY(webp.open(QIODevice::WriteOnly));QCOMPARE(webp.write(bytes),bytes.size());webp.close();QVERIFY(rejected(QUrl::fromLocalFile(path)));
        }
    }
    void destructionWithPendingWorker(){
        QTemporaryDir dir;QQmlEngine engine;const auto a=save(dir.filePath(QStringLiteral("1.png")),QSize(40,30),Qt::red),b=save(dir.filePath(QStringLiteral("2.png")),QSize(40,30),Qt::blue);
        auto s=new ImageCombineSession(&engine);QVERIFY(s->begin(urls(a,b)));delete s;QTest::qWait(100);
    }
};
QTEST_MAIN(ImageCombineSessionTest)
#include "ImageCombineSessionTest.moc"
