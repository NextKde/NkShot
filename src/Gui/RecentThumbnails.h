// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QObject>
#include <QPointer>
#include <QUrl>
#include <QHash>
#include <QList>
#include <memory>
#include <atomic>
class QQmlEngine;
class RecentThumbnailProvider;
// Volatile, explicitly opened recent-file preview session. No disk cache.
class RecentThumbnails final : public QObject {
    Q_OBJECT
    Q_PROPERTY(quint64 revision READ revision NOTIFY changed)
public:
    explicit RecentThumbnails(QQmlEngine *engine, QObject *parent = nullptr);
    ~RecentThumbnails() override;
    void begin(const QList<QUrl> &urls);
    void clear();
    bool active() const { return m_active; }
    quint64 revision() const { return m_revision; }
    Q_INVOKABLE QUrl urlFor(const QUrl &source) const;
Q_SIGNALS:
    void changed();
private:
    void startNext();
    QPointer<QQmlEngine> m_engine;
    RecentThumbnailProvider *m_provider = nullptr;
    QString m_providerId;
    QHash<QUrl, QString> m_ready;
    QList<QUrl> m_pending;
    quint64 m_revision = 0, m_epoch = 0, m_imageId = 0;
    bool m_active = false, m_running = false;
    std::shared_ptr<std::atomic_bool> m_cancelled;
};
