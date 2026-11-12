// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import Kos.Ui as Kos

// Sharing choices only. The owner supplies capabilities and the save-dialog action.
Kos.LiquidDialog {
    id: root
    objectName: "nkshotImageExportPopup"
    property bool jpegAvailable: false
    property bool webpAvailable: false
    property var exportCallback
    property int format: 0
    property string errorMessage: ""
    readonly property var formats: jpegAvailable ? (webpAvailable ? [0, 1] : [0]) : (webpAvailable ? [1] : [])
    width: Math.min(360, parent.width - 32)
    anchors.centerIn: parent
    sourceItem: parent
    focus: true
    header: null
    footer: null
    onOpened: {
        errorMessage = ""
        if (formats.indexOf(format) < 0) format = formats.length ? formats[0] : 0
    }
    contentItem: ColumnLayout {
        spacing: 14
        QQC.Label {
            text: i18nc("@title", "Export a sharing copy")
            font.pixelSize: 17; font.bold: true
            color: Kos.AppTheme.paletteText
        }
        Kos.LiquidSelect {
            objectName: "nkshotImageExportFormat"
            Layout.fillWidth: true
            implicitHeight: 28
            materialForm: true
            popupBorderWidth: 0
            appearanceMode: Kos.AppTheme.dark ? "dark" : "light"
            accentColor: Kos.AppTheme.accent
            focusRingColor: Kos.AppTheme.focusRing
            // Upstream selects dim their whole surface; dialogs express disability with text.
            opacity: 1
            textColor: enabled ? Kos.AppTheme.paletteText : Kos.AppTheme.disabledText
            mutedTextColor: enabled ? Kos.AppTheme.mutedText : Kos.AppTheme.disabledText
            model: root.formats.map(value => value === 0 ? "JPEG" : i18nc("@item", "WebP · Lossless"))
            currentIndex: root.formats.indexOf(root.format)
            enabled: root.formats.length > 1
            Accessible.name: i18nc("@label", "Sharing image format")
            onActivated: index => { root.format = root.formats[index]; root.errorMessage = "" }
        }
        RowLayout {
            Layout.fillWidth: true
            visible: root.format === 0 && root.jpegAvailable
            QQC.Label { text: i18nc("@label", "Quality"); color: Kos.AppTheme.paletteText }
            Kos.LiquidSelect {
                id: quality
                objectName: "nkshotImageExportQuality"
                Layout.fillWidth: true
                implicitHeight: 28
                materialForm: true
                popupBorderWidth: 0
                appearanceMode: Kos.AppTheme.dark ? "dark" : "light"
                accentColor: Kos.AppTheme.accent
                focusRingColor: Kos.AppTheme.focusRing
                opacity: 1
                textColor: enabled ? Kos.AppTheme.paletteText : Kos.AppTheme.disabledText
                mutedTextColor: enabled ? Kos.AppTheme.mutedText : Kos.AppTheme.disabledText
                readonly property var values: [90, 95, 80]
                model: [i18nc("@item", "High · 90"), i18nc("@item", "Highest · 95"), i18nc("@item", "Smaller · 80")]
                currentIndex: 0
                onActivated: index => currentIndex = index
                Accessible.name: i18nc("@label", "JPEG quality")
            }
        }
        QQC.Label {
            objectName: "nkshotImageExportFormatHint"
            Layout.fillWidth: true
            text: root.format === 0
                ? i18nc("@info", "JPEG uses lossy compression. Transparent areas become white.")
                : i18nc("@info", "WebP is lossless and preserves transparency.")
            wrapMode: Text.Wrap
            color: Kos.AppTheme.mutedText
        }
        QQC.Label {
            Layout.fillWidth: true
            text: i18nc("@info", "Only a copy is exported. This does not mark your edit as saved; use Save… for a PNG.")
            wrapMode: Text.Wrap
            color: Kos.AppTheme.mutedText
        }
        QQC.Label {
            objectName: "nkshotImageExportError"
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.formats.length ? root.errorMessage : i18nc("@info", "JPEG and WebP export are unavailable in this installation.")
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: Kos.AppTheme.paletteText
        }
        // Dialog actions are right-aligned throughout: Cancel precedes the primary action.
        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: 8
            Kos.LiquidActionButton {
                objectName: "nkshotImageExportCancel"
                text: i18nc("@action", "Cancel")
                implicitHeight: root.actionHeight
                materialForm: true
                onClicked: root.close()
            }
            Kos.LiquidActionButton {
                objectName: "nkshotImageExportContinue"
                text: i18nc("@action", "Continue…")
                enabled: root.formats.indexOf(root.format) >= 0 && typeof root.exportCallback === "function"
                implicitHeight: root.actionHeight
                materialForm: true
                highlighted: true
                onClicked: {
                    root.errorMessage = ""
                    if (root.exportCallback(root.format, root.format === 0 ? quality.values[quality.currentIndex] : 100)) root.close()
                    else if (!root.errorMessage.length) root.errorMessage = i18nc("@info", "The export dialog could not be opened. Try again.")
                }
            }
        }
    }
}
