// SPDX-License-Identifier: GPL-3.0-only
#include "RecentMediaModel.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>
#include <sys/stat.h>

using NkShot::RecentMediaModel;
class RecentMediaModelTest : public QObject {
    Q_OBJECT
    QTemporaryDir folder;
    QString file(const QString &name,const QByteArray &bytes=QByteArray("fixture-not-media-decoding")) {
        const auto path=folder.filePath(name);QFile output(path);
        if(!output.open(QIODevice::WriteOnly)||output.write(bytes)!=bytes.size())return {};
        output.close();return path;
    }
    QByteArray bytes(const QString &path) { QFile input(path);if(!input.open(QIODevice::ReadOnly))return {};return input.readAll(); }
    QUrl urlAt(const RecentMediaModel &model,int row) { return model.data(model.index(row),RecentMediaModel::UrlRole).toUrl(); }
private Q_SLOTS:
    void initTestCase() { QVERIFY(folder.isValid());QStandardPaths::setTestModeEnabled(true); }
    void sortingCanonicalDedupAndCapacity() {
        const auto storage=folder.filePath(QStringLiteral("sort.json"));RecentMediaModel model(storage);
        QSignalSpy changed(&model,&RecentMediaModel::changed);
        QStringList paths;
        for(int i=0;i<25;++i){const auto path=file(QStringLiteral("sort-%1.png").arg(i));QVERIFY(!path.isEmpty());paths<<path;QVERIFY(model.remember(QUrl::fromLocalFile(path)));}
        QCOMPARE(model.count(),20);QCOMPARE(urlAt(model,0),QUrl::fromLocalFile(paths.last()));QCOMPARE(urlAt(model,19),QUrl::fromLocalFile(paths.at(5)));
        QVERIFY(model.remember(QUrl::fromLocalFile(paths.at(10))));QCOMPARE(model.count(),20);QCOMPARE(urlAt(model,0),QUrl::fromLocalFile(paths.at(10)));
        const auto alias=folder.filePath(QStringLiteral("sort-alias.png"));QVERIFY(QFile::link(paths.at(10),alias));
        QVERIFY(model.remember(QUrl::fromLocalFile(alias)));QCOMPARE(model.count(),20);QCOMPARE(urlAt(model,0),QUrl::fromLocalFile(paths.at(10)));
        QCOMPARE(changed.size(),27);QVERIFY(model.error().isEmpty());
        const auto json=QJsonDocument::fromJson(bytes(storage));QVERIFY(json.isArray());QCOMPARE(json.array().size(),20);QVERIFY(bytes(storage).size()<=65536);
        RecentMediaModel reopened(storage);QCOMPARE(reopened.count(),20);
        for(int i=0;i<20;++i)QCOMPARE(urlAt(reopened,i),urlAt(model,i));
    }
    void rolesAndRejectedInputs() {
        RecentMediaModel model(folder.filePath(QStringLiteral("roles.json")));
        const auto image=file(QStringLiteral("screen ' 中文.PNG"));const auto video=file(QStringLiteral("recording.webm"));
        QVERIFY(model.remember(QUrl::fromLocalFile(image)));QVERIFY(model.remember(QUrl::fromLocalFile(video)));
        QCOMPARE(model.roleNames().value(RecentMediaModel::UrlRole),QByteArray("url"));
        QCOMPARE(model.data(model.index(0),RecentMediaModel::KindRole).toString(),QStringLiteral("video"));
        QCOMPARE(model.data(model.index(1),RecentMediaModel::KindRole).toString(),QStringLiteral("image"));
        QCOMPARE(model.data(model.index(1),RecentMediaModel::NameRole).toString(),QStringLiteral("screen ' 中文.PNG"));
        QCOMPARE(model.data(model.index(1),RecentMediaModel::FolderRole).toString(),folder.path());
        QVERIFY(model.data(model.index(0),RecentMediaModel::AvailableRole).toBool());
        QVERIFY(!model.data(QModelIndex(),RecentMediaModel::UrlRole).isValid());QCOMPARE(model.rowCount(model.index(0)),0);
        const auto unsupported=file(QStringLiteral("unsupported.gif"));
        const QList<QUrl> rejected{QUrl(),QUrl(QStringLiteral("https://example.invalid/a.png")),QUrl(QStringLiteral("a.png")),
            QUrl::fromLocalFile(QStringLiteral("relative.png")),QUrl::fromLocalFile(unsupported),
            QUrl::fromLocalFile(folder.filePath(QStringLiteral("missing.png"))),QUrl::fromLocalFile(folder.path()),
            QUrl(QStringLiteral("file://other-host/tmp/a.png")),QUrl::fromLocalFile(image).resolved(QUrl(QStringLiteral("#fragment"))),
            QUrl(QStringLiteral("file:///tmp/%00.png"))};
        QSignalSpy changed(&model,&RecentMediaModel::changed);
        for(const auto &url:rejected)QVERIFY(!model.remember(url));
        QCOMPARE(model.count(),2);QCOMPARE(changed.size(),0);QVERIFY(model.error().isEmpty());
        for(const auto &suffix:{QStringLiteral("jpg"),QStringLiteral("jpeg"),QStringLiteral("webp"),QStringLiteral("mp4")})
            QVERIFY(model.remember(QUrl::fromLocalFile(file(QStringLiteral("accepted.")+suffix))));
    }
    void missingFileRefreshAndMetadataOnlyRemoval() {
        const auto storage=folder.filePath(QStringLiteral("missing.json"));const auto path=file(QStringLiteral("move-me.png"));const auto other=file(QStringLiteral("keep-me.mp4"));
        RecentMediaModel model(storage);QVERIFY(model.remember(QUrl::fromLocalFile(other)));QVERIFY(model.remember(QUrl::fromLocalFile(path)));
        const auto diskBefore=bytes(storage);const auto relocated=folder.filePath(QStringLiteral("relocated.png"));QVERIFY(QFile::rename(path,relocated));
        QSignalSpy changed(&model,&RecentMediaModel::changed);QSignalSpy dataChanged(&model,&RecentMediaModel::dataChanged);
        model.refresh();QVERIFY(!model.data(model.index(0),RecentMediaModel::AvailableRole).toBool());QCOMPARE(model.count(),2);
        QCOMPARE(urlAt(model,0),QUrl::fromLocalFile(path));QCOMPARE(bytes(storage),diskBefore);QCOMPARE(changed.size(),1);QCOMPARE(dataChanged.size(),1);
        model.refresh();QCOMPARE(changed.size(),1);
        RecentMediaModel reopened(storage);QCOMPARE(reopened.count(),2);QVERIFY(!reopened.data(reopened.index(0),RecentMediaModel::AvailableRole).toBool());
        QVERIFY(QFile::rename(relocated,path));model.refresh();QVERIFY(model.data(model.index(0),RecentMediaModel::AvailableRole).toBool());
        const auto imageBytes=bytes(path),videoBytes=bytes(other);
        QVERIFY(!model.removeAt(-1));QVERIFY(!model.removeAt(model.count()));QVERIFY(model.removeAt(0));QCOMPARE(bytes(path),imageBytes);QCOMPARE(model.count(),1);
        QVERIFY(model.clear());QCOMPARE(model.count(),0);QCOMPARE(bytes(path),imageBytes);QCOMPARE(bytes(other),videoBytes);
        RecentMediaModel empty(storage);QCOMPARE(empty.count(),0);QVERIFY(empty.error().isEmpty());
    }
    void loadValidatesUntrustedMetadata() {
        QJsonArray list;list.append(QStringLiteral("https://example.invalid/a.png"));list.append(QStringLiteral("file://remote/tmp/a.png"));
        list.append(QStringLiteral("relative.png"));list.append(QStringLiteral("file:relative.png"));list.append(QStringLiteral("file:///tmp/a.gif"));
        list.append(QStringLiteral("file:///tmp/a.png?query=1"));list.append(QStringLiteral("file:///tmp/a.png#fragment"));list.append(QStringLiteral("file:///tmp/%00.png"));list.append(42);list.append(QJsonArray());
        const auto absent=QUrl::fromLocalFile(folder.filePath(QStringLiteral("absent.png")));
        list.append(absent.toString(QUrl::FullyEncoded));list.append(absent.toString(QUrl::FullyEncoded));
        for(int i=0;i<40;++i)list.append(QUrl::fromLocalFile(folder.filePath(QStringLiteral("loaded-%1.jpg").arg(i))).toString(QUrl::FullyEncoded));
        const auto storage=file(QStringLiteral("hostile.json"),QJsonDocument(list).toJson(QJsonDocument::Compact));RecentMediaModel model(storage);
        QCOMPARE(model.count(),20);QCOMPARE(urlAt(model,0),absent);QCOMPARE(urlAt(model,19),QUrl::fromLocalFile(folder.filePath(QStringLiteral("loaded-18.jpg"))));
        for(int row=0;row<model.count();++row)QVERIFY(!model.data(model.index(row),RecentMediaModel::AvailableRole).toBool());
        QVERIFY(model.error().isEmpty());
    }
    void corruptAndOversizeIgnored() {
        for(const auto &payload:{QByteArray("{"),QByteArray("{}"),QByteArray(65537,' '),QByteArray("null")}){
            const auto storage=file(QStringLiteral("corrupt.json"),payload);RecentMediaModel model(storage);QCOMPARE(model.count(),0);QVERIFY(!model.error().isEmpty());QCOMPARE(bytes(storage),payload);
        }
        const auto storage=file(QStringLiteral("exact-limit.json"),QByteArray("[]")+QByteArray(65534,' '));RecentMediaModel valid(storage);QCOMPARE(valid.count(),0);QVERIFY(valid.error().isEmpty());
    }
    void persistenceFailureKeepsPriorState() {
        const auto storage=folder.filePath(QStringLiteral("transaction.json"));const auto first=file(QStringLiteral("transaction-first.png"));const auto next=file(QStringLiteral("transaction-next.png"));
        RecentMediaModel model(storage);QVERIFY(model.remember(QUrl::fromLocalFile(first)));const auto stored=bytes(storage);
        const auto backup=storage+QStringLiteral(".backup");QVERIFY(QFile::rename(storage,backup));QVERIFY(QDir().mkpath(storage));
        QSignalSpy changed(&model,&RecentMediaModel::changed);QSignalSpy errors(&model,&RecentMediaModel::errorChanged);
        QVERIFY(!model.remember(QUrl::fromLocalFile(next)));QVERIFY(!model.error().isEmpty());QCOMPARE(model.count(),1);QCOMPARE(urlAt(model,0),QUrl::fromLocalFile(first));
        QVERIFY(!model.removeAt(0));QVERIFY(!model.clear());QCOMPARE(changed.size(),0);QVERIFY(!errors.isEmpty());QCOMPARE(bytes(backup),stored);QVERIFY(QFile::exists(first));QVERIFY(QFile::exists(next));
        QVERIFY(QDir().rmdir(storage));QVERIFY(QFile::rename(backup,storage));QVERIFY(model.remember(QUrl::fromLocalFile(next)));QVERIFY(model.error().isEmpty());QCOMPARE(model.count(),2);
        const auto blockingParent=file(QStringLiteral("not-a-folder"));RecentMediaModel noFolder(blockingParent+QStringLiteral("/list.json"));
        QVERIFY(!noFolder.remember(QUrl::fromLocalFile(first)));QVERIFY(!noFolder.error().isEmpty());QCOMPARE(noFolder.count(),0);
    }
    void metadataSymlinkRejectedAndPrivatePermissions() {
        const auto storage=folder.filePath(QStringLiteral("private-list.json"));
        const auto image=file(QStringLiteral("private-source.png"));const auto target=file(QStringLiteral("never-overwrite.png"));
        RecentMediaModel model(storage);QVERIFY(model.remember(QUrl::fromLocalFile(image)));
        const auto permissions=QFileInfo(storage).permissions();
        QVERIFY(permissions.testFlag(QFileDevice::ReadOwner));QVERIFY(permissions.testFlag(QFileDevice::WriteOwner));
        QVERIFY(!(permissions&(QFileDevice::ReadGroup|QFileDevice::WriteGroup|QFileDevice::ExeGroup
            |QFileDevice::ReadOther|QFileDevice::WriteOther|QFileDevice::ExeOther)));
        const auto stored=bytes(storage),original=bytes(target);const auto backup=storage+QStringLiteral(".backup");
        QVERIFY(QFile::rename(storage,backup));QVERIFY(QFile::link(target,storage));
        RecentMediaModel linked(storage);QCOMPARE(linked.count(),0);QVERIFY(!linked.error().isEmpty());
        QSignalSpy changes(&model,&RecentMediaModel::changed);
        QVERIFY(!model.clear());QVERIFY(!model.error().isEmpty());QCOMPARE(model.count(),1);
        QVERIFY(!model.removeAt(0));QVERIFY(!model.remember(QUrl::fromLocalFile(target)));
        QCOMPARE(changes.size(),0);QCOMPARE(bytes(target),original);QCOMPARE(bytes(backup),stored);QVERIFY(QFileInfo(storage).isSymLink());
        QVERIFY(QFile::remove(storage));QVERIFY(QFile::rename(backup,storage));
        QVERIFY(model.clear());QVERIFY(model.error().isEmpty());QCOMPARE(bytes(target),original);
    }
    void specialMetadataFilesDoNotBlockOrGetReplaced() {
        const auto storage=folder.filePath(QStringLiteral("special-list.json"));
        const auto image=file(QStringLiteral("special-image.png"));RecentMediaModel model(storage);
        QVERIFY(model.remember(QUrl::fromLocalFile(image)));const auto previous=bytes(storage);
        const auto backup=storage+QStringLiteral(".backup");QVERIFY(QFile::rename(storage,backup));
        QCOMPARE(::mkfifo(QFile::encodeName(storage).constData(),0600),0);
        QElapsedTimer elapsed;elapsed.start();RecentMediaModel fifo(storage);
        QVERIFY(elapsed.elapsed()<2000);QCOMPARE(fifo.count(),0);QVERIFY(!fifo.error().isEmpty());
        QVERIFY(!fifo.clear());QVERIFY(!model.clear());QVERIFY(!model.remember(QUrl::fromLocalFile(image)));QCOMPARE(model.count(),1);
        struct stat info{};QCOMPARE(::lstat(QFile::encodeName(storage).constData(),&info),0);QVERIFY(S_ISFIFO(info.st_mode));
        QCOMPARE(bytes(backup),previous);QVERIFY(QFile::exists(image));
        QVERIFY(QFile::remove(storage));QVERIFY(QDir().mkpath(storage));
        RecentMediaModel directory(storage);QCOMPARE(directory.count(),0);QVERIFY(!directory.error().isEmpty());
        QVERIFY(!directory.clear());QVERIFY(QFileInfo(storage).isDir());
        QVERIFY(QDir().rmdir(storage));QVERIFY(QFile::rename(backup,storage));
        QVERIFY(model.clear());QVERIFY(model.error().isEmpty());QVERIFY(QFile::exists(image));
    }
    void replacementSymlinkIsNotFollowed() {
        const auto path=file(QStringLiteral("replace.png"));const auto target=file(QStringLiteral("unrelated.png"));RecentMediaModel model(folder.filePath(QStringLiteral("symlink.json")));
        QVERIFY(model.remember(QUrl::fromLocalFile(path)));QVERIFY(QFile::remove(path));QVERIFY(QFile::link(target,path));model.refresh();
        QCOMPARE(urlAt(model,0),QUrl::fromLocalFile(path));QVERIFY(!model.data(model.index(0),RecentMediaModel::AvailableRole).toBool());
    }
    void singletonHasApplicationLifetime() {
        auto first=RecentMediaModel::instance();QVERIFY(first);QCOMPARE(first,RecentMediaModel::instance());QCOMPARE(first->parent(),QCoreApplication::instance());
    }
};
QTEST_GUILESS_MAIN(RecentMediaModelTest)
#include "RecentMediaModelTest.moc"
