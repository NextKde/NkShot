// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QObject>
#include <QImage>
#include <QPointer>
#include <QRect>
#include <QTimer>
class QScreen;
class ScreenShotSource2;
class QDBusPendingCallWatcher;
// Dedicated request source. No connections to SpectacleCore or ExportManager.
// Each request freshly requires ScreenShot2 API >= 5 and explicitly requests
// compositor-side caller-window exclusion. No sleep-based repaint assumption.
class ScrollFrameKWin : public QObject {
    Q_OBJECT
public:
    explicit ScrollFrameKWin(QObject *parent = nullptr);
    ~ScrollFrameKWin() override;
    bool request(quint64 ticket, QScreen *screen, QRect outputLocalPixels);
    void cancel();
    bool busy() const { return m_ticket != 0; }
Q_SIGNALS:
    void frameReady(quint64 ticket, const QImage &frame);
    void requestFailed(quint64 ticket, const QString &reason);
private:
    QPointer<ScreenShotSource2> m_source;
    QPointer<QDBusPendingCallWatcher> m_probe;
    QTimer m_timeout;
    quint64 m_ticket = 0;
    quint64 m_generation = 0;
};
