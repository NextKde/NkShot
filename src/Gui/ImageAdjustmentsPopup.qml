// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import Kos.Ui as Kos

QQC.Popup {
    id: root
    objectName: "nkshotImageAdjustmentsPopup"
    property var previewModel
    property bool submitted: false
    signal accepted()
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
    modal: true
    focus: true
    padding: 20
    palette.text: Kos.AppTheme.paletteText
    palette.windowText: Kos.AppTheme.paletteText
    palette.buttonText: Kos.AppTheme.paletteText
    background: Kos.KosSurface { fillColor: Kos.AppTheme.contentBackground; radius: 18; elevation: 0.5 }
    component Adjustment: ColumnLayout {
        id: control
        required property string title
        required property string controlName
        property alias value: slider.value
        Layout.fillWidth: true
        spacing: 4
        RowLayout {
            Layout.fillWidth: true
            QQC.Label { Layout.fillWidth: true; text: control.title; color: Kos.AppTheme.paletteText }
            QQC.Label { text: (slider.value > 0 ? "+" : "") + Math.round(slider.value); color: Kos.AppTheme.mutedText }
        }
        QQC.Slider {
            id: slider
            objectName: control.controlName
            Layout.fillWidth: true
            from: -100; to: 100; stepSize: 1; value: 0
            snapMode: QQC.Slider.SnapAlways
            Accessible.name: control.title
            onValueChanged: root.schedulePreview()
            palette.highlight: Kos.AppTheme.accent
            implicitHeight: 28
            background: Rectangle {
                x: slider.leftPadding
                y: slider.topPadding + slider.availableHeight / 2 - height / 2
                width: slider.availableWidth
                height: 4
                radius: 2
                color: Kos.AppTheme.controlBackground
                Rectangle { width: slider.visualPosition * parent.width; height: parent.height; radius: 2; color: Kos.AppTheme.accent }
            }
            handle: Rectangle {
                x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
                y: slider.topPadding + slider.availableHeight / 2 - height / 2
                width: 18; height: 18; radius: 9
                color: slider.pressed ? Kos.AppTheme.accent : Kos.AppTheme.contentBackground
                border.width: slider.visualFocus ? 2 : 1
                border.color: slider.visualFocus || slider.pressed ? Kos.AppTheme.accent : Kos.AppTheme.mutedText
            }
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
            rightPadding: !mirrored && bar.visible ? bar.width + 4 : 0
            leftPadding: mirrored && bar.visible ? bar.width + 4 : 0
            QQC.ScrollBar.horizontal.policy: QQC.ScrollBar.AlwaysOff
            QQC.ScrollBar.vertical: QQC.ScrollBar {
                id: bar
                objectName: "nkshotImageAdjustmentsScrollbar"
                parent: scroll
                x: scroll.mirrored ? 0 : scroll.width - width
                y: scroll.topPadding
                height: scroll.availableHeight
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
                    radius: 12
                    color: Kos.AppTheme.contentSecondaryBackground
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
                    QQC.Button {
                        id: reset
                        objectName: "nkshotImageAdjustmentsReset"
                        text: i18nc("@action", "Reset")
                        enabled: brightness.value !== 0 || contrast.value !== 0 || saturation.value !== 0
                        onClicked: root.resetValues()
                        background: Kos.KosSurface { fillColor: Kos.AppTheme.controlBackground; radius: 9; elevation: 0; focused: reset.visualFocus }
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
            Layout.fillWidth: true
            QQC.Button {
                id: cancel
                objectName: "nkshotImageAdjustmentsCancel"
                text: i18nc("@action", "Cancel")
                implicitHeight: 34; leftPadding: 14; rightPadding: 14
                onClicked: root.close()
                background: Kos.KosSurface { fillColor: Kos.AppTheme.controlBackground; radius: 10; elevation: 0; focused: cancel.visualFocus }
            }
            Item { Layout.fillWidth: true }
            QQC.Button {
                id: create
                objectName: "nkshotImageAdjustmentsCreate"
                text: i18nc("@action", "Create copy")
                enabled: root.previewModel && root.previewModel.ready
                implicitHeight: 34; leftPadding: 14; rightPadding: 14
                palette.buttonText: Kos.AppTheme.accentText
                onClicked: {
                    root.updatePreview()
                    if (root.previewModel && root.previewModel.ready) { root.submitted = true; root.accepted(); root.close() }
                }
                background: Kos.KosSurface { fillColor: Kos.AppTheme.accent; radius: 10; elevation: 0; focused: create.visualFocus }
            }
        }
    }
}
