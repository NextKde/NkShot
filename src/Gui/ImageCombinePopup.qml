// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import Kos.Ui as Kos

QQC.Popup {
    id: root
    objectName: "nkshotCombinePopup"
    property bool horizontal: false
    property bool keepAllPixels: false
    property bool busy: false
    signal chooseImages(bool keepAllPixels, bool horizontal)
    width: Math.min(380, parent.width - 32)
    anchors.centerIn: parent
    modal: true
    focus: true
    padding: 20
    palette.text: Kos.AppTheme.paletteText
    palette.windowText: Kos.AppTheme.paletteText
    palette.buttonText: Kos.AppTheme.paletteText
    background: Kos.KosSurface { fillColor: Kos.AppTheme.contentBackground; radius: 18; elevation: 0.5 }

    component Choice: QQC.Button {
        id: control
        Layout.fillWidth: true
        Layout.preferredWidth: 1
        implicitHeight: 36
        checkable: true
        autoExclusive: true
        palette.buttonText: checked ? Kos.AppTheme.accentText : Kos.AppTheme.paletteText
        background: Kos.KosSurface {
            fillColor: control.checked ? Kos.AppTheme.accent : Kos.AppTheme.controlBackground
            radius: 10; elevation: 0; strokeWidth: 0
            hovered: control.hovered; pressed: control.down; focused: control.visualFocus
        }
    }
    contentItem: ColumnLayout {
        spacing: 14
        QQC.Label {
            text: i18nc("@title", "Combine images")
            font.pixelSize: 17
            font.bold: true
            color: Kos.AppTheme.paletteText
        }
        QQC.Label {
            Layout.fillWidth: true
            text: i18nc("@info", "Create one image from 2–16 files. Your sources and current edit stay unchanged.")
            wrapMode: Text.Wrap
            color: Kos.AppTheme.mutedText
        }
        QQC.Label { text: i18nc("@label", "Direction"); color: Kos.AppTheme.paletteText }
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Choice {
                objectName: "nkshotCombineVertical"
                text: i18nc("@option", "Vertical")
                checked: !root.horizontal
                onClicked: root.horizontal = false
            }
            Choice {
                objectName: "nkshotCombineHorizontal"
                text: i18nc("@option", "Horizontal")
                checked: root.horizontal
                onClicked: root.horizontal = true
            }
        }
        QQC.Label { text: i18nc("@label", "Join method"); color: Kos.AppTheme.paletteText }
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Choice {
                objectName: "nkshotCombineMatch"
                text: i18nc("@option", "Match overlap")
                checked: !root.keepAllPixels
                onClicked: root.keepAllPixels = false
            }
            Choice {
                objectName: "nkshotCombineStack"
                text: i18nc("@option", "Keep all pixels")
                checked: root.keepAllPixels
                onClicked: root.keepAllPixels = true
            }
        }
        QQC.Label {
            objectName: "nkshotCombineMethodHint"
            Layout.fillWidth: true
            text: root.keepAllPixels
                ? i18nc("@info", "Join every pixel edge to edge, with no overlap removed.")
                : i18nc("@info", "Remove matching overlap between screenshots. Keep at least 24 pixels of unchanged overlap.")
            wrapMode: Text.Wrap
            color: Kos.AppTheme.mutedText
        }
        QQC.Label {
            objectName: "nkshotCombineOrderHint"
            Layout.fillWidth: true
            text: root.horizontal
                ? i18nc("@info", "Starts left → right by filename (1, 2, 10). Review and reorder next. Images need the same height, scale and color profile.")
                : i18nc("@info", "Starts top → bottom by filename (1, 2, 10). Review and reorder next. Images need the same width, scale and color profile.")
            wrapMode: Text.Wrap
            color: Kos.AppTheme.mutedText
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            QQC.Button {
                id: cancelButton
                objectName: "nkshotCombineCancel"
                text: i18nc("@action", "Cancel")
                implicitHeight: 36
                leftPadding: 14; rightPadding: 14
                background: Kos.KosSurface {
                    fillColor: Kos.AppTheme.controlBackground; radius: 10; elevation: 0; strokeWidth: 0
                    hovered: cancelButton.hovered; pressed: cancelButton.down; focused: cancelButton.visualFocus
                }
                onClicked: root.close()
            }
            Item { Layout.fillWidth: true }
            QQC.Button {
                id: chooseButton
                objectName: "nkshotCombineChoose"
                text: i18nc("@action", "Choose images…")
                enabled: !root.busy
                implicitHeight: 36
                leftPadding: 14; rightPadding: 14
                palette.buttonText: Kos.AppTheme.accentText
                background: Kos.KosSurface {
                    fillColor: Kos.AppTheme.accent; radius: 10; elevation: 0; strokeWidth: 0
                    hovered: chooseButton.hovered; pressed: chooseButton.down; focused: chooseButton.visualFocus
                }
                onClicked: {
                    root.close()
                    root.chooseImages(root.keepAllPixels, root.horizontal)
                }
            }
        }
    }
}
