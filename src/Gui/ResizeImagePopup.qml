// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import Kos.Ui as Kos

QQC.Popup {
    id: root
    objectName: "nkshotResizePopup"
    property size sourceSize: Qt.size(0, 0)
    readonly property int outputHeight: sourceSize.width > 0 ? Math.max(1, Math.round(sourceSize.height * widthInput.value / sourceSize.width)) : 0
    signal createCopy(int pixelWidth)
    width: Math.min(360, parent.width - 32)
    anchors.centerIn: parent
    palette.text: Kos.AppTheme.paletteText
    palette.windowText: Kos.AppTheme.paletteText
    palette.buttonText: Kos.AppTheme.paletteText
    modal: true
    focus: true
    padding: 20
    onOpened: widthInput.value = Math.min(8192, Math.max(1, sourceSize.width))
    background: Kos.KosSurface { fillColor: Kos.AppTheme.contentBackground; radius: 18; elevation: 0.5 }
    contentItem: ColumnLayout {
        spacing: 14
        QQC.Label {
            text: i18nc("@title", "Resize a copy")
            font.pixelSize: 17
            font.bold: true
            color: Kos.AppTheme.paletteText
        }
        QQC.Label {
            Layout.fillWidth: true
            text: i18nc("@info", "Keep the proportions. Your original image and edits stay unchanged.")
            wrapMode: Text.Wrap
            color: Kos.AppTheme.mutedText
        }
        RowLayout {
            Layout.fillWidth: true
            QQC.Label { text: i18nc("@label", "Width (px)"); color: Kos.AppTheme.paletteText; Layout.fillWidth: true }
            QQC.SpinBox {
                id: widthInput
                objectName: "nkshotResizeWidth"
                from: 1
                to: 8192
                editable: true
                Accessible.name: i18nc("@label", "Copy width in pixels")
                palette.text: Kos.AppTheme.paletteText
                background: Kos.KosSurface { fillColor: Kos.AppTheme.controlBackground; radius: 9; elevation: 0 }
            }
        }
        QQC.Label {
            Layout.fillWidth: true
            text: widthInput.value + " × " + root.outputHeight + " px"
            color: Kos.AppTheme.mutedText
        }
        QQC.Label {
            Layout.fillWidth: true
            visible: root.outputHeight > 32000 || root.outputHeight * widthInput.value > 40000000
            text: i18nc("@info", "Choose a smaller width. The copy must stay within 40 megapixels and 32000 pixels tall.")
            wrapMode: Text.Wrap
            color: Kos.AppTheme.paletteText
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            QQC.Button {
                id: cancelButton
                text: i18nc("@action", "Cancel")
                implicitHeight: 34
                leftPadding: 14
                rightPadding: 14
                background: Kos.KosSurface {
                    fillColor: Kos.AppTheme.controlBackground; radius: 10; elevation: 0; strokeWidth: 0
                    hovered: cancelButton.hovered; pressed: cancelButton.down; focused: cancelButton.visualFocus
                }
                onClicked: root.close()
            }
            QQC.Button {
                id: createButton
                objectName: "nkshotResizeCreate"
                text: i18nc("@action", "Create copy")
                enabled: root.sourceSize.width > 0 && root.outputHeight <= 32000 && root.outputHeight * widthInput.value <= 40000000
                palette.buttonText: Kos.AppTheme.accentText
                leftPadding: 14
                rightPadding: 14
                implicitHeight: 34
                background: Kos.KosSurface {
                    fillColor: Kos.AppTheme.accent; radius: 10; elevation: 0; strokeWidth: 0
                    hovered: createButton.hovered; pressed: createButton.down; focused: createButton.visualFocus
                }
                onClicked: { root.createCopy(widthInput.value); root.close() }
            }
        }
    }
}
