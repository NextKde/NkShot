// SPDX-License-Identifier: GPL-3.0-only
#include "ScreenColorPicker.h"
#include <QDBusConnection>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusObjectPath>
#include <QDBusArgument>
#include <QUuid>
#include <QPointer>
#include <QCoreApplication>
#include <QRegularExpression>
#include <cmath>
namespace {
const QString service = QStringLiteral("org.freedesktop.portal.Desktop");
const QString desktop = QStringLiteral("/org/freedesktop/portal/desktop");
const QString requestInterface = QStringLiteral("org.freedesktop.portal.Request");
QDBusMessage busCall(const QString &method, const QVariantList &arguments) {
    auto message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"), QStringLiteral("org.freedesktop.DBus"), method);
    message.setArguments(arguments); return message;
}
}
ScreenColorPicker::ScreenColorPicker(QObject *parent) : ScreenColorPicker(60000, parent) {}
ScreenColorPicker::ScreenColorPicker(int timeoutMs, QObject *parent) : QObject(parent), m_timeoutMs(qBound(1, timeoutMs, 60000)) {
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, [this]{ finish(QStringLiteral("cancelled"), tr("The screen color picker timed out. Press Escape if the system picker is still open."), true); });
    QDBusConnection::sessionBus().connect(QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"), QStringLiteral("org.freedesktop.DBus"), QStringLiteral("NameOwnerChanged"), this, SLOT(ownerChanged(QString,QString,QString)));
}
ScreenColorPicker::~ScreenColorPicker() { ++m_epoch; detach(true); }
void ScreenColorPicker::detach(bool close) {
    m_timer.stop();
    auto bus = QDBusConnection::sessionBus();
    if (!m_path.isEmpty()) {
        bus.disconnect(m_owner, QString(), requestInterface, QStringLiteral("Response"), this, SLOT(response(QDBusMessage)));
        if (close && !m_owner.isEmpty()) bus.asyncCall(QDBusMessage::createMethodCall(m_owner, m_path, requestInterface, QStringLiteral("Close")), 1000);
    }
    m_path.clear(); m_owner.clear(); m_early = {}; m_acknowledged = false;
}
void ScreenColorPicker::finish(const QString &status, const QString &error, bool close) {
    ++m_epoch; detach(close); m_busy = false; m_status = status; m_error = error; Q_EMIT changed();
}
void ScreenColorPicker::cancel() {
    if (m_busy) finish(QStringLiteral("cancelled"), {}, true);
}
void ScreenColorPicker::request(const QString &parentWindow) {
    if (m_busy) return;
    m_color = QColor();
    if (parentWindow.size() > 4096 || (!parentWindow.isEmpty() && !parentWindow.startsWith(QStringLiteral("wayland:")) && !parentWindow.startsWith(QStringLiteral("x11:")))) {
        finish(QStringLiteral("error"), tr("The parent window identifier is invalid.")); return;
    }
    detach(false); const auto epoch = ++m_epoch; m_busy = true; m_error.clear(); m_status = QStringLiteral("waiting");
    m_timer.start(m_timeoutMs);
    const QPointer<ScreenColorPicker> guard(this);
    Q_EMIT changed();
    if (!guard || !guard->m_busy || epoch != guard->m_epoch) return;
    // Activate the installed portal when it is not yet running. A private test bus may already own it.
    auto watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(busCall(QStringLiteral("StartServiceByName"), {service, uint(0)}), m_timeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, epoch, parentWindow](QDBusPendingCallWatcher *w) {
        w->deleteLater(); if (!m_busy || epoch != m_epoch) return;
        resolveOwner(epoch, parentWindow);
    });
}
void ScreenColorPicker::resolveOwner(quint64 epoch, const QString &parentWindow) {
    auto watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(busCall(QStringLiteral("GetNameOwner"), {service}), m_timeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, epoch, parentWindow](QDBusPendingCallWatcher *w) {
        QDBusPendingReply<QString> reply = *w; w->deleteLater(); if (!m_busy || epoch != m_epoch) return;
        if (reply.isError() || !reply.value().startsWith(QLatin1Char(':'))) { finish(QStringLiteral("error"), tr("The desktop color picker is unavailable. Check that your desktop portal is running.")); return; }
        begin(epoch, parentWindow, reply.value());
    });
}
void ScreenColorPicker::begin(quint64 epoch, const QString &parentWindow, const QString &owner) {
    auto bus = QDBusConnection::sessionBus(); m_owner = owner;
    QString sender = bus.baseService().mid(1); sender.replace(QLatin1Char('.'), QLatin1Char('_'));
    const QString token = QStringLiteral("nkshot_") + QUuid::createUuid().toString(QUuid::Id128);
    m_path = QStringLiteral("/org/freedesktop/portal/desktop/request/") + sender + QLatin1Char('/') + token;
    if (!bus.connect(owner, QString(), requestInterface, QStringLiteral("Response"), this, SLOT(response(QDBusMessage)))) {
        finish(QStringLiteral("error"), tr("Could not listen for the desktop color picker.")); return;
    }
    auto message = QDBusMessage::createMethodCall(owner, desktop, QStringLiteral("org.freedesktop.portal.Screenshot"), QStringLiteral("PickColor"));
    message.setArguments({parentWindow, QVariantMap{{QStringLiteral("handle_token"), token}}});
    // Keep the method reply observer alive after cancel/destruction: some portals create
    // the Request only when replying, so an earlier Close may have done nothing.
    auto watcher = new QDBusPendingCallWatcher(bus.asyncCall(message, 60000), QCoreApplication::instance());
    const QPointer<ScreenColorPicker> self(this);
    const QString prefix = QStringLiteral("/org/freedesktop/portal/desktop/request/") + sender + QLatin1Char('/');
    connect(watcher, &QDBusPendingCallWatcher::finished, watcher, [self, epoch, owner, prefix, bus](QDBusPendingCallWatcher *w) mutable {
        QDBusPendingReply<QDBusObjectPath> reply = *w; w->deleteLater();
        const QString actual = reply.isError() ? QString() : reply.value().path();
        const bool valid = actual.startsWith(prefix) && QRegularExpression(QStringLiteral("^[A-Za-z0-9_]+$")).match(actual.mid(prefix.size())).hasMatch();
        const auto closeActual = [&] { if (valid) bus.asyncCall(QDBusMessage::createMethodCall(owner, actual, requestInterface, QStringLiteral("Close")), 1000); };
        if (!self || !self->m_busy || epoch != self->m_epoch) { closeActual(); return; }
        if (!valid) { self->finish(QStringLiteral("error"), tr("The desktop portal did not open a valid color picker request."), true); return; }
        self->m_path = actual;
        self->m_acknowledged = true;
        const auto early = self->m_early; self->m_early.clear();
        for (const auto &response : early) {
            if (response.path() == actual) { self->response(response); break; }
        }
    });
}
void ScreenColorPicker::ownerChanged(const QString &name, const QString &, const QString &newOwner) {
    if (m_busy && name == service && !m_owner.isEmpty() && newOwner != m_owner)
        finish(QStringLiteral("error"), tr("The desktop portal disconnected. Try again when it is available."));
}
void ScreenColorPicker::response(const QDBusMessage &message) {
    if (!m_busy || message.service() != m_owner) return;
    if (!m_acknowledged) {
        // Subscribe to the owner's Response signals before the method call. Older
        // portals may return a different path and emit before its reply is dispatched.
        const auto prefix = m_path.left(m_path.lastIndexOf(QLatin1Char('/')) + 1);
        if (message.path().startsWith(prefix) && m_early.size() < 8) m_early.append(message);
        return;
    }
    if (message.path() != m_path) return;
    if (message.signature() != QStringLiteral("ua{sv}") || message.arguments().size() != 2) { finish(QStringLiteral("error"), tr("The desktop portal returned an invalid color.")); return; }
    const uint result = message.arguments().at(0).toUInt();
    if (result == 1) { finish(QStringLiteral("cancelled")); return; }
    if (result != 0) { finish(QStringLiteral("error"), tr("The desktop color picker could not complete the request.")); return; }
    const auto values = qdbus_cast<QVariantMap>(message.arguments().at(1));
    const auto value = values.value(QStringLiteral("color"));
    if (value.metaType() != QMetaType::fromType<QDBusArgument>()) { finish(QStringLiteral("error"), tr("The desktop portal returned an invalid color.")); return; }
    const auto tuple = value.value<QDBusArgument>();
    if (tuple.currentSignature() != QStringLiteral("(ddd)")) { finish(QStringLiteral("error"), tr("The desktop portal returned an invalid color.")); return; }
    double r, g, b; tuple.beginStructure(); tuple >> r >> g >> b; tuple.endStructure();
    if (!std::isfinite(r) || !std::isfinite(g) || !std::isfinite(b) || r < 0 || r > 1 || g < 0 || g > 1 || b < 0 || b > 1) { finish(QStringLiteral("error"), tr("The desktop portal returned an invalid color.")); return; }
    m_color = QColor::fromRgbF(r, g, b); finish(QStringLiteral("success"));
}
