// SPDX-License-Identifier: GPL-3.0-only
#include "StatusNotifierAvailability.h"
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QPointer>
namespace NkShot {
namespace {
const QString service=QStringLiteral("org.kde.StatusNotifierWatcher");
const QString path=QStringLiteral("/StatusNotifierWatcher");
const QString properties=QStringLiteral("org.freedesktop.DBus.Properties");
}
StatusNotifierAvailability::StatusNotifierAvailability(QObject *parent):StatusNotifierAvailability(QDBusConnection::sessionBus(),parent){}
StatusNotifierAvailability::StatusNotifierAvailability(const QDBusConnection &bus,QObject *parent):QObject(parent),m_bus(bus) {
    m_bus.connect(QStringLiteral("org.freedesktop.DBus"),QStringLiteral("/org/freedesktop/DBus"),QStringLiteral("org.freedesktop.DBus"),QStringLiteral("NameOwnerChanged"),this,SLOT(ownerChanged(QString,QString,QString)));
    auto message=QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"),QStringLiteral("/org/freedesktop/DBus"),QStringLiteral("org.freedesktop.DBus"),QStringLiteral("GetNameOwner"));
    message.setArguments({service});const auto epoch=m_epoch;
    auto watcher=new QDBusPendingCallWatcher(m_bus.asyncCall(message,2000),this);
    connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,epoch](QDBusPendingCallWatcher *w){
        QDBusPendingReply<QString> reply=*w;w->deleteLater();if(epoch!=m_epoch)return;
        setOwner(reply.isError()?QString():reply.value());
    });
}
void StatusNotifierAvailability::update(bool available) {
    if(m_available==available)return;
    m_available=available;Q_EMIT changed();
}
void StatusNotifierAvailability::ownerChanged(const QString &name,const QString &,const QString &owner) {
    if(name==service)setOwner(owner);
}
void StatusNotifierAvailability::setOwner(const QString &owner) {
    ++m_epoch;++m_query;
    if(!m_owner.isEmpty()) {
        m_bus.disconnect(m_owner,path,service,QStringLiteral("StatusNotifierHostRegistered"),this,SLOT(hostSignal(QDBusMessage)));
        m_bus.disconnect(m_owner,path,service,QStringLiteral("StatusNotifierHostUnregistered"),this,SLOT(hostSignal(QDBusMessage)));
        m_bus.disconnect(m_owner,path,properties,QStringLiteral("PropertiesChanged"),this,SLOT(hostSignal(QDBusMessage)));
    }
    m_owner=owner.startsWith(QLatin1Char(':'))?owner:QString();
    const QPointer<StatusNotifierAvailability> guard(this);const auto epoch=m_epoch;
    update(false);if(!guard||epoch!=guard->m_epoch)return;
    if(m_owner.isEmpty())return;
    m_bus.connect(m_owner,path,service,QStringLiteral("StatusNotifierHostRegistered"),this,SLOT(hostSignal(QDBusMessage)));
    m_bus.connect(m_owner,path,service,QStringLiteral("StatusNotifierHostUnregistered"),this,SLOT(hostSignal(QDBusMessage)));
    m_bus.connect(m_owner,path,properties,QStringLiteral("PropertiesChanged"),this,SLOT(hostSignal(QDBusMessage)));
    refresh();
}
void StatusNotifierAvailability::hostSignal(const QDBusMessage &message) {
    if(message.service()!=m_owner||message.path()!=path)return;
    if(message.interface()==properties && (message.arguments().isEmpty()||message.arguments().first().toString()!=service))return;
    refresh();
}
void StatusNotifierAvailability::refresh() {
    if(m_owner.isEmpty())return;
    const auto owner=m_owner;const auto epoch=m_epoch;const auto query=++m_query;
    auto message=QDBusMessage::createMethodCall(owner,path,properties,QStringLiteral("Get"));
    message.setArguments({service,QStringLiteral("IsStatusNotifierHostRegistered")});
    auto watcher=new QDBusPendingCallWatcher(m_bus.asyncCall(message,2000),this);
    connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,owner,epoch,query](QDBusPendingCallWatcher *w){
        QDBusPendingReply<QDBusVariant> reply=*w;w->deleteLater();
        if(epoch!=m_epoch||query!=m_query||owner!=m_owner)return;
        const auto value=reply.isError()?QVariant():reply.value().variant();
        update(value.metaType()==QMetaType::fromType<bool>()&&value.toBool());
    });
}
} // namespace NkShot
