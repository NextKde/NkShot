// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import Kos.Ui as Kos

Kos.LiquidDialog {
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
    sourceItem: parent
    focus: true
    header: null
    footer: null
    onOpened: { quarterTurns = 0; horizontalMirror = false; widthInput.value = Math.min(8192, Math.max(1, sourceSize.width)) }
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
            Kos.LiquidActionButton {
                objectName: "nkshotCopyRotate"
                Layout.fillWidth: true
                implicitHeight: 28
                materialForm: true
                text: i18nc("@action", "Rotate 90°")
                onClicked: { root.quarterTurns = (root.quarterTurns + 1) % 4; widthInput.value = Math.min(8192, root.orientedSize.width) }
            }
            Kos.LiquidActionButton {
                objectName: "nkshotCopyMirror"
                Layout.fillWidth: true
                implicitHeight: 28
                materialForm: true
                text: i18nc("@action", "Mirror")
                checkable: true
                checked: root.horizontalMirror
                onClicked: root.horizontalMirror = !root.horizontalMirror
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
            Kos.LiquidTextField {
                id: widthInput
                objectName: "nkshotResizeWidth"
                Layout.preferredWidth: 100
                implicitHeight: 28
                specificationForm: true
                property int value: 1
                text: String(value)
                validator: IntValidator { bottom: 1; top: 8192 }
                inputMethodHints: Qt.ImhDigitsOnly
                maximumLength: 4
                onTextEdited: { if (acceptableInput) value = Number(text) }
                onValueChanged: text = String(value)
                onEditingFinished: text = String(value)
                onActiveFocusChanged: { if (!activeFocus) text = String(value) }
                color: enabled ? Kos.AppTheme.paletteText : Kos.AppTheme.disabledText
                opacity: 1
                Accessible.name: i18nc("@label", "Copy width in pixels")
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
        // Dialog actions are right-aligned throughout: Cancel precedes the primary action.
        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: 8
            Kos.LiquidActionButton {
                text: i18nc("@action", "Cancel")
                implicitHeight: root.actionHeight
                materialForm: true
                onClicked: root.close()
            }
            Kos.LiquidActionButton {
                objectName: "nkshotResizeCreate"
                text: i18nc("@action", "Create copy")
                enabled: widthInput.acceptableInput && root.sourceSize.width > 0 && root.outputHeight <= 32000 && root.outputHeight * widthInput.value <= 40000000
                implicitHeight: root.actionHeight
                materialForm: true
                highlighted: true
                onClicked: { root.createCopy(widthInput.value, root.quarterTurns, root.horizontalMirror); root.close() }
            }
        }
    }
}
