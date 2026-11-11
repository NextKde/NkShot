// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QObject>
#include <QImage>
#include <QPointer>
#include <QRect>
#include <QTimer>
class QScreen;
class ScreenShotSource2;
// Dedicated request source. No connections to SpectacleCore or ExportManager.
// A future controller must hide/exclude its windows and wait for repaint BEFORE
// calling request; this class does not claim to establish that visual barrier.
class ScrollFrameKWin : public QObject {
    Q_OBJECT
public:
    explicit ScrollFrameKWin(QObject *parent = nullptr);
    ~ScrollFrameKWin() override;
    bool request(quint64 ticket, QScreen *screen, QRect outputLocalPixels);
    void cancel();
    bool busy() const { return !m_source.isNull(); }
Q_SIGNALS:
    void frameReady(quint64 ticket, const QImage &frame);
    void requestFailed(quint64 ticket, const QString &reason);
private:
    QPointer<ScreenShotSource2> m_source;
    QTimer m_timeout;
    quint64 m_ticket = 0;
    quint64 m_generation = 0;
};
