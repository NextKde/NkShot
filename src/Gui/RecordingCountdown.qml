// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import Kos.Ui as Kos

QQC.Pane {
    id: root
    font: Qt.application.font
    padding: 24
    palette.windowText: Kos.AppTheme.paletteText
    palette.buttonText: Kos.AppTheme.paletteText
    background: Rectangle { color: Kos.AppTheme.contentBackground }
    contentItem: ColumnLayout {
        spacing: 12
        QQC.Label {
            objectName: "nkshotRecordingCountdownStatus"
            Layout.fillWidth: true
            text: countdownWindow.phase === "preparing" ? i18n("Preparing recording")
                : countdownWindow.phase === "starting" ? i18n("Starting recording…") : i18n("Recording starts in")
            color: Kos.AppTheme.paletteText
            font.family: root.font.family
            font.pointSize: root.font.pointSize * 1.2
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
        QQC.Label {
            objectName: "nkshotRecordingCountdownValue"
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: countdownWindow.phase === "countdown" ? countdownWindow.remainingSeconds : "…"
            color: Kos.AppTheme.paletteText
            font.family: root.font.family
            font.pointSize: root.font.pointSize * 4
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        QQC.Label {
            Layout.fillWidth: true
            text: countdownWindow.description
            color: Kos.AppTheme.mutedText
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
            textFormat: Text.PlainText
        }
        QQC.Button {
            objectName: "nkshotRecordingCountdownCancel"
            Layout.fillWidth: true
            implicitHeight: Math.max(36, contentItem.implicitHeight + 14)
            text: i18n("Cancel")
            background: Rectangle {
                radius: 9
                color: Kos.AppTheme.controlBackground
                border.width: parent.activeFocus ? 2 : 0
                border.color: Kos.AppTheme.accent
            }
            contentItem: Text {
                text: parent.text; font: parent.font
                color: Kos.AppTheme.paletteText
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: countdownWindow.cancel()
        }
    }
}
