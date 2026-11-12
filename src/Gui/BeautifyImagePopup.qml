// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import Kos.Ui as Kos

Kos.LiquidDialog {
    id: root
    objectName: "nkshotBeautifyPopup"
    property int styleIndex: 2
    property var previewModel
    readonly property size sourceSize: previewModel ? previewModel.sourceSize : Qt.size(0, 0)
    readonly property int outputWidth: sourceSize.width + paddingInput.value * 2
    readonly property int outputHeight: sourceSize.height + paddingInput.value * 2 + (watermark.text.trim().length ? 36 : 0)
    readonly property bool validSize: sourceSize.width > 0 && outputWidth <= 8192 && outputHeight <= 32000 && outputWidth * outputHeight <= 40000000
    function updatePreview() {
        refresh.stop()
        if (previewModel) previewModel.update(paddingInput.value, styleIndex, watermark.text, rounded.checked)
    }
    function schedulePreview() { if (opened) refresh.restart() }
    onStyleIndexChanged: schedulePreview()
    onOpened: updatePreview()
    onClosed: { refresh.stop(); contextWindow.cancelPresentationPreview() }
    Timer { id: refresh; interval: 120; onTriggered: root.updatePreview() }
    readonly property bool wideLayout: width >= 650
    width: Math.min(780, parent.width - 32)
    height: Math.min(wideLayout ? 480 : 580, parent.height - 32)
    anchors.centerIn: parent
    sourceItem: parent
    focus: true
    header: null
    footer: null
    contentItem: ColumnLayout {
        spacing: 12
        QQC.Label { text: i18nc("@title", "Style a sharing copy"); font.pixelSize: 17; font.bold: true; color: Kos.AppTheme.paletteText }
        QQC.ScrollView {
            id: body
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            objectName: "nkshotPresentationScroll"
            contentWidth: availableWidth
            contentHeight: bodyContents.implicitHeight
            rightPadding: !mirrored && bodyBar.visible ? bodyBar.width + 4 : 0
            leftPadding: mirrored && bodyBar.visible ? bodyBar.width + 4 : 0
            QQC.ScrollBar.horizontal.policy: QQC.ScrollBar.AlwaysOff
            QQC.ScrollBar.vertical: QQC.ScrollBar {
                id: bodyBar
                objectName: "nkshotPresentationScrollbar"
                parent: body
                x: body.mirrored ? 0 : body.width - width
                y: body.topPadding
                height: body.availableHeight
                policy: bodyContents.implicitHeight > body.availableHeight ? QQC.ScrollBar.AlwaysOn : QQC.ScrollBar.AlwaysOff
            }
            GridLayout {
                id: bodyContents
                width: body.availableWidth
                columns: root.wideLayout ? 2 : 1
                columnSpacing: 20
                rowSpacing: 14
                Rectangle {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredWidth: root.wideLayout ? 400 : 320
                    Layout.preferredHeight: root.wideLayout ? 320 : 150
                    Layout.alignment: Qt.AlignTop
                    radius: 14
                    color: Kos.AppTheme.contentSecondaryBackground
                    clip: true
                    Image {
                        id: previewImage
                        objectName: "nkshotPresentationPreview"
                        anchors.fill: parent
                        anchors.margins: 12
                        source: root.previewModel ? root.previewModel.imageSource : ""
                        fillMode: Image.PreserveAspectFit
                        cache: false
                        asynchronous: false
                        visible: root.previewModel && root.previewModel.ready && !root.previewModel.error.length
                        opacity: refresh.running ? 0.65 : 1
                        Accessible.name: i18nc("@info", "Rendered sharing-copy preview")
                    }
                    QQC.Label {
                        anchors.centerIn: parent
                        width: parent.width - 32
                        visible: !previewImage.visible || previewImage.status !== Image.Ready
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        text: root.previewModel && root.previewModel.error.length
                            ? i18nc("@info", "Preview unavailable") : i18nc("@info", "Updating preview…")
                        color: Kos.AppTheme.mutedText
                    }
                }
                ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: root.wideLayout ? 280 : 320
                Layout.alignment: Qt.AlignTop
                spacing: 12
                QQC.Label {
                    Layout.fillWidth: true
                    text: i18nc("@info", "Preview a sharing copy. Your image and edits stay unchanged.")
                    wrapMode: Text.Wrap
                    color: Kos.AppTheme.mutedText
                }
                Kos.LiquidSegmentedControl {
                    objectName: "nkshotPresentationStyle"
                    Layout.fillWidth: true
                    implicitHeight: 28
                    opacity: 1
                    textColor: enabled ? Kos.AppTheme.paletteText : Kos.AppTheme.disabledText
                    mutedTextColor: enabled ? Kos.AppTheme.mutedText : Kos.AppTheme.disabledText
                    labels: [i18nc("@option", "Light"), i18nc("@option", "Dark"), i18nc("@option", "Aurora")]
                    currentIndex: root.styleIndex
                    onSelectionRequested: index => root.styleIndex = index
                }
                RowLayout {
                    Layout.fillWidth: true
                    QQC.Label { text: i18nc("@label", "Padding (px)"); color: Kos.AppTheme.paletteText; Layout.fillWidth: true }
                    Kos.LiquidTextField {
                        id: paddingInput
                        objectName: "nkshotPresentationPadding"
                        Layout.preferredWidth: 80
                        implicitHeight: 28
                        specificationForm: true
                        property int value: 48
                        text: String(value)
                        validator: IntValidator { bottom: 0; top: 240 }
                        inputMethodHints: Qt.ImhDigitsOnly
                        maximumLength: 3
                        onTextEdited: { if (acceptableInput) value = Number(text) }
                        onEditingFinished: text = String(value)
                        onActiveFocusChanged: { if (!activeFocus) text = String(value) }
                        onValueChanged: { text = String(value); root.schedulePreview() }
                        color: enabled ? Kos.AppTheme.paletteText : Kos.AppTheme.disabledText
                        opacity: 1
                        Accessible.name: i18nc("@label", "Background padding in pixels")
                    }
                }
                QQC.Label {
                    Layout.fillWidth: true
                    text: root.validSize ? root.outputWidth + " × " + root.outputHeight + " px"
                        : i18nc("@info", "This copy is too large. Reduce padding or use a smaller image.")
                    wrapMode: Text.Wrap
                    color: Kos.AppTheme.mutedText
                }
                Kos.LiquidTextField {
                    id: watermark
                    objectName: "nkshotPresentationWatermark"
                    Layout.fillWidth: true
                    implicitHeight: 28
                    specificationForm: true
                    maximumLength: 120
                    onTextChanged: root.schedulePreview()
                    placeholderText: i18nc("@info:placeholder", "Watermark text (optional)")
                    Accessible.name: i18nc("@label", "Optional watermark text")
                    color: enabled ? Kos.AppTheme.paletteText : Kos.AppTheme.disabledText
                    opacity: 1
                }
                Kos.LiquidCheckBox {
                    id: rounded
                    objectName: "nkshotPresentationRounded"
                    implicitHeight: 28
                    opacity: 1
                    // Override the upstream label to keep disabled text readable without fading the material.
                    contentItem: QQC.Label {
                        text: rounded.text
                        font: rounded.font
                        leftPadding: rounded.indicator.width + rounded.spacing
                        verticalAlignment: Text.AlignVCenter
                        color: rounded.enabled ? Kos.AppTheme.paletteText : Kos.AppTheme.disabledText
                    }
                    onCheckedChanged: root.schedulePreview()
                    text: i18nc("@option:check", "Round image corners")
                    checked: true
                }
                }
            }
        }
        QQC.Label {
            objectName: "nkshotPresentationStatus"
            Layout.fillWidth: true
            visible: root.previewModel && root.previewModel.error.length > 0
            text: root.previewModel ? root.previewModel.error : ""
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: Kos.AppTheme.paletteText
        }
        // Dialog actions are right-aligned throughout: Cancel precedes the primary action.
        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: 8
            Kos.LiquidActionButton {
                objectName: "nkshotBeautifyCancel"
                text: i18nc("@action", "Cancel")
                implicitHeight: root.actionHeight
                materialForm: true
                onClicked: root.close()
            }
            Kos.LiquidActionButton {
                objectName: "nkshotBeautifyCreate"
                enabled: paddingInput.acceptableInput && root.validSize && root.previewModel && root.previewModel.ready && !root.previewModel.error.length
                text: i18nc("@action", "Create copy")
                implicitHeight: root.actionHeight
                materialForm: true
                highlighted: true
                onClicked: {
                    root.updatePreview()
                    if (root.previewModel.ready && !root.previewModel.error.length && contextWindow.createPresentationCopy()) root.close()
                }
            }
        }
    }
}
