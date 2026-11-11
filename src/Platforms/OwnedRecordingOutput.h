// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
#include <QByteArray>
#include <QStringList>
#include <QList>
namespace NkShot {
// Reservation for an application-private output. Destruction only closes FDs:
// the caller must wait for producer Idle before explicitly requesting cleanup.
class OwnedRecordingOutput {
public:
    enum class CleanupResult { Removed, Missing, Preserved, Failed };
    OwnedRecordingOutput() = default;
    ~OwnedRecordingOutput();
    OwnedRecordingOutput(const OwnedRecordingOutput &) = delete;
    OwnedRecordingOutput &operator=(const OwnedRecordingOutput &) = delete;
    bool reserve(const QString &privateRoot, const QString &absoluteOutputPath);
    QString errorString() const { return m_error; }
    QString path() const { return m_path; }
    bool ownsFile() const { return m_file >= 0; }
    bool identityMatches() const;
    CleanupResult cleanup();
    void release();
private:
    bool directoryIdentityMatches() const;
    struct Identity { quint64 device; quint64 inode; };
    int m_root = -1, m_parent = -1, m_file = -1;
    QString m_rootPath, m_path, m_error;
    QStringList m_directories;
    QList<Identity> m_chain;
    QByteArray m_name;
};
} // namespace NkShot
