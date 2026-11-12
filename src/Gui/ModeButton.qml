/* SPDX-FileCopyrightText: 2026 NextKde
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls as QQC
import Kos.Ui as Kos
import org.kde.spectacle.private

// Keep DelayButton's externally driven progress for capture countdowns.
QQC.DelayButton {
    id: root
    implicitHeight: surface.implicitHeight
    implicitWidth: implicitContentWidth + leftPadding + rightPadding
    leftPadding: 8
    rightPadding: leftPadding
    topPadding: 4
    bottomPadding: topPadding
    font.pixelSize: 13
    icon.width: QmlUtils.toolbarIconSize
    icon.height: QmlUtils.toolbarIconSize
    icon.color: enabled ? Kos.AppTheme.paletteText : Kos.AppTheme.disabledText
    delay: 1
    transition: null
    contentItem: SymbolLabel { control: root }
    background: Kos.LiquidActionButton {
        id: surface
        materialForm: true
        // Compact solid actions (<28px) use the shared 6px corner rule.
        implicitHeight: 26
        cornerRadius: 6
        focusPolicy: Qt.NoFocus
        hoverEnabled: false
        down: root.down
        contentItem: Item {}
        Rectangle {
            anchors.fill: parent
            radius: 6
            color: Qt.alpha(Kos.AppTheme.paletteText, 0.07)
            visible: root.hovered && !root.down
        }
        Rectangle {
            x: root.leftPadding
            y: parent.height - height - 2
            width: Math.max(0, parent.width - root.leftPadding - root.rightPadding) * root.progress
            height: 1
            radius: height / 2
            color: Kos.AppTheme.paletteText
            visible: root.progress > 0
        }
    }
}
