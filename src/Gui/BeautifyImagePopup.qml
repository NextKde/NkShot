// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import Kos.Ui as Kos

QQC.Popup {
    id: root
    objectName: "nkshotBeautifyPopup"
    property int styleIndex: 2
    property size sourceSize: Qt.size(0, 0)
    readonly property int outputWidth: sourceSize.width + paddingInput.value * 2
    readonly property int outputHeight: sourceSize.height + paddingInput.value * 2 + (watermark.text.trim().length ? 36 : 0)
    readonly property bool validSize: sourceSize.width > 0 && outputWidth <= 8192 && outputHeight <= 32000 && outputWidth * outputHeight <= 40000000
    signal createCopy(int padding, int style, string watermark, bool rounded)
    width: Math.min(400, parent.width - 32)
    height: Math.min(368, parent.height - 32)
    anchors.centerIn: parent
    modal: true
    focus: true
    padding: 20
    palette.text: Kos.AppTheme.paletteText
    palette.windowText: Kos.AppTheme.paletteText
    palette.buttonText: Kos.AppTheme.paletteText
    background: Kos.KosSurface { fillColor: Kos.AppTheme.contentBackground; radius: 18; elevation: 0.5 }
    contentItem: ColumnLayout {
        spacing: 12
        QQC.Label { text: i18nc("@title", "Style a sharing copy"); font.pixelSize: 17; font.bold: true; color: Kos.AppTheme.paletteText }
        QQC.ScrollView {
            id: body
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ColumnLayout {
                width: body.availableWidth
                spacing: 12
                QQC.Label {
                    Layout.fillWidth: true
                    text: i18nc("@info", "Add a background and optional watermark. Your original and edits stay unchanged.")
                    wrapMode: Text.Wrap
                    color: Kos.AppTheme.mutedText
                }
                RowLayout {
                    Layout.fillWidth: true
                    Repeater {
                        model: [i18nc("@option", "Light"), i18nc("@option", "Dark"), i18nc("@option", "Aurora")]
                        delegate: QQC.Button {
                            id: styleButton
                            required property int index
                            required property string modelData
                            objectName: "nkshotPresentationStyle" + index
                            Layout.fillWidth: true
                            implicitHeight: 34
                            text: modelData
                            checkable: true
                            autoExclusive: true
                            palette.buttonText: root.styleIndex === index ? Kos.AppTheme.accentText : Kos.AppTheme.paletteText
                            checked: root.styleIndex === index
                            onClicked: root.styleIndex = index
                            background: Kos.KosSurface {
                                fillColor: root.styleIndex === styleButton.index ? Kos.AppTheme.accent : Kos.AppTheme.controlBackground
                                radius: 9; elevation: 0; strokeWidth: 0
                                focused: styleButton.visualFocus; hovered: styleButton.hovered; pressed: styleButton.down
                            }
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    QQC.Label { text: i18nc("@label", "Padding (px)"); color: Kos.AppTheme.paletteText; Layout.fillWidth: true }
                    QQC.SpinBox {
                        id: paddingInput
                        objectName: "nkshotPresentationPadding"
                        from: 0; to: 240; value: 48; editable: true
                        Accessible.name: i18nc("@label", "Background padding in pixels")
                        background: Kos.KosSurface { fillColor: Kos.AppTheme.controlBackground; radius: 9; elevation: 0 }
                    }
                }
                QQC.Label {
                    Layout.fillWidth: true
                    text: root.validSize ? root.outputWidth + " × " + root.outputHeight + " px"
                        : i18nc("@info", "This copy is too large. Reduce padding or use a smaller image.")
                    wrapMode: Text.Wrap
                    color: Kos.AppTheme.mutedText
                }
                QQC.TextField {
                    id: watermark
                    objectName: "nkshotPresentationWatermark"
                    Layout.fillWidth: true
                    implicitHeight: 36
                    maximumLength: 120
                    placeholderText: i18nc("@info:placeholder", "Watermark text (optional)")
                    Accessible.name: i18nc("@label", "Optional watermark text")
                    color: Kos.AppTheme.paletteText
                    background: Kos.KosSurface { fillColor: Kos.AppTheme.controlBackground; radius: 9; elevation: 0 }
                }
                Kos.KosCheckBox {
                    id: rounded
                    text: i18nc("@option:check", "Round image corners")
                    checked: true
                }
            }
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            QQC.Button {
                id: cancel
                text: i18nc("@action", "Cancel")
                implicitHeight: 34; leftPadding: 14; rightPadding: 14
                onClicked: root.close()
                background: Kos.KosSurface { fillColor: Kos.AppTheme.controlBackground; radius: 10; elevation: 0; strokeWidth: 0; focused: cancel.visualFocus }
            }
            QQC.Button {
                id: create
                objectName: "nkshotBeautifyCreate"
                enabled: root.validSize
                opacity: enabled ? 1 : 0.4
                text: i18nc("@action", "Create copy")
                implicitHeight: 34; leftPadding: 14; rightPadding: 14
                palette.buttonText: Kos.AppTheme.accentText
                onClicked: { root.createCopy(paddingInput.value, root.styleIndex, watermark.text, rounded.checked); root.close() }
                background: Kos.KosSurface { fillColor: Kos.AppTheme.accent; radius: 10; elevation: 0; strokeWidth: 0; focused: create.visualFocus }
            }
        }
    }
}
