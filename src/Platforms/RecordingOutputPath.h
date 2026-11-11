// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QDir>
#include <QFileInfo>
#include <QUrl>
namespace NkShot {
enum class RecordingOutputError { None, InvalidFile, DirectoryUnavailable };
// Preflight only: never opens/truncates the destination. The recorder must still
// handle write failures, since permissions/storage can change after this check.
inline RecordingOutputError prepareRecordingOutputPath(const QUrl &url)
{
    if (!url.isLocalFile()) return RecordingOutputError::InvalidFile;
    const QFileInfo target(url.toLocalFile());
    if (!target.isAbsolute() || target.fileName().isEmpty()) return RecordingOutputError::InvalidFile;
    // Existing writable regular files need no write permission on their parent.
    // Reject devices/FIFOs/sockets rather than risking a blocking encoder open.
    if (target.exists()) return target.isFile() && target.isWritable()
        ? RecordingOutputError::None : RecordingOutputError::InvalidFile;
    QDir parent = target.dir();
    if ((!parent.exists() && !parent.mkpath(QStringLiteral("."))) || !QFileInfo(parent.absolutePath()).isWritable())
        return RecordingOutputError::DirectoryUnavailable;
    return RecordingOutputError::None;
}
}
