// SPDX-License-Identifier: GPL-3.0-only
#include "RecordingAutoStop.h"
#include <QTimer>
#include <QPointer>
namespace NkShot {
void RecordingAutoStop::invalidate(){++m_revision;m_epoch=0;m_token=0;m_limit=0;m_elapsed=0;m_pending=false;m_fired=false;m_paused=false;Q_EMIT changed();}
void RecordingAutoStop::begin(int seconds,quint64 epoch){
    const QPointer<RecordingAutoStop> guard(this);const auto revision=m_revision+1;
    invalidate();if(!guard||m_revision!=revision||!epoch||(seconds!=60&&seconds!=300&&seconds!=600))return;
    m_epoch=epoch;m_limit=qint64(seconds)*1000;Q_EMIT changed();
}
void RecordingAutoStop::acknowledge(quint64 epoch,quint64 token){
    if(!enabled()||epoch!=m_epoch||!token||m_token)return;
    m_token=token;Q_EMIT changed();
}
void RecordingAutoStop::update(quint64 epoch,quint64 token,qint64 recordedMs,bool paused){
    if(!active()||epoch!=m_epoch||token!=m_token||recordedMs<0)return;
    const QPointer<RecordingAutoStop> guard(this);const auto revision=m_revision;
    m_elapsed=qMax(m_elapsed,recordedMs);m_paused=paused;Q_EMIT changed();
    if(!guard||revision!=m_revision)return;
    if(m_paused||m_elapsed<m_limit||m_pending||m_fired)return;
    m_pending=true;
    QTimer::singleShot(0,this,[this,revision,epoch,token]{
        if(revision!=m_revision||epoch!=m_epoch||token!=m_token)return;
        m_pending=false;
        if(!active()||m_paused||m_elapsed<m_limit||m_fired)return;
        m_fired=true;Q_EMIT stopRequested(epoch,token);
    });
}
}
