// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QImage>
#include <QString>
namespace NkShot::Imaging {
// Physical-pixel dimensions. Preserves aspect ratio to the nearest output row.
// Invalid/excessive sizes return null; never modifies the source.
// Clockwise quarter-turns, then horizontal mirror in the output orientation.
// A UI-independent sharing transform; input and edit history remain untouched.
QImage orientedCopy(const QImage &source, int quarterTurns, bool mirror, int width, qint64 maximumPixels = 40000000);
QImage resizedToWidth(const QImage &source, int width, qint64 maximumPixels = 40000000);
}

namespace NkShot::Imaging {
enum class PresentationStyle { Light, Dark, Aurora };
struct PresentationOptions {
    int padding = 48;
    PresentationStyle style = PresentationStyle::Aurora;
    QString watermark;
    bool rounded = true;
};
// A separate, opaque sRGB sharing canvas. Original pixels/metadata are untouched.
QImage composePresentation(const QImage &source, const PresentationOptions &options, qint64 maximumPixels = 40000000);
}
