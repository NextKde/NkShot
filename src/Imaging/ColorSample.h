// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QColor>
#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QSizeF>
#include <QString>

namespace NkShot::Imaging
{
// Owns its pixels independently of the source, in 8-bit, straight-alpha sRGB.
// Pixel coordinates always refer to physical pixels, irrespective of source DPR.
QImage colorSampleSnapshot(const QImage &source);

// Maps a centered aspect-fit preview to physical image pixels. Letterboxing,
// invalid inputs and the exclusive right/bottom edges return (-1, -1).
QPoint colorSamplePixelAt(QPointF position, QSizeF viewport, QSize imageSize);

// Returns an unscaled 11-by-11 patch with the selected pixel at (5, 5).
// Areas beyond the image are transparent. Invalid selections return null.
QImage colorSampleNeighborhood(const QImage &image, QPoint pixel);

QString colorSampleHex(const QColor &color);
QString colorSampleRgb(const QColor &color);
}
