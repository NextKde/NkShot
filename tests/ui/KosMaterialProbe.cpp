// SPDX-License-Identifier: GPL-3.0-only
// Component-level RHI validation, not a desktop/window or capture-backend test.
#include <QGuiApplication>
#include <QVulkanInstance>
#include <QQuickRenderControl>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickGraphicsDevice>
#include <QQuickRenderTarget>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QtQml/QQmlExtensionPlugin>
#include <QDir>
#include <QDebug>
#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>
#include <memory>

Q_IMPORT_QML_PLUGIN(Kos_UiPlugin)

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    const QString directory = app.arguments().value(1);
    if (directory.isEmpty() || !QDir().mkpath(directory)) {
        qCritical() << "Usage: kos_material_probe OUTPUT_DIRECTORY";
        return 1;
    }
    QVulkanInstance instance;
    instance.setExtensions(QRhiVulkanInitParams::preferredInstanceExtensions());
    if (!instance.create()) {
        qCritical() << "Vulkan instance failed" << instance.errorCode();
        return 2;
    }
    QRhiVulkanInitParams params;
    params.inst = &instance;
    std::unique_ptr<QRhi> rhi(QRhi::create(QRhi::Vulkan, &params));
    if (!rhi) {
        return 3;
    }
    qInfo() << "Component harness RHI driver:" << rhi->driverInfo().deviceName;
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
    QQuickRenderControl control;
    QQuickWindow window(&control);
    window.setVulkanInstance(&instance);
    window.setGraphicsDevice(QQuickGraphicsDevice::fromRhi(rhi.get()));
    const QSize size(800, 520);
    window.setGeometry(QRect(QPoint(), size));
    if (!control.initialize()) {
        return 4;
    }
    std::unique_ptr<QRhiTexture> texture(rhi->newTexture(QRhiTexture::RGBA8, size, 1,
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
    std::unique_ptr<QRhiRenderBuffer> depth(rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, size, 1));
    if (!texture->create() || !depth->create()) {
        return 5;
    }
    QRhiTextureRenderTargetDescription description(QRhiColorAttachment(texture.get()), depth.get());
    std::unique_ptr<QRhiTextureRenderTarget> target(rhi->newTextureRenderTarget(description));
    std::unique_ptr<QRhiRenderPassDescriptor> pass(target->newCompatibleRenderPassDescriptor());
    target->setRenderPassDescriptor(pass.get());
    if (!target->create()) {
        return 6;
    }
    window.setRenderTarget(QQuickRenderTarget::fromRhiRenderTarget(target.get()));
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(R"QML(
import QtQuick
import Kos.Ui as Kos
Item {
    width: 800; height: 520
    property int mode: 0
    property alias sampling: material.sampling
    property alias canSample: material.canSample
    // Source contains synthetic content only. Backdrop and consumer are siblings,
    // so capture excludes both the material itself and its foreground text.
    Rectangle {
        id: scene; anchors.fill: parent; color: "#f4f5f8"
        Rectangle { x: 50; y: 40; width: 300; height: 300; radius: 150; color: "#e6b9f2" }
        Rectangle { x: 430; y: 140; width: 310; height: 310; radius: 155; color: "#91d7ed" }
        Rectangle { x: 120; y: 350; width: 420; height: 65; radius: 24; color: "#f5c890" }
        Repeater {
            model: 12
            Rectangle { required property int index; x: 60 + index * 58; y: 80;
                width: 4; height: 360; color: index % 2 ? "#447584" : "#995e91"; opacity: 0.5 }
        }
    }
    Kos.LiquidBackdrop { id: sceneBackdrop; sourceItem: scene; registerAsDefault: false }
    Kos.LiquidPanelSurface {
        id: material; x: 180; y: 150; width: 440; height: 220
        visible: parent.mode !== 0
        backdrop: sceneBackdrop; cornerRadius: 26
        darkAppearance: false; reduceTransparency: parent.mode === 2
    }
    Text { x: 210; y: 190; text: "NkShot material probe"; color: "#24242a"; font.pixelSize: 26 }
    Text { x: 210; y: 235; text: "Synthetic scene · shared Kos.Ui"; color: "#454550"; font.pixelSize: 17 }
}
)QML", QUrl(QStringLiteral("qrc:/KosMaterialProbe.qml")));
    std::unique_ptr<QObject> object(component.create());
    auto *item = qobject_cast<QQuickItem *>(object.get());
    if (!item) {
        qCritical() << component.errors();
        return 7;
    }
    item->setParentItem(window.contentItem());
    QImage frames[3];
    const QStringList names{QStringLiteral("baseline"), QStringLiteral("liquid"), QStringLiteral("reduced-transparency")};
    int resultCode = 0;
    for (int mode = 0; mode < 3; ++mode) {
        item->setProperty("mode", mode);
        // Multiple complete frames settle callLater mapping and nested capture/effect layers.
        for (int frame = 0; frame < 6; ++frame) {
            QCoreApplication::processEvents();
            control.polishItems();
            control.beginFrame();
            control.sync();
            control.render();
            QRhiReadbackResult result;
            auto *updates = rhi->nextResourceUpdateBatch();
            updates->readBackTexture(QRhiReadbackDescription(texture.get()), &result);
            control.commandBuffer()->resourceUpdate(updates);
            control.endFrame();
            rhi->finish();
            frames[mode] = QImage(reinterpret_cast<const uchar *>(result.data.constData()),
                result.pixelSize.width(), result.pixelSize.height(), QImage::Format_RGBA8888).copy();
        }
        qInfo() << names[mode] << "canSample" << item->property("canSample") << "sampling" << item->property("sampling");
        if (item->property("sampling").toBool() != (mode == 1)) {
            resultCode = 8;
        }
        if (!frames[mode].save(QDir(directory).filePath(names[mode] + QStringLiteral(".png")))) {
            resultCode = 9;
        }
    }
    if (frames[0] == frames[1] || frames[1] == frames[2]) {
        qCritical() << "Expected different baseline, sampled, and opaque outputs";
        resultCode = 10;
    }
    control.invalidate();
    return resultCode;
}
