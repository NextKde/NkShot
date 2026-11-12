/* SPDX-FileCopyrightText: 2025 Noah Davis <noahadvs@gmail.com>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls as QQC
import org.kde.kirigami as Kirigami
import org.kde.spectacle.private

QQC.ToolButton {
    id: root
    // Glyph from the pinned Kos.Icons font, named the way Spectacle names it
    // upstream (freedesktop icon name). Falls back to icon.name so buttons that
    // take their icon from an Action keep working: SymbolMap translates either
    // form.
    property string symbol: ""
    opacity: enabled ? 1 : 0.38
    icon.color: palette.buttonText
    implicitHeight: QmlUtils.iconTextButtonHeight
    width: display === QQC.ToolButton.IconOnly ? height : implicitWidth
    // Keep actions, accessibility and keyboard focus native. Only the
    // surrounding capsule is custom drawn.
    background: Rectangle {
        radius: Kirigami.Units.mediumSpacing
        color: root.down ? Qt.alpha(root.palette.highlight, 0.28)
            : root.checked ? Qt.alpha(root.palette.highlight, 0.18)
            : root.hovered ? Qt.alpha(root.palette.windowText, 0.10)
            : "transparent"
        border.width: root.visualFocus ? 2 : 0
        border.color: root.palette.highlight
        opacity: root.enabled ? 1 : 0.5
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
        color: root.palette.buttonText
        iconSize: Math.max(root.icon.height, 16)
        mirrored: root.mirrored
        font: root.font
    }
    QQC.ToolTip.text: text
    QQC.ToolTip.visible: (hovered || pressed) && display === QQC.ToolButton.IconOnly
    QQC.ToolTip.delay: Kirigami.Units.toolTipDelay
}
