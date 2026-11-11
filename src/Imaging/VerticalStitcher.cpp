// SPDX-License-Identifier: GPL-3.0-only
#include "VerticalStitcher.h"
#include <QHashFunctions>
#include <QColorSpace>
#include <cstring>

namespace NkShot::Imaging {
namespace {
QVector<size_t> rowHashes(const QImage &image)
{
    QVector<size_t> rows;
    rows.reserve(image.height());
    for (int y = 0; y < image.height(); ++y)
        rows.append(qHashBits(image.constScanLine(y), size_t(image.width()) * 4, 0));
    return rows;
}
bool exactOverlap(const QImage &previous, const QImage &next, int rows)
{
    for (int y = 0; y < rows; ++y) {
        if (std::memcmp(previous.constScanLine(previous.height() - rows + y), next.constScanLine(y), size_t(next.width()) * 4))
            return false;
    }
    return true;
}
StitchResult fail(StitchError error) { return {{}, {}, error}; }
}
StitchResult stackVertical(const QList<QImage> &frames, qint64 maximumPixels)
{
    if (frames.size() < 2 || frames.size() > 16 || maximumPixels < 1)
        return fail(StitchError::InvalidInput);
    const auto &first = frames.first();
    qint64 height = 0;
    for (const auto &frame : frames) {
        if (frame.isNull()) return fail(StitchError::InvalidInput);
        if (frame.width() != first.width() || frame.devicePixelRatio() != first.devicePixelRatio() || frame.colorSpace() != first.colorSpace())
            return fail(StitchError::IncompatibleFrames);
        height += frame.height();
        if (frame.width() > 8192 || height > 32000 || height * frame.width() > maximumPixels)
            return fail(StitchError::TooLarge);
    }
    QImage output(first.width(), int(height), QImage::Format_RGBA8888);
    if (output.isNull()) return fail(StitchError::TooLarge);
    output.setDevicePixelRatio(first.devicePixelRatio());
    output.setColorSpace(first.colorSpace());
    int offset = 0;
    for (const auto &frame : frames) {
        const auto converted = frame.convertToFormat(QImage::Format_RGBA8888);
        if (converted.isNull()) return fail(StitchError::TooLarge);
        for (int y = 0; y < converted.height(); ++y)
            std::memcpy(output.scanLine(offset + y), converted.constScanLine(y), size_t(output.width()) * 4);
        offset += converted.height();
    }
    return {output, QVector<int>(frames.size() - 1, 0), StitchError::None};
}
StitchResult stitchVertical(const QList<QImage> &frames, int minimumOverlap, qint64 maximumPixels)
{
    if (frames.size() < 2 || frames.size() > 16 || minimumOverlap < 1 || maximumPixels < 1)
        return fail(StitchError::InvalidInput);
    const auto &first = frames.first();
    for (const auto &frame : frames) {
        if (frame.isNull() || frame.height() <= minimumOverlap) return fail(StitchError::InvalidInput);
        if (frame.width() != first.width() || frame.devicePixelRatio() != first.devicePixelRatio() || frame.colorSpace() != first.colorSpace())
            return fail(StitchError::IncompatibleFrames);
        if (qint64(frame.width()) * frame.height() > maximumPixels || frame.width() > 8192 || frame.height() > 32000)
            return fail(StitchError::TooLarge);
    }
    auto output = first.convertToFormat(QImage::Format_RGBA8888);
    if (output.isNull()) return fail(StitchError::TooLarge);
    auto previous = output;
    QVector<int> overlaps;
    for (int index = 1; index < frames.size(); ++index) {
        const auto next = frames[index].convertToFormat(QImage::Format_RGBA8888);
        if (next.isNull()) return fail(StitchError::TooLarge);
        const auto prefixRows = rowHashes(next);
        const auto previousRows = rowHashes(previous);
        QVector<int> prefix(prefixRows.size(), 0);
        for (int i = 1; i < prefix.size(); ++i) {
            int length = prefix[i - 1];
            while (length && prefixRows[i] != prefixRows[length]) length = prefix[length - 1];
            if (prefixRows[i] == prefixRows[length]) ++length;
            prefix[i] = length;
        }
        int matched = 0;
        for (const auto hash : previousRows) {
            if (matched == prefixRows.size()) matched = prefix[matched - 1];
            while (matched && hash != prefixRows[matched]) matched = prefix[matched - 1];
            if (hash == prefixRows[matched]) ++matched;
        }
        if (matched == next.height() && exactOverlap(previous, next, matched)) return fail(StitchError::NoProgress);
        int overlap = 0;
        for (int candidate = matched; candidate >= minimumOverlap; candidate = prefix[candidate - 1]) {
            if (!exactOverlap(previous, next, candidate)) continue; // hashes are not pixel proof
            if (overlap) return fail(StitchError::AmbiguousOverlap);
            overlap = candidate;
        }
        if (!overlap) return fail(StitchError::NoOverlap);
        const qint64 height = qint64(output.height()) + next.height() - overlap;
        if (height > 32000 || height * output.width() > maximumPixels) return fail(StitchError::TooLarge);
        QImage combined(output.width(), int(height), QImage::Format_RGBA8888);
        if (combined.isNull()) return fail(StitchError::TooLarge);
        combined.setDevicePixelRatio(first.devicePixelRatio());
        combined.setColorSpace(first.colorSpace());
        for (int y = 0; y < output.height(); ++y)
            std::memcpy(combined.scanLine(y), output.constScanLine(y), size_t(output.width()) * 4);
        for (int y = overlap; y < next.height(); ++y)
            std::memcpy(combined.scanLine(output.height() + y - overlap), next.constScanLine(y), size_t(next.width()) * 4);
        output = combined;
        previous = next;
        overlaps.append(overlap);
    }
    return {output, overlaps, StitchError::None};
}
}
