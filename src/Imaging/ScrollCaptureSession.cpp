// SPDX-License-Identifier: GPL-3.0-only
#include "ScrollCaptureSession.h"
#include "ScrollFrameAligner.h"
#include <cstring>
#include <QColorSpace>
#include <QtMath>
#include <limits>

namespace NkShot::Imaging {
ScrollCaptureSession::ScrollCaptureSession(int overlap, qint64 pixels)
    : m_minimumOverlap(overlap), m_maximumPixels(pixels) {}

bool ScrollCaptureSession::begin(const Region &region, ScrollDirection direction)
{
    // Starting over requires explicit cancel/finish; accidental double-clicks
    // cannot discard accepted work or invalidate an in-flight capture.
    if (m_state == State::Collecting || m_state == State::AwaitingFrame) return false;
    if(direction!=ScrollDirection::Vertical && direction!=ScrollDirection::Horizontal)return false;
    const bool horizontal=direction==ScrollDirection::Horizontal;
    const auto size = region.physicalRect.size();
    if (!region.physicalRect.isValid() || region.outputId.isEmpty()
        || !qIsFinite(region.devicePixelRatio) || region.devicePixelRatio <= 0
        || m_minimumOverlap < 24 || (horizontal ? size.width() : size.height()) <= m_minimumOverlap
        || (horizontal ? size.height() : size.width()) < 8 || size.width() > 8192 || size.height() > 8192
        || qint64(size.width()) * size.height() > qMin(m_maximumPixels, qint64(16000000))) return false;
    m_region = region; m_direction = direction;
    m_frameCount = 0; m_composite = {}; m_lastFrame = {}; m_seams.clear(); m_pending = 0;
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
        m_frameCount = 1; m_composite = owned; m_lastFrame = owned;
        return {};
    }
    // Estimate displacement using consecutive actual captures, not a composite
    // whose overlap deliberately keeps earlier pixels. Never blend or warp.
    const auto alignment = alignScrollFrames(m_lastFrame, owned, m_minimumOverlap, m_direction);
    if (alignment.error != StitchError::None) return {Error::StitchFailed, alignment.error};
    const bool horizontal=m_direction==ScrollDirection::Horizontal;
    const qint64 width=qint64(m_composite.width())+(horizontal ? owned.width()-alignment.overlap : 0);
    const qint64 height=qint64(m_composite.height())+(horizontal ? 0 : owned.height()-alignment.overlap);
    if(width>(horizontal?32000:8192)||height>(horizontal?8192:32000)
        ||width*height>qMin(m_maximumPixels,qint64(40000000)))
        return {Error::StitchFailed, StitchError::TooLarge};
    const auto previous = m_composite.convertToFormat(QImage::Format_RGBA8888);
    const auto next = owned.convertToFormat(QImage::Format_RGBA8888);
    QImage joined(int(width), int(height), QImage::Format_RGBA8888);
    if (joined.isNull() || previous.isNull() || next.isNull()) return {Error::InvalidFrame};
    joined.setDevicePixelRatio(owned.devicePixelRatio()); joined.setColorSpace(owned.colorSpace());
    if(horizontal) {
        for(int y=0;y<previous.height();++y) {
            std::memcpy(joined.scanLine(y),previous.constScanLine(y),size_t(previous.width())*4);
            std::memcpy(joined.scanLine(y)+size_t(previous.width())*4,next.constScanLine(y)+size_t(alignment.overlap)*4,size_t(next.width()-alignment.overlap)*4);
        }
    } else {
        const size_t bytes = size_t(owned.width()) * 4;
        for (int y = 0; y < previous.height(); ++y) std::memcpy(joined.scanLine(y), previous.constScanLine(y), bytes);
        for (int y = alignment.overlap; y < next.height(); ++y)
            std::memcpy(joined.scanLine(previous.height() + y - alignment.overlap), next.constScanLine(y), bytes);
    }
    m_seams.append(horizontal ? previous.width() : previous.height());
    ++m_frameCount; m_composite = joined; m_lastFrame = owned;
    return {};
}
QImage ScrollCaptureSession::finish()
{
    // A single frame is not advertised as a successful scrolling result.
    if (m_state != State::Collecting || m_frameCount < 2) return {};
    const auto result = m_composite;
    m_frameCount = 0; m_composite = {}; m_lastFrame = {}; m_seams.clear(); m_pending = 0;
    m_state = State::Finished;
    return result;
}
void ScrollCaptureSession::cancel()
{
    m_pending = 0; m_frameCount = 0; m_composite = {}; m_lastFrame = {}; m_seams.clear();
    m_state = State::Cancelled;
}
}
