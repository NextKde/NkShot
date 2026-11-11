// SPDX-License-Identifier: GPL-3.0-only
#include "OwnedRecordingOutput.h"
#include <QDir>
#include <QFile>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
namespace NkShot {
namespace {
bool privateDirectory(const struct stat &s) { return S_ISDIR(s.st_mode) && s.st_uid == ::geteuid() && (s.st_mode & 0077) == 0; }
bool regularSingle(const struct stat &s) { return S_ISREG(s.st_mode) && s.st_nlink == 1 && s.st_uid == ::geteuid() && (s.st_mode & 0077) == 0; }
QString systemError(const char *operation) { return QString::fromLatin1(operation) + QStringLiteral(": ") + QString::fromLocal8Bit(std::strerror(errno)); }
}
OwnedRecordingOutput::~OwnedRecordingOutput() { release(); }
void OwnedRecordingOutput::release() {
    if (m_file >= 0) ::close(m_file);
    if (m_parent >= 0) ::close(m_parent);
    if (m_root >= 0) ::close(m_root);
    m_file = m_parent = m_root = -1;
    m_rootPath.clear(); m_directories.clear(); m_chain.clear(); m_name.clear();
}
bool OwnedRecordingOutput::reserve(const QString &privateRoot, const QString &absoluteOutputPath) {
    if (m_file >= 0) { m_error = QStringLiteral("An output is already reserved."); return false; }
    release(); m_error.clear(); m_path.clear();
    if (!QDir::isAbsolutePath(privateRoot) || !QDir::isAbsolutePath(absoluteOutputPath)
        || privateRoot.contains(QChar::Null) || absoluteOutputPath.contains(QChar::Null)
        || QDir::cleanPath(privateRoot) != privateRoot || privateRoot == QStringLiteral("/")
        || !absoluteOutputPath.startsWith(privateRoot + QLatin1Char('/'))) {
        m_error = QStringLiteral("Output must be a strict relative path inside a private root."); return false;
    }
    const auto relative = absoluteOutputPath.mid(privateRoot.size() + 1);
    const auto components = relative.split(QLatin1Char('/'), Qt::KeepEmptyParts);
    for (const auto &part : components) if (part.isEmpty() || part == QStringLiteral(".") || part == QStringLiteral("..")) {
        m_error = QStringLiteral("Empty, dot and parent path components are not allowed."); return false;
    }
    m_root = ::open(QFile::encodeName(privateRoot).constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    struct stat rootStat{};
    if (m_root < 0 || ::fstat(m_root, &rootStat) != 0) { m_error = systemError("Open private root"); release(); return false; }
    if (!privateDirectory(rootStat)) { m_error = QStringLiteral("Root must be an owner-only directory belonging to this user."); release(); return false; }
    m_rootPath = privateRoot; m_chain.append({quint64(rootStat.st_dev), quint64(rootStat.st_ino)});
    m_parent = ::fcntl(m_root, F_DUPFD_CLOEXEC, 0);
    if (m_parent < 0) { m_error = systemError("Retain private root"); release(); return false; }
    for (qsizetype i = 0; i + 1 < components.size(); ++i) {
        const auto name = QFile::encodeName(components.at(i));
        int next = ::openat(m_parent, name.constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (next < 0 && errno == ENOENT) {
            if (::mkdirat(m_parent, name.constData(), 0700) != 0 && errno != EEXIST) { m_error = systemError("Create private subdirectory"); release(); return false; }
            next = ::openat(m_parent, name.constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        }
        struct stat directory{};
        if (next < 0 || ::fstat(next, &directory) != 0) { if (next >= 0) ::close(next); m_error = systemError("Open private subdirectory"); release(); return false; }
        if (!privateDirectory(directory)) { ::close(next); m_error = QStringLiteral("Output subdirectory is not owner-only."); release(); return false; }
        ::close(m_parent); m_parent = next; m_directories.append(components.at(i)); m_chain.append({quint64(directory.st_dev),quint64(directory.st_ino)});
    }
    m_name = QFile::encodeName(components.last());
    m_file = ::openat(m_parent, m_name.constData(), O_CREAT | O_EXCL | O_RDWR | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (m_file < 0) { m_error = systemError("Reserve new output"); release(); return false; }
    struct stat file{};
    if (::fstat(m_file, &file) != 0 || !regularSingle(file)) {
        m_error = QStringLiteral("Reserved output is not a single-link regular file."); release(); return false;
    }
    m_path = absoluteOutputPath;
    return true;
}
bool OwnedRecordingOutput::directoryIdentityMatches() const {
    if (m_file < 0) return false;
    int directory = ::open(QFile::encodeName(m_rootPath).constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    bool same = directory >= 0;
    for (qsizetype i = 0; same && i < m_chain.size(); ++i) {
        struct stat st{};
        same = ::fstat(directory, &st) == 0 && privateDirectory(st) && quint64(st.st_dev) == m_chain.at(i).device && quint64(st.st_ino) == m_chain.at(i).inode;
        if (same && i < m_directories.size()) {
            const int next = ::openat(directory, QFile::encodeName(m_directories.at(i)).constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
            ::close(directory); directory = next; same = next >= 0;
        }
    }
    if (directory >= 0) ::close(directory);
    return same;
}
bool OwnedRecordingOutput::identityMatches() const {
    if (!directoryIdentityMatches()) return false;
    struct stat held{}, entry{};
    return ::fstat(m_file, &held) == 0 && ::fstatat(m_parent, m_name.constData(), &entry, AT_SYMLINK_NOFOLLOW) == 0
        && regularSingle(held) && regularSingle(entry) && held.st_dev == entry.st_dev && held.st_ino == entry.st_ino;
}
OwnedRecordingOutput::CleanupResult OwnedRecordingOutput::cleanup() {
    if (m_file < 0) return CleanupResult::Missing;
    // Rewalk and compare the live directory chain. A moved/replaced directory is
    // deliberately preserved, even though the held parent FD could still reach it.
    if (!directoryIdentityMatches()) { m_error = QStringLiteral("Output directory identity changed; output preserved."); release(); return CleanupResult::Preserved; }
    struct stat held{}, entry{};
    if (::fstat(m_file, &held) != 0) { m_error = systemError("Inspect reserved output"); release(); return CleanupResult::Failed; }
    if (::fstatat(m_parent, m_name.constData(), &entry, AT_SYMLINK_NOFOLLOW) != 0) {
        const bool missing = errno == ENOENT; if (!missing) m_error = systemError("Inspect output entry"); release(); return missing ? CleanupResult::Missing : CleanupResult::Failed;
    }
    if (!regularSingle(held) || !regularSingle(entry) || held.st_dev != entry.st_dev || held.st_ino != entry.st_ino) {
        m_error = QStringLiteral("Output identity or link count changed; output preserved."); release(); return CleanupResult::Preserved;
    }
    // POSIX has no atomic unlink-if-inode-matches operation. Owner-only directories
    // exclude other users, not an adversarial same-UID concurrent pathname swap.
    const int result = ::unlinkat(m_parent, m_name.constData(), 0);
    const bool missing = result != 0 && errno == ENOENT;
    if (result != 0 && !missing) m_error = systemError("Remove reserved output");
    release();
    return result == 0 ? CleanupResult::Removed : missing ? CleanupResult::Missing : CleanupResult::Failed;
}
} // namespace NkShot
