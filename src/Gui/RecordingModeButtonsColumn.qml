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
    spacing: Kirigami.Units.mediumSpacing
    Repeater {
        model: RecordingModeModel
        delegate: ModeButton {
            id: button
            Layout.fillWidth: true
            icon.name: model.recordingMode === VideoPlatform.Region ? "transform-crop"
                : model.recordingMode === VideoPlatform.Window ? "draw-rectangle"
                : "view-fullscreen"
            text: model.display
            onClicked: SpectacleCore.startRecording(model.recordingMode, Settings.videoIncludePointer, Settings.recordingStartDelay, Settings.recordingAutoStopSeconds)
        }
    }
}
