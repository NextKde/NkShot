// SPDX-License-Identifier: GPL-3.0-only
#include "ImageOverlayPreview.h"
#include <QPainter>
void ImageOverlayPreview::paint(QPainter *painter) {
    if(m_image.isNull()||m_displayRect.isEmpty())return;
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    painter->drawImage(m_displayRect,m_image,QRectF(QPointF(),m_image.size()));
}
