// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QObject>
#include <QColor>
#include <QDBusMessage>
#include <QTimer>
class ScreenColorPicker : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool hasColor READ hasColor NOTIFY changed)
    Q_PROPERTY(QColor color READ color NOTIFY changed)
    Q_PROPERTY(QString hexValue READ hexValue NOTIFY changed)
    Q_PROPERTY(QString rgbValue READ rgbValue NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
public:
    explicit ScreenColorPicker(QObject *parent = nullptr);
    ScreenColorPicker(int timeoutMs, QObject *parent);
    ~ScreenColorPicker() override;
    bool busy() const { return m_busy; }
    bool hasColor() const { return m_color.isValid(); }
    QColor color() const { return m_color; }
    QString hexValue() const { return hasColor() ? m_color.name(QColor::HexRgb).toUpper() : QString(); }
    QString rgbValue() const { return hasColor() ? QStringLiteral("rgb(%1, %2, %3)").arg(m_color.red()).arg(m_color.green()).arg(m_color.blue()) : QString(); }
    QString error() const { return m_error; }
    QString status() const { return m_status; }
    Q_INVOKABLE void request(const QString &parentWindow);
    Q_INVOKABLE void cancel();
Q_SIGNALS:
    void changed();
private Q_SLOTS:
    void response(const QDBusMessage &message);
    void ownerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);
private:
    void resolveOwner(quint64 epoch, const QString &parentWindow);
    void begin(quint64 epoch, const QString &parentWindow, const QString &owner);
    void finish(const QString &status, const QString &error = {}, bool close = false);
    void detach(bool close);
    bool m_busy = false, m_acknowledged = false;
    quint64 m_epoch = 0;
    int m_timeoutMs;
    QColor m_color;
    QString m_owner, m_path, m_error, m_status = QStringLiteral("idle");
    QList<QDBusMessage> m_early;
    QTimer m_timer;
};
