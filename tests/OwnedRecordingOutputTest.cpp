// SPDX-License-Identifier: GPL-3.0-only
#include "Platforms/OwnedRecordingOutput.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QProcess>
#include <QStandardPaths>
#include <sys/stat.h>
#include <unistd.h>
using NkShot::OwnedRecordingOutput;
using Result = OwnedRecordingOutput::CleanupResult;
class OwnedRecordingOutputTest : public QObject {
    Q_OBJECT
    static bool write(const QString &path,const QByteArray &bytes) {QFile file(path);return file.open(QIODevice::WriteOnly)&&file.write(bytes)==bytes.size();}
    static QByteArray read(const QString &path) {QFile file(path);return file.open(QIODevice::ReadOnly)?file.readAll():QByteArray();}
private Q_SLOTS:
    void ownedBytesCleanupAndRetry() {
        QTemporaryDir root;QVERIFY(root.isValid());OwnedRecordingOutput output;const auto path=root.filePath(QStringLiteral("nested/中文 ; ' clip.mp4"));
        QVERIFY2(output.reserve(root.path(),path),qPrintable(output.errorString()));QVERIFY(output.ownsFile());QVERIFY(output.identityMatches());QCOMPARE(output.path(),path);QVERIFY(write(path,"partial recording"));
        struct stat st{};QCOMPARE(::stat(QFile::encodeName(path).constData(),&st),0);QCOMPARE(st.st_mode&0777,mode_t(0600));
        QVERIFY(!output.reserve(root.path(),root.filePath(QStringLiteral("other.mp4"))));QCOMPARE(output.cleanup(),Result::Removed);QVERIFY(!QFile::exists(path));QVERIFY(!output.ownsFile());QCOMPARE(output.cleanup(),Result::Missing);
        QVERIFY(output.reserve(root.path(),path));QCOMPARE(output.cleanup(),Result::Removed);
    }
    void releaseAndDestructorPreserve() {
        QTemporaryDir root;const auto released=root.filePath(QStringLiteral("release.mp4"));const auto destroyed=root.filePath(QStringLiteral("destruct.mp4"));
        {OwnedRecordingOutput output;QVERIFY(output.reserve(root.path(),released));QVERIFY(write(released,"finished"));output.release();QVERIFY(!output.ownsFile());QCOMPARE(output.cleanup(),Result::Missing);}
        {OwnedRecordingOutput output;QVERIFY(output.reserve(root.path(),destroyed));QVERIFY(write(destroyed,"producer may still be running"));}
        QCOMPARE(read(released),QByteArray("finished"));QCOMPARE(read(destroyed),QByteArray("producer may still be running"));
    }
    void existingAndSymlinksRejected() {
        QTemporaryDir root;const auto existing=root.filePath(QStringLiteral("existing"));QVERIFY(write(existing,"keep"));OwnedRecordingOutput output;QVERIFY(!output.reserve(root.path(),existing));QCOMPARE(read(existing),QByteArray("keep"));
        const auto alias=root.filePath(QStringLiteral("alias"));QCOMPARE(::symlink(QFile::encodeName(existing).constData(),QFile::encodeName(alias).constData()),0);QVERIFY(!output.reserve(root.path(),alias));QCOMPARE(read(existing),QByteArray("keep"));
        QTemporaryDir outside;const auto link=root.filePath(QStringLiteral("directory-link"));QCOMPARE(::symlink(QFile::encodeName(outside.path()).constData(),QFile::encodeName(link).constData()),0);QVERIFY(!output.reserve(root.path(),link+QStringLiteral("/new.mp4")));QVERIFY(QDir(outside.path()).isEmpty());
    }
    void strictPathsAndBadRoots() {
        QTemporaryDir root;OwnedRecordingOutput output;
        for(const auto &suffix:{QStringLiteral("/../escape"),QStringLiteral("/a//b"),QStringLiteral("/./b"),QStringLiteral("/a/../b"),QStringLiteral("/")})QVERIFY(!output.reserve(root.path(),root.path()+suffix));
        QVERIFY(!output.reserve(root.path(),QStringLiteral("relative.mp4")));QVERIFY(!output.reserve(root.path(),root.path()+QStringLiteral("-else/file")));
        QVERIFY(!output.reserve(root.path()+QStringLiteral("/missing"),root.path()+QStringLiteral("/missing/file")));
        const auto regular=root.filePath(QStringLiteral("file"));QVERIFY(write(regular,"keep"));QVERIFY(!output.reserve(regular,regular+QStringLiteral("/out")));
        QCOMPARE(::chmod(QFile::encodeName(root.path()).constData(),0755),0);QVERIFY(!output.reserve(root.path(),root.filePath(QStringLiteral("out"))));QCOMPARE(::chmod(QFile::encodeName(root.path()).constData(),0700),0);
        const auto link=root.path()+QStringLiteral("-symlink");QCOMPARE(::symlink(QFile::encodeName(root.path()).constData(),QFile::encodeName(link).constData()),0);QVERIFY(!output.reserve(link,link+QStringLiteral("/out")));QVERIFY(QFile::remove(link));
    }
    void replacedFilePreserved() {
        QTemporaryDir root;OwnedRecordingOutput output;const auto path=root.filePath(QStringLiteral("clip"));QVERIFY(output.reserve(root.path(),path));QVERIFY(write(path,"owned"));QVERIFY(QFile::rename(path,path+QStringLiteral(".old")));QVERIFY(write(path,"replacement"));
        QVERIFY(!output.identityMatches());QCOMPARE(output.cleanup(),Result::Preserved);QCOMPARE(read(path),QByteArray("replacement"));QCOMPARE(read(path+QStringLiteral(".old")),QByteArray("owned"));
    }
    void replacedWithSymlinkPreserved() {
        QTemporaryDir root;OwnedRecordingOutput output;const auto path=root.filePath(QStringLiteral("clip"));const auto other=root.filePath(QStringLiteral("other"));QVERIFY(output.reserve(root.path(),path));QVERIFY(QFile::remove(path));QVERIFY(write(other,"untouched"));QCOMPARE(::symlink(QFile::encodeName(other).constData(),QFile::encodeName(path).constData()),0);
        QVERIFY(!output.identityMatches());QCOMPARE(output.cleanup(),Result::Preserved);QVERIFY(QFileInfo(path).isSymLink());QCOMPARE(read(other),QByteArray("untouched"));
    }
    void changedParentPreserved() {
        QTemporaryDir root;OwnedRecordingOutput output;const auto path=root.filePath(QStringLiteral("parent/clip"));QVERIFY(output.reserve(root.path(),path));QVERIFY(write(path,"owned"));QVERIFY(QDir(root.path()).rename(QStringLiteral("parent"),QStringLiteral("moved")));QVERIFY(QDir(root.path()).mkdir(QStringLiteral("parent")));QVERIFY(write(path,"replacement"));
        QVERIFY(!output.identityMatches());QCOMPARE(output.cleanup(),Result::Preserved);QCOMPARE(read(path),QByteArray("replacement"));QCOMPARE(read(root.filePath(QStringLiteral("moved/clip"))),QByteArray("owned"));
    }
    void hardLinkAndMissingPreserved() {
        QTemporaryDir root;OwnedRecordingOutput output;const auto path=root.filePath(QStringLiteral("clip"));QVERIFY(output.reserve(root.path(),path));QVERIFY(write(path,"shared"));const auto alias=root.filePath(QStringLiteral("hard"));QCOMPARE(::link(QFile::encodeName(path).constData(),QFile::encodeName(alias).constData()),0);
        QVERIFY(!output.identityMatches());QCOMPARE(output.cleanup(),Result::Preserved);QCOMPARE(read(path),QByteArray("shared"));QCOMPARE(read(alias),QByteArray("shared"));
        QVERIFY(output.reserve(root.path(),root.filePath(QStringLiteral("missing"))));QVERIFY(QFile::remove(output.path()));QCOMPARE(output.cleanup(),Result::Missing);
    }
    void permissionsBroadenedPreserved_data() {QTest::addColumn<bool>("changeRoot");QTest::newRow("file0644")<<false;QTest::newRow("root0755")<<true;}
    void permissionsBroadenedPreserved() {
        QFETCH(bool,changeRoot);QTemporaryDir root;OwnedRecordingOutput output;const auto path=root.filePath(QStringLiteral("clip"));QVERIFY(output.reserve(root.path(),path));QVERIFY(write(path,"keep broadened"));
        QCOMPARE(::chmod(QFile::encodeName(changeRoot?root.path():path).constData(),changeRoot?0755:0644),0);
        QVERIFY(!output.identityMatches());QCOMPARE(output.cleanup(),Result::Preserved);QCOMPARE(read(path),QByteArray("keep broadened"));
        QCOMPARE(::chmod(QFile::encodeName(root.path()).constData(),0700),0);
    }
    void renamedParentPreserved_data() {QTest::addColumn<bool>("replaceWithLink");QTest::newRow("missing-parent")<<false;QTest::newRow("symlink-parent")<<true;}
    void renamedParentPreserved() {
        QFETCH(bool,replaceWithLink);QTemporaryDir root;QTemporaryDir outside;OwnedRecordingOutput output;const auto path=root.filePath(QStringLiteral("parent/clip"));QVERIFY(output.reserve(root.path(),path));QVERIFY(write(path,"moved owned"));
        QVERIFY(QDir(root.path()).rename(QStringLiteral("parent"),QStringLiteral("moved")));
        if(replaceWithLink){QVERIFY(write(outside.filePath(QStringLiteral("clip")),"outside untouched"));QCOMPARE(::symlink(QFile::encodeName(outside.path()).constData(),QFile::encodeName(root.filePath(QStringLiteral("parent"))).constData()),0);}
        QVERIFY(!output.identityMatches());QCOMPARE(output.cleanup(),Result::Preserved);QCOMPARE(read(root.filePath(QStringLiteral("moved/clip"))),QByteArray("moved owned"));
        if(replaceWithLink)QCOMPARE(read(outside.filePath(QStringLiteral("clip"))),QByteArray("outside untouched"));
    }
    void replacementDirectoryAndUnknownSiblingPreserved() {
        QTemporaryDir root;OwnedRecordingOutput output;const auto path=root.filePath(QStringLiteral("clip"));QVERIFY(output.reserve(root.path(),path));QVERIFY(QFile::remove(path));QVERIFY(QDir(root.path()).mkdir(QStringLiteral("clip")));QVERIFY(write(path+QStringLiteral("/unknown"),"nested unknown"));
        QVERIFY(!output.identityMatches());QCOMPARE(output.cleanup(),Result::Preserved);QCOMPARE(read(path+QStringLiteral("/unknown")),QByteArray("nested unknown"));
        const auto fresh=root.filePath(QStringLiteral("fresh"));const auto sibling=root.filePath(QStringLiteral("unrelated"));QVERIFY(write(sibling,"sibling untouched"));QVERIFY(output.reserve(root.path(),fresh));QVERIFY(write(fresh,"owned"));QCOMPARE(output.cleanup(),Result::Removed);QCOMPARE(read(sibling),QByteArray("sibling untouched"));QCOMPARE(read(path+QStringLiteral("/unknown")),QByteArray("nested unknown"));
    }
    void actualFfmpegPreservesReservationInode() {
        const auto ffmpeg=QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));if(ffmpeg.isEmpty())QSKIP("FFmpeg CLI unavailable: AVIO reservation compatibility not executed");
        QTemporaryDir root;OwnedRecordingOutput output;const auto path=root.filePath(QStringLiteral("encoded.mp4"));QVERIFY(output.reserve(root.path(),path));struct stat before{},after{};QCOMPARE(::stat(QFile::encodeName(path).constData(),&before),0);
        QProcess process;process.start(ffmpeg,{QStringLiteral("-nostdin"),QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-y"),QStringLiteral("-f"),QStringLiteral("lavfi"),QStringLiteral("-i"),QStringLiteral("color=blue:s=64x48:r=10:d=0.3"),QStringLiteral("-an"),QStringLiteral("-c:v"),QStringLiteral("mpeg4"),QStringLiteral("-threads"),QStringLiteral("1"),path});
        QVERIFY(process.waitForFinished(15000));QVERIFY2(process.exitCode()==0,process.readAllStandardError().constData());QCOMPARE(::stat(QFile::encodeName(path).constData(),&after),0);QCOMPARE(before.st_dev,after.st_dev);QCOMPARE(before.st_ino,after.st_ino);QVERIFY(after.st_size>0);QVERIFY(output.identityMatches());QCOMPARE(output.cleanup(),Result::Removed);
    }
};
QTEST_GUILESS_MAIN(OwnedRecordingOutputTest)
#include "OwnedRecordingOutputTest.moc"
