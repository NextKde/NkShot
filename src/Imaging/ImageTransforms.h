// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QImage>
namespace NkShot::Imaging {
// Physical-pixel dimensions. Preserves aspect ratio to the nearest output row.
// Invalid/excessive sizes return null; never modifies the source.
QImage resizedToWidth(const QImage &source, int width, qint64 maximumPixels = 40000000);
}
