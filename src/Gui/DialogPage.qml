/* SPDX-FileCopyrightText: 2022 Noah Davis <noahadvs@gmail.com>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import org.kde.kirigami as Kirigami
import Kos.Ui as Kos
import org.kde.spectacle.private

EmptyPage {
    id: root
    readonly property real minimumWidth: 360
    readonly property real minimumHeight: Math.max(360, captureTitle.implicitHeight
        + footerActions.implicitHeight + topPadding + bottomPadding + 32
        + captureSetup.chromeMinimumHeight + (header.visible ? header.height : 0))
    LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true
    palette.windowText: Kos.AppTheme.paletteText
    palette.text: Kos.AppTheme.paletteText
    palette.buttonText: Kos.AppTheme.paletteText
    palette.button: Kos.AppTheme.controlBackground
    padding: 20
    background: Rectangle { color: Kos.AppTheme.contentBackground }
    component ConfigHelpButtonRow : Row {
        spacing: Kirigami.Units.mediumSpacing
        QQC.Button {
            implicitHeight: Math.max(32, contentItem.implicitHeight + 12)
            leftPadding: 12
            rightPadding: 12
            icon.color: Kos.AppTheme.paletteText
            background: Rectangle {
                radius: 8
                color: Kos.AppTheme.controlBackground
                border.width: parent.activeFocus ? 2 : 0
                border.color: Kos.AppTheme.accent
            }
            y: dprRound((parent.height - height) / 2)
            icon.name: "configure"
            text: i18n("Configure NkShot…")
            onClicked: contextWindow.showPreferencesDialog()
        }
        QQC.ToolButton {
            implicitHeight: Math.max(32, contentItem.implicitHeight + 12)
            leftPadding: 12
            rightPadding: 12
            icon.color: Kos.AppTheme.paletteText
            background: Rectangle {
                radius: 8
                color: parent.down ? Kos.AppTheme.controlBackground : "transparent"
                border.width: parent.activeFocus ? 2 : 0
                border.color: Kos.AppTheme.accent
            }
            id: startupMore
            objectName: "nkshotStartupMore"
            y: dprRound((parent.height - height) / 2)
            flat: false
            icon.name: "view-more-symbolic"
            text: i18nc("@action:menu", "More")
            down: pressed || startupMenu.visible
            Accessible.role: Accessible.ButtonMenu
            onPressed: startupMenu.popup(startupMore, 0, startupMore.height)
            QQC.Menu {
                id: startupMenu
                objectName: "nkshotStartupMenu"
                width: 240
                padding: 6
                background: Kos.KosSurface {fillColor: Kos.AppTheme.contentBackground; radius: 14; elevation: 0.5}
                QQC.MenuItem {
                    objectName: "nkshotOpenMediaAction"
                    text: i18nc("@action", "Open…")
                    icon.name: "document-open"
                    enabled: contextWindow.canOpenMedia
                    onTriggered: contextWindow.openMedia()
                }
                QQC.MenuItem {
                    objectName: "nkshotRecentMediaAction"
                    text: i18nc("@action", "Recent files…")
                    icon.name: "document-open-recent"
                    enabled: contextWindow.canOpenMedia
                    onTriggered: recentPopup.open()
                }
                QQC.MenuItem {
                    objectName: "nkshotClipboardImageAction"
                    text: i18nc("@action", "Edit clipboard image…")
                    icon.name: "edit-paste"
                    enabled: contextWindow.canOpenMedia
                    onTriggered: contextWindow.editClipboardImage()
                }
                QQC.MenuItem {
                    objectName: "nkshotStartupColorAction"
                    text: i18nc("@action", "Pick a color…")
                    icon.name: "color-picker"
                    onTriggered: contextWindow.openColorPicker()
                }
                QQC.MenuSeparator {}
                QQC.MenuItem {
                    text: i18n("Help")
                    icon.name: "help-contents"
                    onTriggered: Qt.callLater(function() { HelpMenu.popup(startupMore) })
                }
            }
        }
    }


    header: InlineMessageList { width: root.width; maximumHeight: 96 }
    contentItem: ColumnLayout {
        spacing: 16
        QQC.Label {
            id: captureTitle
            Layout.fillWidth: true
            text: i18nc("@title", "New capture")
            DragHandler {
                target: null
                acceptedButtons: Qt.LeftButton
                onActiveChanged: if (active) {
                    acceptedButtons = contextWindow.startSystemMove() ? Qt.NoButton : Qt.LeftButton
                } else {
                    acceptedButtons = Qt.LeftButton
                }
            }
            font.pointSize: root.font.pointSize * 1.65
            font.bold: true
            color: Kos.AppTheme.text
        }
        CaptureSetupPane {
            id: captureSetup
            Layout.fillWidth: true
            Layout.fillHeight: true
            settings: Settings
            captureModes: CaptureModeModel
            recordingModes: RecordingModeModel
            recordingAvailable: SpectacleCore.videoPlatform.supportedRecordingModes !== VideoPlatform.NoRecordingModes
            transientSupported: (ImagePlatform.supportedGrabModes & ImagePlatform.TransientWithParent) !== 0
            clickSupported: ImagePlatform.supportedShutterModes === (ImagePlatform.Immediate | ImagePlatform.OnClick)
            onClickOnly: ImagePlatform.supportedShutterModes === ImagePlatform.OnClick
            audioSupported: SpectacleCore.videoPlatform.formatSupportsAudio(Settings.preferredVideoFormat)
            captureTimeRemaining: SpectacleCore.captureTimeRemaining
            captureProgress: SpectacleCore.captureProgress
            onCaptureRequested: SpectacleCore.takeNewScreenshot()
            onRecordRequested: recordingMode => SpectacleCore.startRecording(recordingMode, Settings.videoIncludePointer, Settings.recordingStartDelay, Settings.recordingAutoStopSeconds)
            onCancelRequested: SpectacleCore.cancelScreenshot()
        }
        ConfigHelpButtonRow { id: footerActions; Layout.alignment: Qt.AlignHCenter }
    }
    Shortcut {
        sequences: [StandardKey.Preferences]
        onActivated: contextWindow.showPreferencesDialog()
    }
    RecentMediaPopup { id: recentPopup; parent: QQC.Overlay.overlay }

}
