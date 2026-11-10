// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import Kos.Ui as Kos

QQC.Pane {
    id: root
    padding: 12
    palette.windowText: Kos.AppTheme.paletteText
    palette.text: Kos.AppTheme.paletteText
    palette.buttonText: Kos.AppTheme.paletteText
    background: Kos.KosSurface {
        fillColor: Kos.AppTheme.contentSecondaryBackground
        radius: 14
        elevation: 0
    }
    component PinButton: QQC.ToolButton {
        icon.color: Kos.AppTheme.paletteText
        implicitHeight: 36
        QQC.ToolTip.visible: hovered
        QQC.ToolTip.text: text
        background: Kos.KosSurface {
            fillColor: parent.checked || parent.hovered ? Kos.AppTheme.controlBackground : "transparent"
            radius: 9
            elevation: 0
            strokeWidth: 0
        }
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        RowLayout {
            Layout.fillWidth: true
            PinButton {
                objectName: "nkshotPinTop"
                text: i18nc("@action", "Stay on Top")
                icon.name: "window-pin"
                display: QQC.AbstractButton.IconOnly
                checkable: true
                checked: pinnedWindow.stayOnTop
                onClicked: pinnedWindow.stayOnTop = checked
            }
            QQC.Label {
                text: pinnedWindow.snapshotSize.width + " × " + pinnedWindow.snapshotSize.height
                color: Kos.AppTheme.paletteText
            }
            Item { Layout.fillWidth: true }
            PinButton {
                objectName: "nkshotPinCopy"
                text: i18nc("@action", "Copy")
                icon.name: "edit-copy"
                onClicked: pinnedWindow.copyImage()
            }
            PinButton {
                objectName: "nkshotPinClose"
                text: i18nc("@action", "Close")
                display: QQC.AbstractButton.IconOnly
                icon.name: "window-close"
                onClicked: pinnedWindow.close()
            }
        }
        Image {
            objectName: "nkshotPinSnapshot"
            Layout.fillWidth: true
            Layout.fillHeight: true
            source: pinnedWindow.imageSource
            fillMode: Image.PreserveAspectFit
            smooth: true
        }
    }
}
