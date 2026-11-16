// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "ImageTransforms.h"
#include <QSizeF>
class QPainter;
namespace NkShot::Imaging
{
QSizeF deviceFrameSize(const QSize &source, PresentationFrame frame);
void drawDeviceFrame(QPainter &painter, const QImage &source, PresentationFrame frame);
}
