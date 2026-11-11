// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import Kos.Ui as Kos

QQC.Popup {
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
                    radius: 12
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
                        onValueChanged: root.schedulePreview()
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
                    onTextChanged: root.schedulePreview()
                    placeholderText: i18nc("@info:placeholder", "Watermark text (optional)")
                    Accessible.name: i18nc("@label", "Optional watermark text")
                    color: Kos.AppTheme.paletteText
                    background: Kos.KosSurface { fillColor: Kos.AppTheme.controlBackground; radius: 9; elevation: 0 }
                }
                Kos.KosCheckBox {
                    id: rounded
                    objectName: "nkshotPresentationRounded"
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
        RowLayout {
            Layout.fillWidth: true
            QQC.Button {
                id: cancel
                objectName: "nkshotBeautifyCancel"
                text: i18nc("@action", "Cancel")
                implicitHeight: 34; leftPadding: 14; rightPadding: 14
                onClicked: root.close()
                background: Kos.KosSurface { fillColor: Kos.AppTheme.controlBackground; radius: 10; elevation: 0; strokeWidth: 0; focused: cancel.visualFocus }
            }
            Item { Layout.fillWidth: true }
            QQC.Button {
                id: create
                objectName: "nkshotBeautifyCreate"
                enabled: root.validSize && root.previewModel && root.previewModel.ready && !root.previewModel.error.length
                opacity: enabled ? 1 : 0.4
                text: i18nc("@action", "Create copy")
                implicitHeight: 34; leftPadding: 14; rightPadding: 14
                palette.buttonText: Kos.AppTheme.accentText
                onClicked: {
                    root.updatePreview()
                    if (root.previewModel.ready && !root.previewModel.error.length && contextWindow.createPresentationCopy()) root.close()
                }
                background: Kos.KosSurface { fillColor: Kos.AppTheme.accent; radius: 10; elevation: 0; strokeWidth: 0; focused: create.visualFocus }
            }
        }
    }
}
