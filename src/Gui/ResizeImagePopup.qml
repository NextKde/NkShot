// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import Kos.Ui as Kos

QQC.Popup {
    id: root
    objectName: "nkshotResizePopup"
    property size sourceSize: Qt.size(0, 0)
    property int quarterTurns: 0
    property bool horizontalMirror: false
    readonly property size orientedSize: quarterTurns % 2 ? Qt.size(sourceSize.height, sourceSize.width) : sourceSize
    readonly property int outputHeight: sourceSize.width > 0 ? Math.max(1, Math.round(orientedSize.height * widthInput.value / orientedSize.width)) : 0
    signal createCopy(int pixelWidth, int quarterTurns, bool mirror)
    width: Math.min(360, parent.width - 32)
    anchors.centerIn: parent
    palette.text: Kos.AppTheme.paletteText
    palette.windowText: Kos.AppTheme.paletteText
    palette.buttonText: Kos.AppTheme.paletteText
    modal: true
    focus: true
    padding: 20
    onOpened: { quarterTurns = 0; horizontalMirror = false; widthInput.value = Math.min(8192, Math.max(1, sourceSize.width)) }
    background: Kos.KosSurface { fillColor: Kos.AppTheme.contentBackground; radius: 18; elevation: 0.5 }
    contentItem: ColumnLayout {
        spacing: 14
        QQC.Label {
            text: i18nc("@title", "Size and orientation")
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
            spacing: 8
            QQC.Button {
                id: rotateButton
                objectName: "nkshotCopyRotate"
                Layout.fillWidth: true
                implicitHeight: 36
                text: i18nc("@action", "Rotate 90°")
                font: Qt.application.font
                background: Kos.KosSurface {
                    fillColor: Kos.AppTheme.controlBackground; radius: 10; elevation: 0; strokeWidth: 0
                    hovered: rotateButton.hovered; pressed: rotateButton.down; focused: rotateButton.visualFocus
                }
                onClicked: { root.quarterTurns = (root.quarterTurns + 1) % 4; widthInput.value = Math.min(8192, root.orientedSize.width) }
            }
            QQC.Button {
                id: mirrorButton
                objectName: "nkshotCopyMirror"
                Layout.fillWidth: true
                implicitHeight: 36
                text: i18nc("@action", "Mirror")
                font: Qt.application.font
                checkable: true
                checked: root.horizontalMirror
                onClicked: root.horizontalMirror = !root.horizontalMirror
                background: Kos.KosSurface {
                    fillColor: root.horizontalMirror ? Kos.AppTheme.accent : Kos.AppTheme.controlBackground
                    radius: 10; elevation: 0; strokeWidth: 0
                    hovered: mirrorButton.hovered; pressed: mirrorButton.down; focused: mirrorButton.visualFocus
                }
                palette.buttonText: root.horizontalMirror ? Kos.AppTheme.accentText : Kos.AppTheme.paletteText
                Accessible.description: i18nc("@info", "Mirror horizontally after rotation")
            }
        }
        QQC.Label {
            objectName: "nkshotCopyOrientation"
            Layout.fillWidth: true
            text: root.horizontalMirror ? i18nc("@info", "%1° clockwise · mirrored", root.quarterTurns * 90)
                : i18nc("@info", "%1° clockwise", root.quarterTurns * 90)
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
                onClicked: { root.createCopy(widthInput.value, root.quarterTurns, root.horizontalMirror); root.close() }
            }
        }
    }
}
