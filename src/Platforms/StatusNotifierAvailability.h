// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QObject>
#include <QDBusConnection>
#include <QDBusMessage>
namespace NkShot {
// Protocol availability only: a registered host is not proof an icon is visible.
class StatusNotifierAvailability : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool hostAvailable READ hostAvailable NOTIFY changed)
public:
    explicit StatusNotifierAvailability(QObject *parent = nullptr);
    StatusNotifierAvailability(const QDBusConnection &bus, QObject *parent = nullptr);
    bool hostAvailable() const { return m_available; }
Q_SIGNALS:
    void changed();
private Q_SLOTS:
    void ownerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);
    void hostSignal(const QDBusMessage &message);
private:
    void setOwner(const QString &owner);
    void refresh();
    void update(bool available);
    QDBusConnection m_bus;
    QString m_owner;
    quint64 m_epoch = 0, m_query = 0;
    bool m_available = false;
};
} // namespace NkShot
