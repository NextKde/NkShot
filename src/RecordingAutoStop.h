// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QObject>
namespace NkShot {
// Consumes the platform's active-recording clock, never wall time.
class RecordingAutoStop final : public QObject {
    Q_OBJECT
public:
    explicit RecordingAutoStop(QObject *parent=nullptr):QObject(parent){}
    void begin(int seconds,quint64 epoch);
    void acknowledge(quint64 epoch,quint64 token);
    void update(quint64 epoch,quint64 token,qint64 recordedMs,bool paused);
    void invalidate();
    bool enabled() const {return m_limit>0;}
    bool active() const {return enabled()&&m_token!=0;}
    qint64 remaining() const {return qMax(qint64(0),m_limit-m_elapsed);}
Q_SIGNALS:
    void changed();
    void stopRequested(quint64 epoch,quint64 token);
private:
    quint64 m_epoch=0,m_token=0,m_revision=0;
    qint64 m_limit=0,m_elapsed=0;
    bool m_pending=false,m_fired=false,m_paused=false;
};
}
