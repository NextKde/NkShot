/* SPDX-FileCopyrightText: 2025 Noah Davis <noahadvs@gmail.com>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls as QQC
import Kos.Ui as Kos
import org.kde.kirigami as Kirigami
import org.kde.spectacle.private

QQC.ToolButton {
    id: root
    // Glyph from the pinned Kos.Icons font, named the way Spectacle names it
    // upstream (freedesktop icon name). Falls back to icon.name so buttons that
    // take their icon from an Action keep working: SymbolMap translates either
    // form.
    property string symbol: ""
    icon.color: enabled ? palette.buttonText : Kos.AppTheme.disabledText
    icon.width: QmlUtils.toolbarIconSize
    icon.height: QmlUtils.toolbarIconSize
    implicitHeight: QmlUtils.toolbarButtonHeight
    topPadding: (height - Math.max(QmlUtils.toolbarIconSize, implicitContentHeight)) / 2
    bottomPadding: topPadding
    leftPadding: display === QQC.ToolButton.IconOnly ? (height - QmlUtils.toolbarIconSize) / 2 : 8
    rightPadding: leftPadding
    width: display === QQC.ToolButton.IconOnly ? height : implicitWidth
    // Keep actions, accessibility and keyboard focus native. Only the
    // surrounding capsule is custom drawn.
    background: Rectangle {
        radius: height / 2
        color: root.down || root.checked ? Qt.alpha(root.palette.windowText, 0.13)
            : root.hovered ? Qt.alpha(root.palette.windowText, 0.07)
            : "transparent"
        Rectangle {
            anchors.fill: parent
            anchors.margins: -2
            radius: parent.radius + 2
            color: "transparent"
            border.width: 2
            border.color: Kos.AppTheme.focusRing
            visible: root.visualFocus
        }
        Behavior on color {
            ColorAnimation { duration: Settings.reduceUiMotion ? 0 : Kirigami.Units.shortDuration }
        }
    }
    contentItem: SymbolLabel {
        symbol: root.symbol
            || (root.action && root.action.symbol !== undefined ? root.action.symbol : "")
            || root.icon.name
        text: root.text
        display: root.display
        spacing: Kirigami.Units.smallSpacing
        color: root.enabled ? root.palette.buttonText : Kos.AppTheme.disabledText
        iconSize: QmlUtils.toolbarIconSize
        mirrored: root.mirrored
        font: root.font
    }
    QQC.ToolTip.text: text
    QQC.ToolTip.visible: (hovered || pressed) && display === QQC.ToolButton.IconOnly
    QQC.ToolTip.delay: Kirigami.Units.toolTipDelay
}
