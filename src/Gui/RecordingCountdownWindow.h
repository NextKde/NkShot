// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QQuickView>

// Presentation of controller-owned state; this window never starts a stream.
class RecordingCountdownWindow : public QQuickView
{
    Q_OBJECT
    Q_PROPERTY(int remainingSeconds READ remainingSeconds NOTIFY stateChanged)
    Q_PROPERTY(QString description READ description NOTIFY stateChanged)
    Q_PROPERTY(QString phase READ phase NOTIFY stateChanged)
public:
    explicit RecordingCountdownWindow(QQmlEngine *engine);
    int remainingSeconds() const { return m_remainingSeconds; }
    QString description() const { return m_description; }
    QString phase() const { return m_phase; }
    void showCountdown(int remainingSeconds, const QString &description);
    void updateRemaining(int remainingSeconds);
    void showStarting();
    void dismiss();
    Q_INVOKABLE void cancel();
Q_SIGNALS:
    void stateChanged();
    void cancelRequested();
protected:
    bool event(QEvent *event) override;
private:
    int m_remainingSeconds = 0;
    QString m_description;
    QString m_phase = QStringLiteral("preparing");
    bool m_active = false;
};
