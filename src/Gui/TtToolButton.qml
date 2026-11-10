/* SPDX-FileCopyrightText: 2025 Noah Davis <noahadvs@gmail.com>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls as QQC
import org.kde.kirigami as Kirigami
import org.kde.spectacle.private

QQC.ToolButton {
    id: root
    opacity: enabled ? 1 : 0.38
    icon.color: palette.buttonText
    implicitHeight: QmlUtils.iconTextButtonHeight
    width: display === QQC.ToolButton.IconOnly ? height : implicitWidth
    // Keep the native content item, actions, accessibility and keyboard focus.
    // Only the surrounding capsule is custom drawn.
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
    QQC.ToolTip.text: text
    QQC.ToolTip.visible: (hovered || pressed) && display === QQC.ToolButton.IconOnly
    QQC.ToolTip.delay: Kirigami.Units.toolTipDelay
}
