/* SPDX-FileCopyrightText: 2022 Noah Davis <noahadvs@gmail.com>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

import QtQuick
import QtQuick.Templates as T
import QtQuick.Window
import Kos.Ui as Kos
import org.kde.kirigami as Kirigami
import org.kde.spectacle.private

T.Pane {
    id: root
    // Same canonical material used by NextKde apps, with no compositor blur request.
    property real radius: Kirigami.Units.largeSpacing * 2
    property real topLeftRadius: radius
    property real topRightRadius: radius
    property real bottomLeftRadius: radius
    property real bottomRightRadius: radius
    property real backgroundColorOpacity: Kos.LiquidStyle.panelMaterialOpacity
    property Kos.LiquidBackdrop backdrop: Kos.LiquidStyle.backdropFor(root.Window.window)
    readonly property bool uniformCorners: topLeftRadius === radius && topRightRadius === radius
        && bottomLeftRadius === radius && bottomRightRadius === radius

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            contentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding)

    padding: Kirigami.Units.largeSpacing
    spacing: Kirigami.Units.mediumSpacing

    background: Item {
        Kos.LiquidPanelSurface {
            anchors.fill: parent
            visible: root.uniformCorners
            backdrop: root.backdrop
            cornerRadius: root.radius
            materialOpacity: root.backgroundColorOpacity
            tintColor: root.palette.window
            darkAppearance: Kos.AppTheme.relativeLuminance(root.palette.window) < 0.28
            reduceTransparency: !Settings.useLiquidMaterial
        }
        // Joined annotation-option panels need independent corner radii, which
        // the shared material does not expose. Keep a truthful opaque fallback.
        FloatingBackground {
            anchors.fill: parent
            visible: !root.uniformCorners
            color: root.palette.window
            border.color: Qt.alpha(root.palette.windowText, 0.16)
            radius: root.radius
            corners.topLeftRadius: root.topLeftRadius
            corners.topRightRadius: root.topRightRadius
            corners.bottomLeftRadius: root.bottomLeftRadius
            corners.bottomRightRadius: root.bottomRightRadius
            border.width: contextWindow.dprRound(1)
        }
    }
}
