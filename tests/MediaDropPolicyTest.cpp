// SPDX-License-Identifier: GPL-3.0-only
#include "MediaDropPolicy.h"
#include <QtTest>
using NkShot::isSupportedMediaDrop;
class MediaDropPolicyTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void accepted_data() {
        QTest::addColumn<QUrl>("url");
        const QStringList paths{QStringLiteral("/not-required-to-exist/photo.png"), QStringLiteral("/tmp/photo.JPG"),
            QStringLiteral("/tmp/photo.JpEg"), QStringLiteral("/tmp/photo.WEBP"), QStringLiteral("/tmp/clip.MP4"),
            QStringLiteral("/tmp/clip.webm"), QStringLiteral("/tmp/中文 照片.png"), QStringLiteral("/tmp/100% done.png"),
            QStringLiteral("/tmp/literal%ZZ.jpg"), QStringLiteral("/tmp/literal%2F.png"), QStringLiteral("/tmp/question?#.png")};
        for (int i=0;i<paths.size();++i) QTest::newRow(qPrintable(QString::number(i))) << QUrl::fromLocalFile(paths.at(i));
        QTest::newRow("encoded-space") << QUrl(QStringLiteral("file:///tmp/my%20photo.png"),QUrl::StrictMode);
    }
    void accepted() { QFETCH(QUrl,url); QVERIFY(isSupportedMediaDrop({url})); }
    void rejected_data() {
        QTest::addColumn<QUrl>("url");
        const QStringList urls{QString(), QStringLiteral("https://example.org/file.png"), QStringLiteral("data:image/png;base64,abc"),
            QStringLiteral("photo.png"), QStringLiteral("file:photo.png"), QStringLiteral("file://server/tmp/photo.png"),
            QStringLiteral("file://localhost/tmp/photo.png"), QStringLiteral("file:////server/photo.png"), QStringLiteral("file://user@/tmp/photo.png"),
            QStringLiteral("file:///tmp/photo.png?"), QStringLiteral("file:///tmp/photo.png?x=1"),
            QStringLiteral("file:///tmp/photo.png#"), QStringLiteral("file:///tmp/photo.png#frag"),
            QStringLiteral("file:///tmp/dir.png/"), QStringLiteral("file:///tmp/no-extension"),
            QStringLiteral("file:///tmp/photo.gif"), QStringLiteral("file:///tmp/photo.svg"),
            QStringLiteral("file:///tmp/photo.png.exe"), QStringLiteral("file:///tmp/photo.%ZZpng"),
            QStringLiteral("file:///tmp/photo%00.png"), QStringLiteral("file:///tmp/bad%.png"),
            QStringLiteral("file:///tmp/bad%2.png"), QStringLiteral("file:///tmp/bad%GG.png")};
        for (int i=0;i<urls.size();++i) QTest::newRow(qPrintable(QString::number(i))) << QUrl(urls.at(i),QUrl::StrictMode);
    }
    void rejected() { QFETCH(QUrl,url); QVERIFY(!isSupportedMediaDrop({url})); }
    void cardinality() {
        const auto a=QUrl::fromLocalFile(QStringLiteral("/tmp/photo.png"));
        QVERIFY(!isSupportedMediaDrop({})); QVERIFY(!isSupportedMediaDrop({a,a}));
        QVERIFY(!isSupportedMediaDrop({a,QUrl(QStringLiteral("https://example.org/x.png"))}));
    }
};
QTEST_GUILESS_MAIN(MediaDropPolicyTest)
#include "MediaDropPolicyTest.moc"
