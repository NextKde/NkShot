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
        + footerActions.implicitHeight + topPadding + bottomPadding
        + 2 * captureLayout.spacing + captureSetup.chromeMinimumHeight)
    LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true
    palette.windowText: Kos.AppTheme.paletteText
    palette.text: Kos.AppTheme.paletteText
    palette.buttonText: Kos.AppTheme.paletteText
    palette.button: Kos.AppTheme.controlBackground
    padding: 20
    background: Rectangle { color: Kos.AppTheme.contentBackground }
    component StartupButton: Kos.LiquidActionButton {
        id: button
        materialForm: true
        implicitHeight: 30
        implicitWidth: captionMetrics.advanceWidth(text) + icon.width + spacing + leftPadding + rightPadding
        FontMetrics { id: captionMetrics; font: button.font }
        font.pixelSize: 13
        // Embedded page actions use the same neutral fill as the setup controls.
        surfaceColor: Kos.AppTheme.controlBackground
        leftPadding: 12
        rightPadding: 12
        spacing: 4
        icon.width: 16
        icon.height: 16
        icon.color: textColor
        contentItem: SymbolLabel { control: parent }
        reduceMotion: Settings.reduceUiMotion
    }
    component ConfigHelpButtonRow : Row {
        spacing: Kirigami.Units.mediumSpacing
        StartupButton {
            icon.name: "configure"
            text: i18n("Configure NkShot…")
            onClicked: contextWindow.showPreferencesDialog()
        }
        StartupButton {
            id: startupMore
            objectName: "nkshotStartupMore"
            icon.name: "view-more-symbolic"
            text: i18nc("@action:menu", "More")
            checkable: true
            checked: startupMenu.visible
            Accessible.role: Accessible.ButtonMenu
            onClicked: startupMenu.popup(startupMore, 0, startupMore.height)
            GlassMenu {
                id: startupMenu
                objectName: "nkshotStartupMenu"
                GlassMenuItem {
                    objectName: "nkshotOpenMediaAction"
                    text: i18nc("@action", "Open…")
                    icon.name: "document-open"
                    enabled: contextWindow.canOpenMedia
                    onTriggered: contextWindow.openMedia()
                }
                GlassMenuItem {
                    objectName: "nkshotRecentMediaAction"
                    text: i18nc("@action", "Recent files…")
                    icon.name: "document-open-recent"
                    enabled: contextWindow.canOpenMedia
                    onTriggered: recentPopup.open()
                }
                GlassMenuItem {
                    objectName: "nkshotClipboardImageAction"
                    text: i18nc("@action", "Edit clipboard image…")
                    icon.name: "edit-paste"
                    enabled: contextWindow.canOpenMedia
                    onTriggered: contextWindow.editClipboardImage()
                }
                GlassMenuItem {
                    objectName: "nkshotStartupColorAction"
                    text: i18nc("@action", "Pick a color…")
                    icon.name: "color-picker"
                    onTriggered: contextWindow.openColorPicker()
                }
                QQC.MenuSeparator {}
                GlassMenuItem {
                    text: i18n("Help")
                    icon.name: "help-contents"
                    onTriggered: Qt.callLater(function() { HelpMenu.popup(startupMore) })
                }
            }
        }
    }

    InlineMessageList {
        // Keep notifications out of the setup layout, including its minimum height.
        parent: root
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: root.topPadding
        anchors.leftMargin: root.leftPadding
        anchors.rightMargin: root.rightPadding
        maximumHeight: Math.min(96, root.availableHeight)
        z: 3
    }
    contentItem: ColumnLayout {
        id: captureLayout
        spacing: root.padding
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
            font.pixelSize: 20
            font.weight: Font.DemiBold
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
        // Page footer actions are centered; dialog footer actions align right.
        ConfigHelpButtonRow { id: footerActions; Layout.alignment: Qt.AlignHCenter }
    }
    Shortcut {
        sequences: [StandardKey.Preferences]
        onActivated: contextWindow.showPreferencesDialog()
    }
    RecentMediaPopup { id: recentPopup; parent: QQC.Overlay.overlay; sceneSource: root }

}
