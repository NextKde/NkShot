// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Window
import QtQuick.Controls as QQC
import Kos.Ui as Kos

Kos.LiquidCheckBox {
    id: root
    implicitHeight: 28
    opacity: 1
    // Keep the shared glass indicator without its upstream blue focus ring.
    indicator: Kos.LiquidSurface {
        implicitWidth: 22
        implicitHeight: 22
        x: root.leftPadding
        y: (root.height - height) / 2
        radius: 6
        backdrop: Kos.LiquidStyle.backdropForItem(root, root.Window.window)
        tintColor: root.checked ? Kos.AppTheme.accent : "#ffffff"
        tintStrength: root.checked ? 0.38 : 0.05
        Kos.VectorIcon {
            anchors.centerIn: parent
            name: "check"
            size: 15
            visible: root.checked
            color: root.enabled ? Kos.AppTheme.paletteText : Kos.AppTheme.disabledText
        }
    }
    contentItem: QQC.Label {
        text: root.text
        textFormat: Text.PlainText
        font: root.font
        leftPadding: root.indicator.width + root.spacing
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        color: root.enabled ? Kos.AppTheme.paletteText : Kos.AppTheme.disabledText
    }
}
