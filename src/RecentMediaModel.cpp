// SPDX-License-Identifier: GPL-3.0-only
#include "RecentMediaModel.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>

namespace NkShot {
namespace {
constexpr qint64 MaximumBytes = 64 * 1024;
constexpr int MaximumItems = 20;
bool supported(const QString &path) {
    const auto suffix=QFileInfo(path).suffix().toLower();
    return suffix==QStringLiteral("png")||suffix==QStringLiteral("jpg")||suffix==QStringLiteral("jpeg")
        ||suffix==QStringLiteral("webp")||suffix==QStringLiteral("mp4")||suffix==QStringLiteral("webm");
}
}
RecentMediaModel::RecentMediaModel(const QString &storageFile,QObject *parent) : QAbstractListModel(parent)
{
    m_storageFile=storageFile.isEmpty()
        ? QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)).filePath(QStringLiteral("recent-media.json"))
        : storageFile;
    load();
}
RecentMediaModel *RecentMediaModel::instance()
{
    // The application owns this QObject; QPointer is cleared during application
    // destruction rather than deleting a QObject from a late static destructor.
    static QPointer<RecentMediaModel> model;
    if(!QCoreApplication::instance())return nullptr;
    if(!model)model=new RecentMediaModel(QString(),QCoreApplication::instance());
    return model;
}
int RecentMediaModel::rowCount(const QModelIndex &parent) const { return parent.isValid()?0:count(); }
QHash<int,QByteArray> RecentMediaModel::roleNames() const {
    return {{UrlRole,"url"},{NameRole,"name"},{FolderRole,"folder"},{KindRole,"kind"},{AvailableRole,"available"}};
}
QVariant RecentMediaModel::data(const QModelIndex &index,int role) const {
    if(!index.isValid()||index.model()!=this||index.column()!=0||index.row()<0||index.row()>=count())return {};
    const auto &entry=m_entries.at(index.row());const QFileInfo file(entry.url.toLocalFile());
    switch(role){
    case UrlRole:return entry.url;
    case NameRole:case Qt::DisplayRole:return file.fileName();
    case FolderRole:return file.absolutePath();
    case KindRole:{const auto suffix=file.suffix().toLower();return suffix==QStringLiteral("mp4")||suffix==QStringLiteral("webm")?QStringLiteral("video"):QStringLiteral("image");}
    case AvailableRole:return entry.available;
    default:return {};
    }
}
QUrl RecentMediaModel::normalizedLocalUrl(const QUrl &url) {
    if(!url.isValid()||!url.isLocalFile()||!url.host().isEmpty()||!url.userInfo().isEmpty()||url.hasQuery()||url.hasFragment())return {};
    const auto path=url.toLocalFile();
    if(path.isEmpty()||path.contains(QChar::Null)||!QDir::isAbsolutePath(path)||!supported(path))return {};
    return QUrl::fromLocalFile(QDir::cleanPath(path));
}
bool RecentMediaModel::available(const QUrl &url) {
    const QFileInfo file(url.toLocalFile());
    // Stored paths are stable. Do not follow a later replacement symlink to an
    // unrelated file or relocate a missing item by scanning the filesystem.
    return file.isFile()&&!file.isSymLink()&&file.isReadable();
}
void RecentMediaModel::setError(const QString &error) {
    if(error==m_error)return;
    m_error=error;Q_EMIT errorChanged();
}
void RecentMediaModel::load() {
    if(!QDir::isAbsolutePath(m_storageFile)){setError(tr("The recent-files storage path must be absolute."));return;}
    if(QFileInfo(m_storageFile).isSymLink()){
        setError(tr("The recent-files metadata path is a symbolic link and was ignored."));return;
    }
    const QFileInfo storage(m_storageFile);
    if(!storage.exists())return;
    if(!storage.isFile()){
        setError(tr("The recent-files metadata path is not a regular file and was ignored."));return;
    }
    QFile file(m_storageFile);
    if(!file.open(QIODevice::ReadOnly)){setError(tr("Recent files could not be read: %1").arg(file.errorString()));return;}
    const auto bytes=file.read(MaximumBytes+1);
    if(bytes.size()>MaximumBytes||!file.atEnd()){setError(tr("The recent-files list is too large and was ignored."));return;}
    QJsonParseError parse;
    const auto document=QJsonDocument::fromJson(bytes,&parse);
    if(parse.error!=QJsonParseError::NoError||!document.isArray()){setError(tr("The recent-files list is invalid and was ignored."));return;}
    QSet<QUrl> seen;
    // The byte bound also bounds traversal. Retain only twenty validated URLs,
    // even if externally supplied JSON contains many duplicate/invalid entries.
    for(const auto &value:document.array()){
        if(!value.isString())continue;
        const auto url=normalizedLocalUrl(QUrl(value.toString(),QUrl::StrictMode));
        if(url.isEmpty()||seen.contains(url))continue;
        seen.insert(url);m_entries.append({url,available(url)});
        if(m_entries.size()==MaximumItems)break;
    }
}
bool RecentMediaModel::commit(const QList<Entry> &entries) {
    if(!QDir::isAbsolutePath(m_storageFile)){setError(tr("The recent-files storage path must be absolute."));return false;}
    if(QFileInfo(m_storageFile).isSymLink()){
        setError(tr("Recent files could not be saved because the metadata file is a symbolic link."));return false;
    }
    const QFileInfo storage(m_storageFile);
    if(storage.exists()&&!storage.isFile()){
        setError(tr("Recent files could not be saved because the metadata path is not a regular file."));return false;
    }
    QJsonArray urls;for(const auto &entry:entries)urls.append(entry.url.toString(QUrl::FullyEncoded));
    const auto bytes=QJsonDocument(urls).toJson(QJsonDocument::Compact);
    if(bytes.size()>MaximumBytes){setError(tr("The recent-files list could not be saved because it is too large."));return false;}
    if(!QDir().mkpath(QFileInfo(m_storageFile).absolutePath())){setError(tr("The recent-files folder could not be created."));return false;}
    QSaveFile file(m_storageFile);
    if(!file.open(QIODevice::WriteOnly)){setError(tr("Recent files could not be saved: %1").arg(file.errorString()));return false;}
    // Only our newly opened metadata staging file is made private. Do not
    // change existing media permissions or any system security configuration.
    if(!file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner)){
        const auto error=file.errorString();file.cancelWriting();setError(tr("Recent files could not be saved privately: %1").arg(error));return false;
    }
    if(QFileInfo(m_storageFile).isSymLink()){
        file.cancelWriting();setError(tr("Recent files could not be saved because the metadata file became a symbolic link."));return false;
    }
    if(file.write(bytes)!=bytes.size()){const auto error=file.errorString();file.cancelWriting();setError(tr("Recent files could not be saved: %1").arg(error));return false;}
    if(!file.commit()){setError(tr("Recent files could not be saved: %1").arg(file.errorString()));return false;}
    beginResetModel();m_entries=entries;endResetModel();
    setError(QString());Q_EMIT changed();return true;
}
bool RecentMediaModel::remember(const QUrl &url) {
    const auto normalized=normalizedLocalUrl(url);
    if(normalized.isEmpty())return false;
    const QFileInfo file(normalized.toLocalFile());
    if(!file.isFile()||!file.isReadable())return false;
    // A successful explicit remember canonicalizes aliases; the saved path
    // itself will not subsequently follow renames or replacement symlinks.
    const auto canonical=normalizedLocalUrl(QUrl::fromLocalFile(file.canonicalFilePath()));
    if(canonical.isEmpty()||!available(canonical))return false;
    QList<Entry> entries{{canonical,true}};
    for(const auto &entry:m_entries){
        if(entry.url!=canonical)entries.append(entry);
        if(entries.size()==MaximumItems)break;
    }
    return commit(entries);
}
bool RecentMediaModel::removeAt(int row) {
    if(row<0||row>=count())return false;
    auto entries=m_entries;entries.removeAt(row);return commit(entries);
}
bool RecentMediaModel::clear() { return commit({}); }
void RecentMediaModel::refresh() {
    bool any=false;
    for(int row=0;row<count();++row){
        const bool current=available(m_entries.at(row).url);
        if(current==m_entries.at(row).available)continue;
        m_entries[row].available=current;Q_EMIT dataChanged(index(row),index(row),{AvailableRole});any=true;
    }
    if(any)Q_EMIT changed();
}

} // namespace NkShot
