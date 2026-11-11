// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import Kos.Ui as Kos

QQC.Pane {
    id: root
    padding: 16
    palette.windowText: Kos.AppTheme.paletteText
    palette.text: Kos.AppTheme.paletteText
    palette.buttonText: Kos.AppTheme.paletteText
    background: Rectangle { color: Kos.AppTheme.contentSecondaryBackground }
    ColumnLayout {
        anchors.fill: parent
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            QQC.Label {
                Layout.fillWidth: true
                text: ocrWindow.heading
                font.pixelSize: 17
                font.weight: Font.DemiBold
                color: Kos.AppTheme.paletteText
            }
            QQC.ToolButton {
                id: copyButton
                objectName: "nkshotOcrCopy"
                property bool copied: false
                text: copied ? i18nc("@info", "Copied") : i18nc("@action", "Copy all")
                icon.name: copied ? "dialog-ok" : "edit-copy"
                icon.color: Kos.AppTheme.paletteText
                display: QQC.AbstractButton.TextBesideIcon
                onClicked: ocrWindow.copyText()
                Connections {
                    target: ocrWindow
                    function onTextCopied() { copyButton.copied = true; feedback.restart() }
                }
                Timer { id: feedback; interval: 1800; onTriggered: copyButton.copied = false }
                background: Kos.KosSurface {
                    fillColor: Kos.AppTheme.controlBackground
                    radius: 10
                    elevation: 0
                    strokeWidth: 0
                }
            }
            QQC.ToolButton {
                objectName: "nkshotOcrSave"
                text: i18nc("@action", "Save text as…")
                icon.name: "document-save-as"
                icon.color: Kos.AppTheme.paletteText
                display: QQC.AbstractButton.IconOnly
                QQC.ToolTip.text: text
                QQC.ToolTip.visible: hovered
                onClicked: ocrWindow.saveAs()
            }
            QQC.ToolButton {
                objectName: "nkshotOcrClose"
                text: i18nc("@action", "Close")
                icon.name: "dialog-close"
                icon.color: Kos.AppTheme.paletteText
                display: QQC.AbstractButton.IconOnly
                QQC.ToolTip.text: text
                QQC.ToolTip.visible: hovered
                onClicked: ocrWindow.close()
            }
        }
        QQC.Label {
            Layout.fillWidth: true
            text: ocrWindow.description
            wrapMode: Text.Wrap
            color: Kos.AppTheme.mutedText
            font.pixelSize: 12
        }
        QQC.Label {
            id: saveStatus
            objectName: "nkshotOcrSaveStatus"
            Layout.fillWidth: true
            visible: text.length > 0
            wrapMode: Text.Wrap
            color: Kos.AppTheme.paletteText
            font.pixelSize: 12
            Connections {
                target: ocrWindow
                function onSaveFinished(message, success) { saveStatus.text = message }
            }
        }
        QQC.ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            QQC.TextArea {
                objectName: "nkshotOcrText"
                text: ocrWindow.recognizedText
                textFormat: TextEdit.PlainText
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.Wrap
                color: Kos.AppTheme.paletteText
                selectedTextColor: Kos.AppTheme.accentText
                selectionColor: Kos.AppTheme.accent
                font.pixelSize: 15
                padding: 14
                background: Kos.KosSurface {
                    fillColor: Kos.AppTheme.contentBackground
                    radius: 12
                    elevation: 0
                }
            }
        }
    }
}
