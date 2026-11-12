/* SPDX-FileCopyrightText: 2023 Aleix Pol Gonzalez <aleixpol@kde.org>
 * SPDX-FileCopyrightText: 2022 Noah Davis <noahadvs@gmail.com>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import org.kde.kirigami as Kirigami
import org.kde.spectacle.private

ColumnLayout {
    id: root
    spacing: Kirigami.Units.mediumSpacing

    // GIF and WebP can't carry an audio track, so the audio options are
    // disabled for those formats.
    readonly property bool audioSupported: SpectacleCore.videoPlatform.formatSupportsAudio(Settings.preferredVideoFormat)

    QQC.CheckBox {
        Layout.fillWidth: true
        text: i18nc("@option:check", "Include mouse pointer")
        QQC.ToolTip.text: i18nc("@info:tooltip", "Show the mouse cursor in the screen recording.")
        QQC.ToolTip.delay: Kirigami.Units.toolTipDelay
        QQC.ToolTip.visible: hovered
        checked: Settings.videoIncludePointer
        onToggled: Settings.videoIncludePointer = checked
    }
    QQC.CheckBox {
        Layout.fillWidth: true
        enabled: root.audioSupported
        text: i18nc("@option:check", "Record system audio")
        QQC.ToolTip.text: i18nc("@info:tooltip", "Include the audio that is currently playing on the system in the recording.")
        QQC.ToolTip.delay: Kirigami.Units.toolTipDelay
        QQC.ToolTip.visible: hovered
        // Explain why this is disabled, since a disabled control doesn't get
        // hover events and can't show its tooltip.
        Accessible.description: root.audioSupported ? "" : audioUnsupportedMessage.text
        checked: Settings.videoRecordSystemAudio
        onToggled: Settings.videoRecordSystemAudio = checked
    }
    QQC.CheckBox {
        Layout.fillWidth: true
        enabled: root.audioSupported
        text: i18nc("@option:check", "Record microphone")
        QQC.ToolTip.text: i18nc("@info:tooltip", "Include audio from the default microphone in the recording.")
        QQC.ToolTip.delay: Kirigami.Units.toolTipDelay
        QQC.ToolTip.visible: hovered
        Accessible.description: root.audioSupported ? "" : audioUnsupportedMessage.text
        checked: Settings.videoRecordMicrophone
        onToggled: Settings.videoRecordMicrophone = checked
    }
    QQC.ToolButton {
        id: advanced
        objectName: "nkshotLegacyRecordingAdvanced"
        Layout.fillWidth: true
        checkable: true
        readonly property int validLimit: [60, 300, 600].indexOf(Settings.recordingAutoStopSeconds) >= 0
            ? Settings.recordingAutoStopSeconds : 0
        text: (checked ? i18n("Fewer options") : i18n("More options"))
            + (!checked && validLimit > 0 ? i18n(" · Auto-stop %1 min", validLimit / 60) : "")
        icon.name: checked ? "arrow-up" : "arrow-down"
    }
    ColumnLayout {
        Layout.fillWidth: true
        visible: advanced.checked
        RowLayout {
            Layout.fillWidth: true
            QQC.Label {text: i18n("Auto-stop"); Layout.fillWidth: true}
            QQC.ComboBox {
                objectName: "nkshotLegacyRecordingAutoStop"
                model: [i18n("Off"), i18n("1 minute"), i18n("5 minutes"), i18n("10 minutes")]
                currentIndex: Math.max(0, [0, 60, 300, 600].indexOf(Settings.recordingAutoStopSeconds))
                onActivated: Settings.recordingAutoStopSeconds = [0, 60, 300, 600][currentIndex]
                Accessible.name: i18n("Stop recording automatically after")
            }
        }
        QQC.Label {
            Layout.fillWidth: true
            text: i18n("Counts recorded time only. Pauses do not count.")
            wrapMode: Text.WordWrap
        }
    }
    Kirigami.InlineMessage {
        id: audioUnsupportedMessage
        Layout.fillWidth: true
        type: Kirigami.MessageType.Information
        text: i18nc("@info", "The selected video format does not support audio.")
        visible: !root.audioSupported
    }
}
