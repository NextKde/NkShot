// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "VerticalStitcher.h"
#include <QRect>
#include <QString>
#include <optional>

namespace NkShot::Imaging {
// Backend-independent manual-scroll collector. Does not capture or inject input.
// All calls belong on one owning/controller thread. Only accepted frames change
// the session; an asynchronous adapter must return the exact request ticket.
class ScrollCaptureSession {
public:
    struct Region {
        QRect physicalRect;
        QString outputId;
        qreal devicePixelRatio = 1;
        bool operator==(const Region &) const = default;
    };
    enum class State { Idle, Collecting, AwaitingFrame, Finished, Cancelled };
    enum class Error { None, StaleRequest, CaptureFailed,
                       RegionChanged, InvalidFrame, StitchFailed };
    struct Reply {
        Error error = Error::None;
        StitchError stitchError = StitchError::None;
        bool accepted() const { return error == Error::None; }
    };
    explicit ScrollCaptureSession(int minimumOverlap = 24, qint64 maximumPixels = 40000000);
    bool begin(const Region &region);
    std::optional<quint64> requestFrame();
    Reply acceptFrame(quint64 ticket, const Region &region, const QImage &image);
    Reply captureFailed(quint64 ticket);
    QImage finish();
    void cancel();
    State state() const { return m_state; }
    int frameCount() const { return m_frameCount; }
    QImage preview() const { return m_composite; }
    QVector<int> seams() const { return m_seams; }
private:
    bool consumes(quint64 ticket);
    Region m_region;
    State m_state = State::Idle;
    quint64 m_serial = 0;
    quint64 m_pending = 0;
    int m_minimumOverlap;
    qint64 m_maximumPixels;
    int m_frameCount = 0;
    QImage m_composite;
    QImage m_lastFrame;
    QVector<int> m_seams;
};
}
