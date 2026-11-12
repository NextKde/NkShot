// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import Kos.Ui as Kos

QQC.Pane {
    id: root
    // This decorated utility window keeps a compact 16px content inset.
    padding: 16
    font: Qt.application.font
    palette.windowText: Kos.AppTheme.paletteText
    palette.text: Kos.AppTheme.paletteText
    palette.buttonText: Kos.AppTheme.paletteText
    background: Rectangle { color: Kos.AppTheme.contentSecondaryBackground }

    component ActionButton: Kos.LiquidActionButton {
        id: button
        implicitHeight: 30
        // Measure the caption independently of SymbolLabel's allocated width.
        implicitWidth: iconOnly ? implicitHeight : Math.max(implicitHeight,
            captionMetrics.advanceWidth(text) + (icon.name.length && display !== QQC.AbstractButton.TextOnly ? 20 : 0)
            + leftPadding + rightPadding)
        FontMetrics { id: captionMetrics; font: button.font }
        materialForm: true
        iconOnly: display === QQC.AbstractButton.IconOnly
        font.pixelSize: 13
        // Utility-window actions use feedback from the surface, without a focus ring.
        focusRingColor: "transparent"
        contentItem: SymbolLabel { control: parent; color: parent.textColor }
        QQC.ToolTip.text: text
        QQC.ToolTip.visible: hovered
        Accessible.name: text
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            QQC.Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                elide: Text.ElideRight
                text: ocrWindow.heading
                font.pixelSize: 17
                font.weight: Font.DemiBold
                color: Kos.AppTheme.paletteText
            }
            ActionButton {
                id: copyButton
                objectName: "nkshotOcrCopy"
                property bool copied: false
                text: copied ? i18nc("@info", "Copied") : ocrWindow.translation.translatedText.length ? i18nc("@action", "Copy original") : i18nc("@action", "Copy all")
                icon.name: copied ? "dialog-ok" : "edit-copy"
                display: root.width < 520 ? QQC.AbstractButton.IconOnly : QQC.AbstractButton.TextBesideIcon
                onClicked: ocrWindow.copyText()
                Connections {
                    target: ocrWindow
                    function onTextCopied() { copyButton.copied = true; feedback.restart() }
                }
                Timer { id: feedback; interval: 1800; onTriggered: copyButton.copied = false }
            }
            ActionButton {
                objectName: "nkshotOcrSave"
                text: i18nc("@action", "Save text as…")
                icon.name: "document-save-as"
                display: QQC.AbstractButton.IconOnly
                onClicked: ocrWindow.saveAs()
            }
            ActionButton {
                objectName: "nkshotOcrClose"
                text: i18nc("@action", "Close")
                icon.name: "dialog-close"
                display: QQC.AbstractButton.IconOnly
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
        RowLayout {
            visible: ocrWindow.translationEnabled
            Layout.fillWidth: true
            Kos.LiquidSelect {
                id: direction
                objectName: "nkshotTranslationDirection"
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                implicitHeight: 30
                materialForm: true
                popupBorderWidth: 0
                appearanceMode: Kos.AppTheme.dark ? "dark" : "light"
                accentColor: Kos.AppTheme.accent
                focusRingColor: "transparent"
                opacity: 1
                textColor: enabled ? Kos.AppTheme.paletteText : Kos.AppTheme.disabledText
                mutedTextColor: enabled ? Kos.AppTheme.mutedText : Kos.AppTheme.disabledText
                model: [i18nc("@item", "English → Chinese"), i18nc("@item", "Chinese → English")]
                Accessible.name: i18nc("@label", "Translation direction")
                onActivated: index => {
                    currentIndex = index
                    ocrWindow.translation.cancel()
                }
            }
            ActionButton {
                objectName: "nkshotTranslate"
                highlighted: true
                text: ocrWindow.translation.busy ? i18nc("@action", "Cancel") : i18nc("@action", "Translate")
                onClicked: {
                    if (ocrWindow.translation.busy) ocrWindow.translation.cancel()
                    else ocrWindow.translation.translate(ocrWindow.recognizedText, direction.currentIndex === 0 ? "en_zh" : "zh_en")
                }
            }
        }
        QQC.Label {
            objectName: "nkshotTranslationStatus"
            Layout.fillWidth: true
            visible: ocrWindow.translation.status.length > 0 || ocrWindow.translation.busy
            text: ocrWindow.translation.busy ? i18nc("@info", "Translating on this device…") : ocrWindow.translation.status
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: Kos.AppTheme.mutedText
            font.pixelSize: 12
        }
        ActionButton {
            objectName: "nkshotTranslationSetup"
            readonly property url guideUrl: "https://github.com/NextKde/NkShot/blob/main/docs/local-translation.md"
            visible: ocrWindow.translationEnabled && !ocrWindow.translation.busy
                && ocrWindow.translation.status.length > 0 && ocrWindow.translation.translatedText.length === 0
            text: i18nc("@action", "Set up offline translation")
            onClicked: Qt.openUrlExternally(guideUrl)
        }
        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: root.width >= 760 ? 2 : 1
            uniformCellWidths: true
            columnSpacing: 12
            rowSpacing: 12
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 0
                Layout.minimumHeight: 60
                RowLayout {
                    visible: translatedPane.visible
                    Layout.fillWidth: true
                    Layout.minimumHeight: translationHeader.implicitHeight
                    QQC.Label { text: i18nc("@label", "Original"); color: Kos.AppTheme.mutedText; Layout.fillWidth: true }
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
                        font.pixelSize: 13
                        padding: 12
                        background: Kos.KosSurface { fillColor: Kos.AppTheme.contentBackground; radius: 8; elevation: 0; strokeWidth: 0 }
                    }
                }
            }
            ColumnLayout {
                id: translatedPane
                visible: ocrWindow.translation.busy || ocrWindow.translation.translatedText.length > 0
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 0
                Layout.minimumHeight: 60
                RowLayout {
                    id: translationHeader
                    Layout.fillWidth: true
                    QQC.Label { text: i18nc("@label", "Translation"); color: Kos.AppTheme.mutedText; Layout.fillWidth: true }
                    ActionButton {
                        objectName: "nkshotCopyTranslation"
                        text: i18nc("@action", "Copy translation")
                        icon.name: "edit-copy"
                        enabled: ocrWindow.translation.translatedText.length > 0
                        onClicked: ocrWindow.copyTranslation()
                    }
                }
                QQC.ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    QQC.TextArea {
                        objectName: "nkshotTranslatedText"
                        text: ocrWindow.translation.translatedText
                        textFormat: TextEdit.PlainText
                        readOnly: true
                        selectByMouse: true
                        wrapMode: TextEdit.Wrap
                        color: Kos.AppTheme.paletteText
                        selectedTextColor: Kos.AppTheme.accentText
                        selectionColor: Kos.AppTheme.accent
                        font.pixelSize: 13
                        padding: 12
                        background: Kos.KosSurface { fillColor: Kos.AppTheme.contentBackground; radius: 8; elevation: 0; strokeWidth: 0 }
                    }
                }
            }
        }
    }
}
