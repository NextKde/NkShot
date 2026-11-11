// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import Kos.Ui as Kos
import org.kde.kirigami as Kirigami

QQC.Popup {
    id: root
    objectName: "nkshotClipPopup"
    property var player
    property url sourceUrl
    readonly property var job: contextWindow.clipJob
    property bool detailsVisible: false
    property bool previewing: false
    property bool rangeInitialized: false
    property real previousPosition: 0
    property url sessionSource
    readonly property int startMs: startTime.value * 10
    readonly property int endMs: endTime.value * 10
    readonly property bool gif: job.outputFormat === "gif"
    readonly property bool validRange: job.ready && startMs >= 0 && endMs <= job.durationMs
        && endMs - startMs >= (gif ? 200 : 100) && (!gif || endMs - startMs <= 15000)
    readonly property bool locked: job.busy || contextWindow.choosingClipOutput
    function stopPreview() { previewing = false; player.pause() }
    width: Math.min(560, parent.width - 24)
    x: (parent.width - width) / 2
    y: Math.max(12, parent.height - height - 12)
    padding: 16
    modal: false
    focus: true
    font: Kirigami.Theme.defaultFont
    closePolicy: QQC.Popup.CloseOnEscape
    background: Kos.KosSurface {fillColor: Kos.AppTheme.contentBackground; radius: 16; elevation: 0}
    onOpened: {
        detailsVisible = false
        sessionSource = sourceUrl
        previousPosition = player.position
        player.pause()
        rangeInitialized = false
        startTime.value = 0
        endTime.value = 0
        job.includeAudio = true
        job.outputFormat = "mp4"
        contextWindow.prepareClipExport(sourceUrl)
    }
    onClosed: {
        previewing = false
        player.pause()
        contextWindow.cancelClipExport()
        if (player.source.toString() === sessionSource.toString()) player.setPosition(previousPosition)
    }
    onSourceUrlChanged: if (opened) close()
    Connections {
        target: contextWindow
        function onClipUiCancelled() { root.close() }
    }
    Connections {
        target: root.job
        function onChanged() {
            if (root.opened && root.job.ready && !root.rangeInitialized) {
                endTime.value = Math.floor(root.job.durationMs / 10)
                root.rangeInitialized = true
            }
        }
    }
    Connections {
        target: root.player
        function onPositionChanged() {
            if (root.previewing && root.player.position >= root.endMs) {
                root.previewing = false
                root.player.pause()
                root.player.setPosition(root.endMs)
            }
        }
    }
    component SolidButton: QQC.Button {
        id: button
        implicitHeight: Math.max(32, contentItem.implicitHeight + 12)
        leftPadding: 12
        rightPadding: 12
        font: root.font
        contentItem: QQC.Label {
            text: button.text
            font: button.font
            color: button.highlighted ? Kos.AppTheme.accentText : Kos.AppTheme.paletteText
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 9
            color: button.highlighted ? Kos.AppTheme.accent : Kos.AppTheme.controlBackground
            opacity: button.down ? 0.72 : button.hovered ? 0.9 : 1
            border.width: button.visualFocus ? 2 : 0
            border.color: Kos.AppTheme.accent
        }
        opacity: enabled ? 1 : 0.45
    }
    component TimeField: QQC.SpinBox {
        id: field
        editable: true
        from: 0
        to: Math.max(0, Math.floor(root.job.durationMs / 10))
        stepSize: 10
        implicitWidth: 82
        implicitHeight: Math.max(30, contentItem.implicitHeight + 10)
        leftPadding: 8
        rightPadding: 8
        font: root.font
        enabled: root.job.ready && !root.locked
        textFromValue: function(value, locale) { return Number(value / 100).toLocaleString(locale, 'f', 2) }
        valueFromText: function(text, locale) { return Math.round(Number.fromLocaleString(locale, text) * 100) }
        validator: DoubleValidator {bottom: 0; top: root.job.durationMs / 1000; decimals: 2}
        // Keep keyboard Up/Down fine adjustment without large form spinners.
        up.indicator: Item {implicitWidth: 0; implicitHeight: 0}
        down.indicator: Item {implicitWidth: 0; implicitHeight: 0}
        contentItem: TextInput {
            text: field.displayText
            font: field.font
            color: Kos.AppTheme.paletteText
            selectionColor: Kos.AppTheme.accent
            selectedTextColor: Kos.AppTheme.accentText
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            readOnly: !field.editable
            validator: field.validator
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            selectByMouse: true
            clip: true
        }
        background: Rectangle {
            color: Kos.AppTheme.controlBackground
            radius: 7
            border.width: field.activeFocus ? 1 : 0
            border.color: Kos.AppTheme.accent
        }
        onValueModified: root.stopPreview()
    }
    contentItem: ColumnLayout {
        spacing: 10
        RowLayout {
            Layout.fillWidth: true
            QQC.Label {
                Layout.fillWidth: true
                text: i18nc("@title", "Export a clip")
                font.family: root.font.family
                font.pointSize: root.font.pointSize * 1.12
                font.bold: true
                color: Kos.AppTheme.paletteText
            }
            QQC.ComboBox {
                id: formatChoice
                objectName: "nkshotClipFormat"
                model: ["MP4", "GIF"]
                currentIndex: root.gif ? 1 : 0
                enabled: !contextWindow.choosingClipOutput
                implicitHeight: Math.max(30, contentItem.implicitHeight + 10)
                implicitWidth: 78
                leftPadding: mirrored ? 24 : 10
                rightPadding: mirrored ? 10 : 24
                font: root.font
                palette.text: Kos.AppTheme.paletteText
                palette.buttonText: Kos.AppTheme.paletteText
                background: Rectangle {color: Kos.AppTheme.controlBackground; radius: 8}
                contentItem: QQC.Label {text: formatChoice.displayText; font: formatChoice.font; color: Kos.AppTheme.paletteText; verticalAlignment: Text.AlignVCenter}
                indicator: QQC.Label {text: "⌄"; color: Kos.AppTheme.mutedText; x: formatChoice.mirrored ? 8 : formatChoice.width - width - 8; anchors.verticalCenter: parent.verticalCenter}
                Accessible.name: i18nc("@label", "Clip output format")
                onActivated: {root.stopPreview(); root.job.outputFormat = currentIndex === 1 ? "gif" : "mp4"}
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            SolidButton {
                id: previewButton
                objectName: "nkshotClipPreview"
                implicitWidth: 34
                Layout.preferredWidth: 34
                text: ""
                enabled: root.validRange && !root.locked
                Accessible.name: root.previewing ? i18nc("@action", "Pause preview") : i18nc("@action", "Play selected range")
                QQC.ToolTip.visible: hovered
                QQC.ToolTip.text: Accessible.name
                contentItem: Text {
                    text: root.previewing ? "Ⅱ" : "▶"
                    color: Kos.AppTheme.paletteText
                    font: root.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    if (root.previewing) root.stopPreview()
                    else {root.player.pause(); root.player.setPosition(root.startMs); root.previewing = true; root.player.play()}
                }
            }
            QQC.RangeSlider {
                id: range
                objectName: "nkshotClipRange"
                Layout.fillWidth: true
                from: 0
                to: Math.max(1, root.job.durationMs / 10)
                stepSize: 1
                first.value: startTime.value
                second.value: endTime.value
                first.onMoved: {root.stopPreview(); startTime.value = Math.min(Math.round(first.value), endTime.value - 10)}
                second.onMoved: {root.stopPreview(); endTime.value = Math.max(Math.round(second.value), startTime.value + 10)}
                enabled: root.job.ready && !root.locked
                Accessible.name: i18nc("@label", "Clip time range")
                implicitHeight: 34
                leftPadding: 6
                rightPadding: 6
                background: Rectangle {
                    x: range.leftPadding
                    y: range.topPadding + (range.availableHeight - height) / 2
                    width: range.availableWidth
                    height: 10
                    radius: 5
                    color: Kos.AppTheme.controlBackground
                    Rectangle {
                        x: Math.min(range.first.visualPosition, range.second.visualPosition) * parent.width
                        width: Math.abs(range.second.visualPosition - range.first.visualPosition) * parent.width
                        height: parent.height
                        radius: 5
                        color: Kos.AppTheme.accent
                        opacity: 0.55
                    }
                }
                first.handle: Rectangle {
                    x: range.leftPadding + range.first.visualPosition * (range.availableWidth - width)
                    y: range.topPadding + (range.availableHeight - height) / 2
                    width: 10; height: 26; radius: 5
                    color: Kos.AppTheme.accent
                    border.color: Kos.AppTheme.paletteText
                    border.width: range.first.pressed || range.activeFocus ? 1 : 0
                }
                second.handle: Rectangle {
                    x: range.leftPadding + range.second.visualPosition * (range.availableWidth - width)
                    y: range.topPadding + (range.availableHeight - height) / 2
                    width: 10; height: 26; radius: 5
                    color: Kos.AppTheme.accent
                    border.color: Kos.AppTheme.paletteText
                    border.width: range.second.pressed || range.activeFocus ? 1 : 0
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            QQC.Label {text: i18nc("@label", "From"); font: root.font; color: Kos.AppTheme.mutedText}
            TimeField {id: startTime; objectName: "nkshotClipStart"; Layout.fillWidth: true; Accessible.name: i18nc("@label", "Clip start in seconds")}
            QQC.Label {text: i18nc("@label", "to"); font: root.font; color: Kos.AppTheme.mutedText}
            TimeField {id: endTime; objectName: "nkshotClipEnd"; Layout.fillWidth: true; Accessible.name: i18nc("@label", "Clip end in seconds")}
            QQC.Label {text: i18nc("@label:unit", "s"); font: root.font; color: Kos.AppTheme.mutedText}
        }
        RowLayout {
            Layout.fillWidth: true
            QQC.Label {
                Layout.fillWidth: true
                font: root.font
                color: Kos.AppTheme.mutedText
                text: root.validRange ? i18nc("@info", "%1 s · %2", ((root.endMs - root.startMs) / 1000).toFixed(2), root.gif ? i18nc("@info", "Silent · Looping") : root.job.hasAudio ? (root.job.includeAudio ? i18nc("@info", "Audio included") : i18nc("@info", "Audio removed")) : i18nc("@info", "No audio track"))
                    : root.gif ? i18nc("@info", "Select 0.20–15 seconds for GIF.") : i18nc("@info", "Select at least 0.10 seconds.")
                wrapMode: Text.Wrap
            }
            SolidButton {
                objectName: "nkshotClipDetails"
                text: i18nc("@action", "Details")
                Accessible.name: i18nc("@action", "Export format details")
                checkable: true
                checked: root.detailsVisible
                onClicked: root.detailsVisible = !root.detailsVisible
            }
        }
        QQC.CheckBox {
            id: includeAudio
            objectName: "nkshotClipIncludeAudio"
            Layout.fillWidth: true
            visible: root.detailsVisible && !root.gif && root.job.hasAudio
            enabled: !root.locked
            checked: root.job.includeAudio
            text: i18nc("@option:check", "Include audio in exported copy")
            font: root.font
            palette.windowText: Kos.AppTheme.paletteText
            onToggled: root.job.includeAudio = checked
            indicator: Rectangle {
                x: includeAudio.leftPadding
                y: (includeAudio.height - height) / 2
                width: 18; height: 18; radius: 5
                color: includeAudio.checked ? Kos.AppTheme.accent : Kos.AppTheme.controlBackground
                border.color: includeAudio.checked ? Kos.AppTheme.accent : Kos.AppTheme.border
                opacity: includeAudio.enabled ? 1 : 0.5
                QQC.Label {anchors.centerIn: parent; text: "✓"; visible: includeAudio.checked; color: Kos.AppTheme.accentText}
            }
        }
        QQC.Label {
            Layout.fillWidth: true
            visible: root.detailsVisible
            text: root.gif ? i18nc("@info", "New GIF: silent, looping, 10 fps, up to 640 px and 20 MiB. The original stays unchanged.")
                : i18nc("@info", "New MP4: H.264 video, with AAC audio only when included. WebM sources are re-encoded at 30 fps; this is not lossless. The original, preview speed and volume stay unchanged.")
            wrapMode: Text.Wrap
            font: root.font
            color: Kos.AppTheme.mutedText
        }
        QQC.ProgressBar {
            id: exportProgress
            objectName: "nkshotClipProgress"
            Layout.fillWidth: true
            visible: root.job.busy
            indeterminate: !root.job.ready
            value: root.job.progress
            palette.highlight: Kos.AppTheme.accent
        }
        QQC.Label {
            objectName: "nkshotClipStatus"
            Layout.fillWidth: true
            text: root.job.error.length ? root.job.error
                : root.job.outputUrl.toString().length ? i18nc("@info", "Clip saved: %1", decodeURIComponent(root.job.outputUrl.toString().split('/').pop()))
                : root.job.busy ? (root.job.ready ? i18nc("@info", "Exporting locally…") : i18nc("@info", "Checking local video…")) : ""
            visible: text.length > 0
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            font: root.font
            color: Kos.AppTheme.paletteText
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            SolidButton {
                objectName: "nkshotClipCancel"
                text: root.job.outputUrl.toString().length ? i18nc("@action", "Done") : i18nc("@action", "Cancel")
                onClicked: root.close()
            }
            Item {Layout.fillWidth: true}
            SolidButton {
                objectName: "nkshotClipExport"
                text: i18nc("@action", "Export…")
                highlighted: true
                enabled: root.validRange && !root.locked
                onClicked: {root.stopPreview(); contextWindow.chooseClipOutput(root.startMs, root.endMs)}
            }
        }
    }
}
