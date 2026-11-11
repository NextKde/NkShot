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
    width: Math.min(420, parent.width - 24)
    x: (parent.width - width) / 2
    y: Math.max(12, parent.height - height - 12)
    padding: 12
    modal: false
    focus: true
    font: Kirigami.Theme.defaultFont
    closePolicy: QQC.Popup.CloseOnEscape
    background: Kos.KosSurface {fillColor: Kos.AppTheme.contentBackground; radius: 16; elevation: 0}
    onOpened: {
        sessionSource = sourceUrl
        previousPosition = player.position
        player.pause()
        rangeInitialized = false
        startTime.value = 0
        endTime.value = 0
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
    component TimeField: QQC.SpinBox {
        editable: true
        from: 0
        to: Math.max(0, Math.floor(root.job.durationMs / 10))
        stepSize: 10
        implicitHeight: 32
        enabled: root.job.ready && !root.locked
        textFromValue: function(value, locale) { return Number(value / 100).toLocaleString(locale, 'f', 2) }
        valueFromText: function(text, locale) { return Math.round(Number.fromLocaleString(locale, text) * 100) }
        validator: DoubleValidator {bottom: 0; top: root.job.durationMs / 1000; decimals: 2}
        palette.text: Kos.AppTheme.paletteText
        palette.buttonText: Kos.AppTheme.paletteText
        background: Kos.KosSurface {fillColor: Kos.AppTheme.controlBackground; radius: 8; elevation: 0}
        onValueModified: {
            root.previewing = false
            root.player.pause()
        }
    }
    contentItem: ColumnLayout {
        spacing: 8
        RowLayout {
            Layout.fillWidth: true
            QQC.Label {
                Layout.fillWidth: true
                text: i18nc("@title", "Export a clip")
                font.family: root.font.family
                font.pixelSize: 15
                font.bold: true
                color: Kos.AppTheme.paletteText
            }
            QQC.ComboBox {
                id: formatChoice
                objectName: "nkshotClipFormat"
                model: ["MP4", "GIF"]
                currentIndex: root.gif ? 1 : 0
                enabled: !contextWindow.choosingClipOutput
                implicitHeight: 28
                implicitWidth: 82
                leftPadding: 10
                rightPadding: 26
                palette.text: Kos.AppTheme.paletteText
                palette.buttonText: Kos.AppTheme.paletteText
                background: Kos.KosSurface {fillColor: Kos.AppTheme.controlBackground; radius: 8; elevation: 0}
                contentItem: QQC.Label {text: formatChoice.displayText; color: Kos.AppTheme.paletteText; verticalAlignment: Text.AlignVCenter}
                indicator: QQC.Label {text: "⌄"; color: Kos.AppTheme.mutedText; x: formatChoice.width - width - 9; anchors.verticalCenter: parent.verticalCenter}
                Accessible.name: i18nc("@label", "Clip output format")
                onActivated: {root.stopPreview(); root.job.outputFormat = currentIndex === 1 ? "gif" : "mp4"}
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
            palette.highlight: Kos.AppTheme.accent
            Accessible.name: i18nc("@label", "Clip time range")
            implicitHeight: 24
            background: Rectangle {
                x: range.leftPadding
                y: range.topPadding + (range.availableHeight - height) / 2
                width: range.availableWidth
                height: 4
                radius: 2
                color: Kos.AppTheme.border
                Rectangle {
                    x: Math.min(range.first.visualPosition, range.second.visualPosition) * parent.width
                    width: Math.abs(range.second.visualPosition - range.first.visualPosition) * parent.width
                    height: parent.height
                    radius: 2
                    color: Kos.AppTheme.accent
                }
            }
            first.handle: Rectangle {
                x: range.leftPadding + range.first.visualPosition * (range.availableWidth - width)
                y: range.topPadding + (range.availableHeight - height) / 2
                width: 14; height: 14; radius: 7
                color: Kos.AppTheme.contentBackground
                border.color: Kos.AppTheme.accent
                border.width: 2
            }
            second.handle: Rectangle {
                x: range.leftPadding + range.second.visualPosition * (range.availableWidth - width)
                y: range.topPadding + (range.availableHeight - height) / 2
                width: 14; height: 14; radius: 7
                color: Kos.AppTheme.contentBackground
                border.color: Kos.AppTheme.accent
                border.width: 2
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                QQC.Label {text: i18nc("@label", "Start (seconds)"); color: Kos.AppTheme.mutedText}
                TimeField {id: startTime; objectName: "nkshotClipStart"; Layout.fillWidth: true; Accessible.name: i18nc("@label", "Clip start in seconds")}
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                QQC.Label {text: i18nc("@label", "End (seconds)"); color: Kos.AppTheme.mutedText}
                TimeField {id: endTime; objectName: "nkshotClipEnd"; Layout.fillWidth: true; Accessible.name: i18nc("@label", "Clip end in seconds")}
            }
        }
        QQC.Label {
            Layout.fillWidth: true
            color: Kos.AppTheme.mutedText
            text: root.validRange ? i18nc("@info", "%1 s · %2", ((root.endMs - root.startMs) / 1000).toFixed(2), root.gif ? i18nc("@info", "Silent · Looping") : root.job.hasAudio ? i18nc("@info", "Audio included") : i18nc("@info", "No audio track"))
                : root.gif ? i18nc("@info", "GIF range: 0.20–15 seconds. Shorten or extend the selection.") : i18nc("@info", "Choose a range of at least 0.10 seconds.")
            wrapMode: Text.Wrap
        }
        QQC.Label {
            Layout.fillWidth: true
            text: root.gif ? i18nc("@info", "New GIF · Silent, looping · 10 fps · Up to 640 px / 20 MiB. Original unchanged.")
                : i18nc("@info", "New MP4 only · H.264/AAC. Original and playback settings stay unchanged.")
            wrapMode: Text.Wrap
            color: Kos.AppTheme.mutedText
            font.pixelSize: 11
        }
        QQC.ProgressBar {
            objectName: "nkshotClipProgress"
            Layout.fillWidth: true
            visible: root.job.busy
            indeterminate: !root.job.ready
            value: root.job.progress
        }
        QQC.Label {
            objectName: "nkshotClipStatus"
            Layout.fillWidth: true
            text: root.job.error.length ? root.job.error
                : root.job.outputUrl.toString().length ? i18nc("@info", "Clip saved: %1", decodeURIComponent(root.job.outputUrl.toString().split('/').pop()))
                : root.job.busy ? (root.job.ready ? i18nc("@info", "Exporting locally…") : i18nc("@info", "Checking local recording…")) : ""
            visible: text.length > 0
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: Kos.AppTheme.paletteText
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Kos.KosButton {
                objectName: "nkshotClipPreview"
                implicitHeight: 32
                text: root.previewing ? i18nc("@action", "Pause preview") : i18nc("@action", "Preview")
                enabled: root.validRange && !root.locked
                onClicked: {
                    if (root.previewing) {root.previewing = false; root.player.pause()}
                    else {root.player.pause(); root.player.setPosition(root.startMs); root.previewing = true; root.player.play()}
                }
            }
            Item {Layout.fillWidth: true}
            Kos.KosButton {
                objectName: "nkshotClipCancel"
                implicitHeight: 32
                text: root.job.outputUrl.toString().length ? i18nc("@action", "Done") : i18nc("@action", "Cancel")
                onClicked: root.close()
            }
            Kos.KosButton {
                objectName: "nkshotClipExport"
                implicitHeight: 32
                flatPrimary: true
                text: i18nc("@action", "Export…")
                highlighted: true
                enabled: root.validRange && !root.locked
                onClicked: {root.previewing = false; root.player.pause(); contextWindow.chooseClipOutput(root.startMs, root.endMs)}
            }
        }
    }
}
