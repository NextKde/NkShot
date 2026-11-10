// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <KQuickImageEditor/AnnotationViewport>

// Keep the upstream document and input model. Only replace its GPU-only paint
// node when Qt explicitly uses its software scene graph.
class PortableAnnotationViewport : public AnnotationViewport
{
    Q_OBJECT
    QML_ELEMENT
public:
    explicit PortableAnnotationViewport(QQuickItem *parent = nullptr);
protected:
    void updatePolish() override;
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;
private:
    QImage m_softwareImage;
    QMetaObject::Connection m_repaintConnection;
};
