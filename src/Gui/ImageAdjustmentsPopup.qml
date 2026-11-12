// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import Kos.Ui as Kos

Kos.LiquidDialog {
    id: root
    objectName: "nkshotImageAdjustmentsPopup"
    property var previewModel
    property bool submitted: false
    signal cancelled()
    readonly property bool wideLayout: width >= 650
    function updatePreview() {
        refresh.stop()
        if (previewModel) previewModel.update(Math.round(brightness.value), Math.round(contrast.value), Math.round(saturation.value))
    }
    function schedulePreview() { if (opened) refresh.restart() }
    function resetValues() { brightness.value = 0; contrast.value = 0; saturation.value = 0; updatePreview() }
    onOpened: { submitted = false; resetValues() }
    onClosed: { refresh.stop(); if (!submitted) cancelled() }
    Timer { id: refresh; interval: 100; onTriggered: root.updatePreview() }
    width: Math.min(780, parent.width - 32)
    height: Math.min(wideLayout ? 440 : 540, parent.height - 32)
    anchors.centerIn: parent
    sourceItem: parent
    focus: true
    header: null
    footer: null
    component Adjustment: ColumnLayout {
        id: control
        required property string title
        required property string controlName
        property int value: 0
        Layout.fillWidth: true
        spacing: 4
        RowLayout {
            Layout.fillWidth: true
            QQC.Label { Layout.fillWidth: true; text: control.title; color: Kos.AppTheme.paletteText }
            QQC.Label { text: (control.value > 0 ? "+" : "") + control.value; color: Kos.AppTheme.mutedText }
        }
        onValueChanged: root.schedulePreview()
        PopupSlider {
            objectName: control.controlName
            Layout.fillWidth: true
            value: (control.value + 100) / 200
            wheelStep: 1 / 200
            Accessible.name: control.title
            onPreviewChanged: value => control.value = Math.round(value * 200 - 100)
        }
    }
    contentItem: ColumnLayout {
        spacing: 12
        QQC.Label { text: i18nc("@title", "Adjust a sharing copy"); font.pixelSize: 17; font.bold: true; color: Kos.AppTheme.paletteText }
        QQC.ScrollView {
            id: scroll
            objectName: "nkshotImageAdjustmentsScroll"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            contentHeight: body.implicitHeight
            rightPadding: mirrored ? 0 : 14
            leftPadding: mirrored ? 14 : 0
            QQC.ScrollBar.horizontal.policy: QQC.ScrollBar.AlwaysOff
            QQC.ScrollBar.vertical: QQC.ScrollBar {
                id: bar
                objectName: "nkshotImageAdjustmentsScrollbar"
                parent: scroll
                x: scroll.mirrored ? 0 : scroll.width - width
                y: scroll.topPadding
                height: scroll.availableHeight
                implicitWidth: 10
                leftPadding: 2
                background: Item {}
                policy: body.implicitHeight > scroll.availableHeight ? QQC.ScrollBar.AlwaysOn : QQC.ScrollBar.AlwaysOff
            }
            GridLayout {
                id: body
                width: scroll.availableWidth
                columns: root.wideLayout ? 2 : 1
                columnSpacing: 20; rowSpacing: 14
                Rectangle {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredWidth: root.wideLayout ? 400 : 320
                    Layout.preferredHeight: root.wideLayout ? 310 : 150
                    Layout.alignment: Qt.AlignTop
                    radius: 14
                    color: Kos.AppTheme.controlBackground
                    clip: true
                    Image {
                        id: preview
                        objectName: "nkshotImageAdjustmentsImage"
                        anchors.fill: parent
                        anchors.margins: 12
                        source: root.previewModel ? root.previewModel.imageSource : ""
                        cache: false
                        asynchronous: false
                        fillMode: Image.PreserveAspectFit
                        visible: root.previewModel && root.previewModel.ready
                        opacity: refresh.running ? 0.65 : 1
                        Accessible.name: i18nc("@info", "Adjusted copy preview")
                    }
                    QQC.Label {
                        anchors.centerIn: parent
                        width: parent.width - 24
                        visible: !preview.visible || preview.status !== Image.Ready
                        text: root.previewModel && root.previewModel.error.length ? i18nc("@info", "Preview unavailable") : i18nc("@info", "Preparing preview…")
                        wrapMode: Text.Wrap
                        horizontalAlignment: Text.AlignHCenter
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
                        text: i18nc("@info", "Adjust the current image and its annotations together in a separate, flattened copy. Your edit stays unchanged.")
                        wrapMode: Text.Wrap
                        color: Kos.AppTheme.mutedText
                    }
                    Adjustment { id: brightness; title: i18nc("@label", "Brightness"); controlName: "nkshotImageAdjustmentBrightness" }
                    Adjustment { id: contrast; title: i18nc("@label", "Contrast"); controlName: "nkshotImageAdjustmentContrast" }
                    Adjustment { id: saturation; title: i18nc("@label", "Saturation"); controlName: "nkshotImageAdjustmentSaturation" }
                    Kos.LiquidActionButton {
                        implicitHeight: 28
                        materialForm: true
                        objectName: "nkshotImageAdjustmentsReset"
                        text: i18nc("@action", "Reset")
                        enabled: brightness.value !== 0 || contrast.value !== 0 || saturation.value !== 0
                        onClicked: root.resetValues()
                    }
                }
            }
        }
        QQC.Label {
            objectName: "nkshotImageAdjustmentsError"
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.previewModel ? root.previewModel.error : ""
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: Kos.AppTheme.paletteText
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: 8
            Kos.LiquidActionButton {
                objectName: "nkshotImageAdjustmentsCancel"
                text: i18nc("@action", "Cancel")
                implicitHeight: root.actionHeight
                materialForm: true
                onClicked: root.close()
            }
            Kos.LiquidActionButton {
                objectName: "nkshotImageAdjustmentsCreate"
                text: i18nc("@action", "Create copy")
                enabled: root.previewModel && root.previewModel.ready
                implicitHeight: root.actionHeight
                materialForm: true
                highlighted: true
                onClicked: {
                    root.updatePreview()
                    if (root.previewModel && root.previewModel.ready) { root.submitted = true; root.accepted(); root.close() }
                }
            }
        }
    }
}
