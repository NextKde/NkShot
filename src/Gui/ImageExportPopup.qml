// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import Kos.Ui as Kos

// Sharing choices only. The owner supplies capabilities and the save-dialog action.
QQC.Popup {
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
    modal: true
    focus: true
    padding: 20
    palette.text: Kos.AppTheme.paletteText
    palette.windowText: Kos.AppTheme.paletteText
    palette.buttonText: Kos.AppTheme.paletteText
    onOpened: {
        errorMessage = ""
        if (formats.indexOf(format) < 0) format = formats.length ? formats[0] : 0
    }
    background: Kos.KosSurface { fillColor: Kos.AppTheme.contentBackground; radius: 18; elevation: 0.5 }
    contentItem: ColumnLayout {
        spacing: 14
        QQC.Label {
            text: i18nc("@title", "Export a sharing copy")
            font.pixelSize: 17; font.bold: true
            color: Kos.AppTheme.paletteText
        }
        QQC.ComboBox {
            objectName: "nkshotImageExportFormat"
            Layout.fillWidth: true
            model: root.formats.map(value => value === 0 ? "JPEG" : i18nc("@item", "WebP · Lossless"))
            currentIndex: root.formats.indexOf(root.format)
            enabled: root.formats.length > 1
            Accessible.name: i18nc("@label", "Sharing image format")
            onActivated: {root.format = root.formats[currentIndex]; root.errorMessage = ""}
            background: Kos.KosSurface { fillColor: Kos.AppTheme.controlBackground; radius: 9; elevation: 0 }
        }
        RowLayout {
            Layout.fillWidth: true
            visible: root.format === 0 && root.jpegAvailable
            QQC.Label { text: i18nc("@label", "Quality"); color: Kos.AppTheme.paletteText }
            QQC.ComboBox {
                id: quality
                objectName: "nkshotImageExportQuality"
                Layout.fillWidth: true
                readonly property var values: [90, 95, 80]
                model: [i18nc("@item", "High · 90"), i18nc("@item", "Highest · 95"), i18nc("@item", "Smaller · 80")]
                currentIndex: 0
                Accessible.name: i18nc("@label", "JPEG quality")
                background: Kos.KosSurface { fillColor: Kos.AppTheme.controlBackground; radius: 9; elevation: 0 }
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
        RowLayout {
            Layout.fillWidth: true
            QQC.Button {
                id: cancel
                objectName: "nkshotImageExportCancel"
                text: i18nc("@action", "Cancel")
                implicitHeight: 34; leftPadding: 14; rightPadding: 14
                onClicked: root.close()
                background: Kos.KosSurface { fillColor: Kos.AppTheme.controlBackground; radius: 10; elevation: 0; focused: cancel.visualFocus }
            }
            Item { Layout.fillWidth: true }
            QQC.Button {
                id: proceed
                objectName: "nkshotImageExportContinue"
                text: i18nc("@action", "Continue…")
                enabled: root.formats.indexOf(root.format) >= 0 && typeof root.exportCallback === "function"
                implicitHeight: 34; leftPadding: 14; rightPadding: 14
                palette.buttonText: Kos.AppTheme.accentText
                onClicked: {
                    root.errorMessage = ""
                    if (root.exportCallback(root.format, root.format === 0 ? quality.values[quality.currentIndex] : 100)) root.close()
                    else if (!root.errorMessage.length) root.errorMessage = i18nc("@info", "The export dialog could not be opened. Try again.")
                }
                background: Kos.KosSurface { fillColor: Kos.AppTheme.accent; radius: 10; elevation: 0; focused: proceed.visualFocus }
            }
        }
    }
}
