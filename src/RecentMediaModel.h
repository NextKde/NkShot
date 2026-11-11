// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QAbstractListModel>
#include <QUrl>

namespace NkShot {

// Local path metadata only. Removing an entry never removes its file.
class RecentMediaModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
public:
    enum Role { UrlRole = Qt::UserRole + 1, NameRole, FolderRole, KindRole, AvailableRole };
    Q_ENUM(Role)
    explicit RecentMediaModel(const QString &storageFile = QString(), QObject *parent = nullptr);
    static RecentMediaModel *instance();
    int count() const { return m_entries.size(); }
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    QString error() const { return m_error; }
    Q_INVOKABLE bool remember(const QUrl &url);
    Q_INVOKABLE bool removeAt(int row);
    Q_INVOKABLE bool clear();
    Q_INVOKABLE void refresh();
private:
    struct Entry { QUrl url; bool available = false; };
    static QUrl normalizedLocalUrl(const QUrl &url);
    static bool available(const QUrl &url);
    void load();
    bool commit(const QList<Entry> &entries);
    void setError(const QString &error);
    QList<Entry> m_entries;
    QString m_storageFile, m_error;
Q_SIGNALS:
    void changed();
    void errorChanged();
};

} // namespace NkShot
