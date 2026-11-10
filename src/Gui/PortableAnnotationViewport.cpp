// SPDX-License-Identifier: GPL-3.0-only
#include "PortableAnnotationViewport.h"
#include <QQuickWindow>
#include <QSGImageNode>
#include <QSGRendererInterface>
#include <QSGTexture>
#include <cmath>

PortableAnnotationViewport::PortableAnnotationViewport(QQuickItem *parent)
    : AnnotationViewport(parent)
{
    connect(this, &AnnotationViewport::documentChanged, this, [this]() {
        disconnect(m_repaintConnection);
        if (document()) {
            m_repaintConnection = connect(document(), &AnnotationDocument::repainted, this, [this]() {
                polish();
                update();
            });
        }
        polish();
        update();
    });
}

void PortableAnnotationViewport::updatePolish()
{
    // The document may flush a GUI timer when rendering. Never perform this
    // from updatePaintNode: a threaded scene graph can block the GUI there.
    if (document() && window()
        && window()->rendererInterface()->graphicsApi() == QSGRendererInterface::Software) {
        m_softwareImage = document()->renderToImage();
    } else {
        m_softwareImage = {};
    }
}

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
    const qreal imageScale = windowDpr / imageDpr;
    const auto windowImageSize = (view.size() * windowDpr).toSize();
    const auto pixels = QRectF(view.topLeft() * imageDpr, windowImageSize.toSizeF() / imageScale).toRect();
    if (pixels.isEmpty()) {
        return nullptr;
    }
    auto image = m_softwareImage.copy(pixels);
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
