// SPDX-License-Identifier: GPL-3.0-only
#include "PortableAnnotationViewport.h"
#include <QQuickWindow>
#include <QSGImageNode>
#include <QSGRendererInterface>
#include <QSGTexture>
#include <cmath>

QSGNode *PortableAnnotationViewport::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data)
{
    if (!window() || window()->rendererInterface()->graphicsApi() != QSGRendererInterface::Software) {
        return AnnotationViewport::updatePaintNode(oldNode, data);
    }
    // Recreate the inexpensive node so texture ownership is unambiguous.
    delete oldNode;
    if (!document() || width() <= 0 || height() <= 0) {
        return nullptr;
    }
    const qreal imageDpr = document()->imageDpr();
    const qreal windowDpr = window()->effectiveDevicePixelRatio();
    const auto canvas = document()->canvasRect();
    const auto view = canvas.intersected(viewportRect().translated(canvas.topLeft())).translated(-canvas.topLeft());
    const auto pixels = QRectF(view.topLeft() * imageDpr, view.size() * imageDpr).toRect();
    if (pixels.isEmpty()) {
        return nullptr;
    }
    auto image = document()->renderToImage().copy(pixels);
    if (image.isNull()) {
        return nullptr;
    }
    const auto outputSize = (pixels.size().toSizeF() * (windowDpr / imageDpr)).toSize();
    image = image.scaled(outputSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    auto node = window()->createImageNode();
    node->setOwnsTexture(true);
    node->setTexture(window()->createTextureFromImage(image));
    node->setFiltering(QSGTexture::Linear);
    const auto size = image.size().toSizeF() / windowDpr;
    const QPointF origin(std::round((width() - size.width()) / 2 * windowDpr) / windowDpr,
                         std::round((height() - size.height()) / 2 * windowDpr) / windowDpr);
    node->setRect(QRectF(origin, size));
    return node;
}
