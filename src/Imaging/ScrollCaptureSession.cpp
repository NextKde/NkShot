// SPDX-License-Identifier: GPL-3.0-only
#include "ScrollCaptureSession.h"
#include <QtMath>
#include <limits>

namespace NkShot::Imaging {
ScrollCaptureSession::ScrollCaptureSession(int overlap, qint64 pixels)
    : m_minimumOverlap(overlap), m_maximumPixels(pixels) {}

bool ScrollCaptureSession::begin(const Region &region)
{
    // Starting over requires explicit cancel/finish; accidental double-clicks
    // cannot discard accepted work or invalidate an in-flight capture.
    if (m_state == State::Collecting || m_state == State::AwaitingFrame) return false;
    const auto size = region.physicalRect.size();
    if (!region.physicalRect.isValid() || region.outputId.isEmpty()
        || !qIsFinite(region.devicePixelRatio) || region.devicePixelRatio <= 0
        || m_minimumOverlap < 1 || size.height() <= m_minimumOverlap
        || size.width() > 8192 || size.height() > 32000
        || qint64(size.width()) * size.height() > m_maximumPixels) return false;
    m_region = region;
    m_frameCount = 0; m_composite = {}; m_pending = 0;
    m_state = State::Collecting;
    return true;
}
std::optional<quint64> ScrollCaptureSession::requestFrame()
{
    if (m_state != State::Collecting || m_frameCount >= 16
        || m_serial == std::numeric_limits<quint64>::max()) return {};
    m_pending = ++m_serial; // Never reset across cancelled/restarted sessions.
    m_state = State::AwaitingFrame;
    return m_pending;
}
bool ScrollCaptureSession::consumes(quint64 ticket)
{
    if (m_state != State::AwaitingFrame || ticket != m_pending) return false;
    m_pending = 0;
    m_state = State::Collecting;
    return true;
}
ScrollCaptureSession::Reply ScrollCaptureSession::captureFailed(quint64 ticket)
{
    if (!consumes(ticket)) return {Error::StaleRequest};
    return {Error::CaptureFailed};
}
ScrollCaptureSession::Reply ScrollCaptureSession::acceptFrame(quint64 ticket, const Region &region, const QImage &image)
{
    if (!consumes(ticket)) return {Error::StaleRequest};
    if (!(region == m_region)) return {Error::RegionChanged};
    if (image.isNull() || image.size() != m_region.physicalRect.size()
        || image.devicePixelRatio() != m_region.devicePixelRatio) return {Error::InvalidFrame};
    // Detach from backend buffers, which may be reused after the callback.
    const auto owned = image.copy();
    if (owned.isNull()) return {Error::InvalidFrame};
    if (m_frameCount == 0) {
        m_frameCount = 1; m_composite = owned;
        return {};
    }
    // The composite suffix is the entire last fixed-height frame. Match only
    // against that suffix implicitly, retaining no unbounded list of frames.
    const auto result = stitchVertical({m_composite, owned}, m_minimumOverlap, m_maximumPixels);
    if (result.error != StitchError::None) return {Error::StitchFailed, result.error};
    ++m_frameCount; m_composite = result.image;
    return {};
}
QImage ScrollCaptureSession::finish()
{
    // A single frame is not advertised as a successful scrolling result.
    if (m_state != State::Collecting || m_frameCount < 2) return {};
    const auto result = m_composite;
    m_frameCount = 0; m_composite = {}; m_pending = 0;
    m_state = State::Finished;
    return result;
}
void ScrollCaptureSession::cancel()
{
    m_pending = 0; m_frameCount = 0; m_composite = {};
    m_state = State::Cancelled;
}
}
