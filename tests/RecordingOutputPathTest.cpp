// SPDX-License-Identifier: GPL-3.0-only
#include "Platforms/RecordingOutputPath.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <sys/stat.h>
class RecordingOutputPathTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void existingFileRemainsUntouched() {
        QTemporaryDir folder;QVERIFY(folder.isValid());
        const auto path=folder.filePath(QStringLiteral("keep.mp4"));QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));QCOMPARE(file.write("existing recording"),18);file.close();
        QCOMPARE(NkShot::prepareRecordingOutputPath(QUrl::fromLocalFile(path)),NkShot::RecordingOutputError::None);
        QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),QByteArray("existing recording"));
    }
    void directoryAndBlockedParentRejected() {
        QTemporaryDir folder;QVERIFY(folder.isValid());
        const auto directory=folder.filePath(QStringLiteral("directory.mp4"));QVERIFY(QDir().mkpath(directory));
        QCOMPARE(NkShot::prepareRecordingOutputPath(QUrl::fromLocalFile(directory)),NkShot::RecordingOutputError::InvalidFile);
        const auto blocker=folder.filePath(QStringLiteral("file-parent"));QFile file(blocker);
        QVERIFY(file.open(QIODevice::WriteOnly));file.write("keep");file.close();
        QCOMPARE(NkShot::prepareRecordingOutputPath(QUrl::fromLocalFile(blocker+QStringLiteral("/clip.mp4"))),NkShot::RecordingOutputError::DirectoryUnavailable);
        QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),QByteArray("keep"));
    }
    void specialFileRejectedAndReadonlyParentPreserved() {
        QTemporaryDir folder;QVERIFY(folder.isValid());
        const auto fifo=folder.filePath(QStringLiteral("pipe.mp4"));
        QCOMPARE(::mkfifo(QFile::encodeName(fifo).constData(),0600),0);
        QCOMPARE(NkShot::prepareRecordingOutputPath(QUrl::fromLocalFile(fifo)),NkShot::RecordingOutputError::InvalidFile);
        const auto path=folder.filePath(QStringLiteral("existing.mp4"));QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));file.write("keep");file.close();
        const auto permissions=QFile::permissions(folder.path());
        QVERIFY(QFile::setPermissions(folder.path(),QFileDevice::ReadOwner|QFileDevice::ExeOwner));
        const auto result=NkShot::prepareRecordingOutputPath(QUrl::fromLocalFile(path));
        QVERIFY(QFile::setPermissions(folder.path(),permissions));
        QCOMPARE(result,NkShot::RecordingOutputError::None);
        QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),QByteArray("keep"));
    }
    void newPathCreatesOnlyDirectory() {
        QTemporaryDir folder;QVERIFY(folder.isValid());
        const auto path=folder.filePath(QStringLiteral("nested/clip.mp4"));
        QCOMPARE(NkShot::prepareRecordingOutputPath(QUrl::fromLocalFile(path)),NkShot::RecordingOutputError::None);
        QVERIFY(QFileInfo(folder.filePath(QStringLiteral("nested"))).isDir());QVERIFY(!QFileInfo::exists(path));
        QCOMPARE(NkShot::prepareRecordingOutputPath(QUrl(QStringLiteral("https://example.invalid/video.mp4"))),NkShot::RecordingOutputError::InvalidFile);
        QCOMPARE(NkShot::prepareRecordingOutputPath(QUrl::fromLocalFile(QStringLiteral("relative.mp4"))),NkShot::RecordingOutputError::InvalidFile);
    }
};
QTEST_GUILESS_MAIN(RecordingOutputPathTest)
#include "RecordingOutputPathTest.moc"
