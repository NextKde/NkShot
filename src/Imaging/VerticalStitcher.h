// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QImage>
#include <QList>
#include <QVector>

namespace NkShot::Imaging {
enum class StitchError { None, InvalidInput, IncompatibleFrames, NoOverlap, AmbiguousOverlap, NoProgress, TooLarge };
struct StitchResult {
    QImage image;
    QVector<int> overlaps;
    StitchError error = StitchError::None;
};
// Exact, conservative local stitching. No capture, scrolling, files or network.
// Frames must be ordered top-to-bottom with a unique unchanged vertical overlap.
// Explicit concatenation: retains every pixel, including duplicate content.
StitchResult stackVertical(const QList<QImage> &frames, qint64 maximumPixels = 40000000);
StitchResult stitchVertical(const QList<QImage> &frames, int minimumOverlap = 24, qint64 maximumPixels = 40000000);
}
