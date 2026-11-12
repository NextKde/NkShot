/* SPDX-FileCopyrightText: 2022 Noah Davis <noahadvs@gmail.com>
 * SPDX-FileCopyrightText: 2023 Aleix Pol Gonzalez <aleixpol@kde.org>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

import QtQuick
import QtQuick.Window
import QtQuick.Effects
import Kos.Ui as Kos
import org.kde.kquickimageeditor

Item {
    id: root
    required property AnnotationViewport viewport
    required property point targetPoint
    property int factor: 3
    readonly property color crosshairColor: Qt.alpha(Kos.AppTheme.paletteText, 0.6)
    readonly property real hairline: 1 / Screen.devicePixelRatio

    implicitWidth: 67 * factor
    implicitHeight: implicitWidth

    FloatingBackground {
        anchors.fill: parent
        radius: width / 2
        color: Kos.AppTheme.contentBackground
        shadow.color: Qt.rgba(0, 0, 0, 0.16)
        shadow.size: 16
        shadow.yOffset: 4
    }

    Rectangle {
        id: lens
        anchors.fill: parent
        radius: width / 2
        clip: true
        color: "transparent"
        // Item.clip is rectangular; mask the sampled pixels and crosshair as a circle.
        layer.enabled: true
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: circleMask
            maskThresholdMin: 0.5
            maskSpreadAtMin: 1
        }

        ShaderEffectSource {
            anchors.fill: parent
            sourceItem: root.viewport
            // Preserve the odd-sized 3x pixel grid and its center-pixel offset.
            sourceRect: Qt.rect((root.targetPoint.x - root.viewport.viewportRect.x) - 33,
                                (root.targetPoint.y - root.viewport.viewportRect.y) - 33,
                                67, 67)
            smooth: false
        }

        Item {
            id: center
            x: contextWindow.dprRound((parent.width - width) / 2)
            y: contextWindow.dprRound((parent.height - height) / 2)
            width: root.factor * 3
            height: root.factor * 3
        }
        Rectangle { // top
            anchors.top: parent.top
            anchors.bottom: center.top
            x: contextWindow.dprRound((parent.width - width) / 2)
            width: root.hairline
            color: root.crosshairColor
        }
        Rectangle { // bottom
            anchors.bottom: parent.bottom
            anchors.top: center.bottom
            x: contextWindow.dprRound((parent.width - width) / 2)
            width: root.hairline
            color: root.crosshairColor
        }
        Rectangle { // left
            anchors.left: parent.left
            anchors.right: center.left
            y: contextWindow.dprRound((parent.height - height) / 2)
            height: root.hairline
            color: root.crosshairColor
        }
        Rectangle { // right
            anchors.right: parent.right
            anchors.left: center.right
            y: contextWindow.dprRound((parent.height - height) / 2)
            height: root.hairline
            color: root.crosshairColor
        }
    }
    Rectangle {
        id: circleMask
        width: lens.width
        height: lens.height
        radius: width / 2
        color: "white"
        visible: false
        layer.enabled: true
    }
    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: "transparent"
        border.width: root.hairline
        border.color: Qt.alpha(Kos.AppTheme.paletteText, 0.3)
    }
}
