/*
 * SPDX-FileCopyrightText: 2023 Aleix Pol i Gonzalez <aleixpol@kde.org>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

import QtQuick
import QtQuick.Window
import Kos.Ui as Kos
import QtQuick.Layouts
import QtQuick.Controls as QQC
import QtMultimedia
import org.kde.kirigami as Kirigami
import org.kde.spectacle.private

FocusScope {
    id: root
    objectName: "nkshotRecordingView"
    // A reusable local playback view; capture remains owned by SpectacleCore.
    property url sourceUrl: SpectacleCore.currentVideo
    readonly property bool usesAnimatedImage: {
        const format = SpectacleCore.videoPlatform.formatForPath(sourceUrl.toString())
        return format === VideoPlatform.WebP || format === VideoPlatform.Gif
    }
    readonly property bool playbackFailed: (animatedImage.source.toString().length > 0 && animatedImage.status === Image.Error)
        || (!usesAnimatedImage && mediaPlayer.error !== MediaPlayer.NoError)
    function reloadMedia() {
        mediaPlayer.source = ""
        animatedImage.source = ""
        Qt.callLater(function() {
            animatedImage.source = Qt.binding(function() {
                return root.usesAnimatedImage ? root.sourceUrl : ""
            })
            mediaPlayer.source = Qt.binding(function() { return root.usesAnimatedImage ? "" : root.sourceUrl })
        })
    }
    readonly property bool hasAnimatedImage: animatedImage.status === Image.Ready
    readonly property bool hasContent: hasAnimatedImage || mediaPlayer.hasVideo
    readonly property bool playing: hasAnimatedImage ? animatedImage.playing : mediaPlayer.playing

    function togglePlay() {
        if (clipPopup.opened) return
        if (root.playing) {
            if (root.hasAnimatedImage) {
                animatedImage.playing = false
                return
            }
            mediaPlayer.pause()
        } else {
            if (root.hasAnimatedImage) {
                animatedImage.playing = true
                return
            }
            mediaPlayer.play()
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: enabled ?
            (pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor)
            : undefined
        enabled: root.hasContent && !clipPopup.opened && !root.captureActive
        onPositionChanged: {
            contextWindow.startDrag()
        }
        onClicked: root.togglePlay()
    }

    // When QtMultimedia uses the FFmpeg backend, it can't play certain animated
    // image formats such as WebP.
    AnimatedImage {
        id: animatedImage
        objectName: "nkshotAnimatedImage"
        anchors.fill: parent
        source: root.usesAnimatedImage ? root.sourceUrl : ""
        visible: status === Image.Ready && !root.captureActive
        fillMode: Image.PreserveAspectFit
    }

    VideoOutput {
        id: videoOutput
        visible: mediaPlayer.hasVideo && !root.captureActive && GraphicsInfo.api !== GraphicsInfo.Software
        anchors.fill: parent
        anchors.bottomMargin: clipPopup.opened ? clipPopup.height + 20 : 0
        fillMode: VideoOutput.PreserveAspectFit
    }

    SoftwareVideoPreview {
        objectName: "nkshotSoftwareVideoPreview"
        anchors.fill: parent
        anchors.bottomMargin: clipPopup.opened ? clipPopup.height + 20 : 0
        sink: videoOutput.videoSink
        active: GraphicsInfo.api === GraphicsInfo.Software && mediaPlayer.hasVideo && !root.captureActive
        visible: active
    }

    ImageOverlayPreview {
        objectName: "nkshotVideoAnnotationOverlay"
        anchors.fill: videoOutput
        visible: clipPopup.opened && contextWindow.clipJob.hasAnnotationOverlay && !root.captureActive
        image: contextWindow.clipJob.annotationOverlay
        displayRect: cropOverlay.displayRect
    }

    CropPreviewOverlay {
        id: cropOverlay
        objectName: "nkshotVideoCropOverlay"
        anchors.fill: videoOutput
        visible: clipPopup.opened && contextWindow.clipJob.ready && contextWindow.clipJob.cropAspect !== "original" && !root.captureActive
        interactive: visible && !clipPopup.locked && !clipPopup.editingPrivacy
        freeResize: contextWindow.clipJob.cropAspect === "free"
        onRectRequested: (x, y, width, height) => {
            clipPopup.stopPreview(); contextWindow.clipJob.setCropRect(x, y, width, height)
        }
        onPositionRequested: (x, y) => {clipPopup.stopPreview(); contextWindow.clipJob.setCropPosition(x, y)}
        onCenterRequested: {clipPopup.stopPreview(); contextWindow.clipJob.resetCropPosition()}
        sourceSize: contextWindow.clipJob.sourceSize
        cropRect: contextWindow.clipJob.cropRect
        readonly property real fittedScale: sourceSize.width > 0 && sourceSize.height > 0
            ? Math.max(0, Math.min(width / sourceSize.width, height / sourceSize.height)) : 0
        displayRect: GraphicsInfo.api === GraphicsInfo.Software
            ? Qt.rect((width-sourceSize.width*fittedScale)/2, (height-sourceSize.height*fittedScale)/2,
                      sourceSize.width*fittedScale, sourceSize.height*fittedScale)
            : videoOutput.contentRect
    }

    Item {
        anchors.fill: videoOutput
        visible: clipPopup.opened && contextWindow.clipJob.ready && contextWindow.clipJob.privacyEnabled && !root.captureActive
        Repeater {
            model: contextWindow.clipJob.privacyRegions
            delegate: Rectangle {
                required property int index
                required property var modelData
                objectName: "nkshotVideoPrivacyRegion_" + index
                readonly property size sourceSize: contextWindow.clipJob.sourceSize
                x: cropOverlay.displayRect.x + modelData.x / Math.max(1, sourceSize.width) * cropOverlay.displayRect.width
                y: cropOverlay.displayRect.y + modelData.y / Math.max(1, sourceSize.height) * cropOverlay.displayRect.height
                width: modelData.width / Math.max(1, sourceSize.width) * cropOverlay.displayRect.width
                height: modelData.height / Math.max(1, sourceSize.height) * cropOverlay.displayRect.height
                color: "black"
                MouseArea {
                    anchors.fill: parent
                    enabled: clipPopup.editingPrivacy && !clipPopup.locked
                    onClicked: {clipPopup.stopPreview(); clipPopup.activePrivacyIndex = parent.index}
                }
            }
        }
    }

    PrivacyRegionOverlay {
        objectName: "nkshotVideoPrivacyOverlay"
        anchors.fill: videoOutput
        visible: clipPopup.opened && contextWindow.clipJob.ready && contextWindow.clipJob.privacyEnabled && !root.captureActive
        interactive: visible && clipPopup.editingPrivacy && !clipPopup.locked
        sourceSize: contextWindow.clipJob.sourceSize
        region: clipPopup.activePrivacyRect
        regionIndex: clipPopup.activePrivacyIndex
        displayRect: cropOverlay.displayRect
        onRectRequested: (x, y, width, height) => {
            clipPopup.stopPreview(); contextWindow.clipJob.setPrivacyRectAt(clipPopup.activePrivacyIndex, x, y, width, height)
        }
    }

    AudioOutput {
        id: audioOutput
        objectName: "nkshotPlaybackAudio"
    }

    MediaPlayer {
        id: mediaPlayer
        objectName: "nkshotMediaPlayer"
        source: root.usesAnimatedImage ? "" : root.sourceUrl
        videoOutput: videoOutput
        audioOutput: audioOutput
        onHasVideoChanged: if (hasVideo) {
            pause();
        }
        onPlaybackStateChanged: if (playbackState === MediaPlayer.StoppedState) {
            pause()
        }
    }

    QQC.Pane {
        anchors.centerIn: parent
        width: Math.min(420, parent.width - 24)
        visible: root.playbackFailed && !root.captureActive
        padding: 20
        background: Kos.KosSurface { fillColor: Kos.AppTheme.contentBackground; radius: 16; elevation: 0 }
        contentItem: ColumnLayout {
            spacing: 12
            QQC.Label {
                Layout.fillWidth: true
                text: i18nc("@title", "This recording could not be played")
                font.bold: true
                wrapMode: Text.Wrap
                color: Kos.AppTheme.paletteText
            }
            QQC.Label {
                Layout.fillWidth: true
                text: i18nc("@info", "The file may be incomplete or its format unavailable. The original file has not been changed.")
                wrapMode: Text.Wrap
                color: Kos.AppTheme.mutedText
            }
            QQC.Label {
                Layout.fillWidth: true
                text: mediaPlayer.errorString
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                visible: text.length > 0
                color: Kos.AppTheme.mutedText
            }
            QQC.Button {
                id: retryButton
                objectName: "nkshotPlaybackRetry"
                implicitHeight: 34
                leftPadding: 14
                rightPadding: 14
                palette.buttonText: Kos.AppTheme.accentText
                background: Kos.KosSurface {
                    fillColor: Kos.AppTheme.accent
                    radius: 10
                    elevation: 0
                    strokeWidth: 0
                    hovered: retryButton.hovered
                    pressed: retryButton.down
                    focused: retryButton.visualFocus
                }
                text: i18nc("@action", "Retry playback")
                onClicked: root.reloadMedia()
            }
        }
    }

    // Keep capture controls separate from the editor. During finalization the
    // encoder still owns its output: never present playback or a saved claim.
    readonly property string autoStopRemainingText: {
        const seconds = Math.max(0, Math.ceil(SpectacleCore.recordingAutoStopRemaining / 1000))
        return Math.floor(seconds / 60).toString().padStart(2, "0") + ":"
            + (seconds % 60).toString().padStart(2, "0")
    }
    readonly property bool finalizing: SpectacleCore.videoPlatform.recordingState === VideoPlatform.Rendering
    readonly property bool captureActive: SpectacleCore.videoPlatform.isRecording || finalizing
    QQC.Pane {
        objectName: "nkshotRecordingStatus"
        anchors.centerIn: parent
        width: Math.min(parent.width - 40, 400)
        padding: 28
        visible: root.captureActive
        background: Kos.KosSurface {
            fillColor: Kos.AppTheme.contentBackground
            radius: 24
            elevation: 0
            strokeWidth: 1
            strokeColor: Kos.AppTheme.border
        }
        contentItem: ColumnLayout {
            spacing: 20
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 8
                Rectangle {
                    width: 8; height: 8; radius: 4
                    color: SpectacleCore.videoPlatform.isPaused || root.finalizing ? Kos.AppTheme.mutedText : "#e44b50"
                }
                QQC.Label {
                    text: root.finalizing ? i18nc("@info", "Finishing recording")
                        : SpectacleCore.videoPlatform.isPaused ? i18nc("@info", "Recording paused") : i18nc("@info", "Recording")
                    font.family: Qt.application.font.family
                    font.pixelSize: Qt.application.font.pixelSize
                    font.weight: Font.DemiBold
                    color: Kos.AppTheme.paletteText
                }
            }
            QQC.Label {
                objectName: "nkshotRecordingElapsed"
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: SpectacleCore.recordedTime
                font.family: Qt.application.font.family
                font.pixelSize: 44
                font.weight: Font.Light
                color: Kos.AppTheme.paletteText
                Accessible.name: i18nc("@label", "Recorded time: %1", text)
            }
            QQC.Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                objectName: "nkshotRecordingStatusDescription"
                text: root.finalizing ? i18nc("@info", "Preparing your video for review…")
                    : SpectacleCore.recordingAutoStopActive
                        ? (SpectacleCore.videoPlatform.isPaused
                            ? i18nc("@info", "Auto-stop in %1 · Paused", root.autoStopRemainingText)
                            : i18nc("@info", "Auto-stop in %1", root.autoStopRemainingText))
                    : SpectacleCore.videoPlatform.isPaused ? i18nc("@info", "Your recording will continue from here.")
                    : i18nc("@info", "Finish when you are ready to review and trim.")
                color: Kos.AppTheme.mutedText
                font: Qt.application.font
            }
            RowLayout {
                Layout.fillWidth: true
                visible: !root.finalizing
                spacing: 12
                QQC.Button {
                    id: pauseCapture
                    objectName: "nkshotRecordingPause"
                    Layout.fillWidth: true
                    implicitHeight: 44
                    visible: SpectacleCore.videoPlatform.pauseSupported
                    enabled: SpectacleCore.videoPlatform.pauseSupported && !SpectacleCore.videoPlatform.pausePending
                    text: SpectacleCore.videoPlatform.pausePending
                        ? (SpectacleCore.videoPlatform.isPaused ? i18nc("@action", "Resuming…") : i18nc("@action", "Pausing…"))
                        : (SpectacleCore.videoPlatform.isPaused ? i18nc("@action", "Resume") : i18nc("@action", "Pause"))
                    onClicked: SpectacleCore.toggleRecordingPause()
                    font: Qt.application.font
                    contentItem: QQC.Label {
                        text: pauseCapture.text; font: pauseCapture.font
                        color: Kos.AppTheme.paletteText
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    opacity: enabled ? 1 : 0.45
                    background: Kos.KosSurface {
                        fillColor: Kos.AppTheme.controlBackground
                        radius: 12; elevation: 0; strokeWidth: 0
                        hovered: pauseCapture.hovered; pressed: pauseCapture.down; focused: pauseCapture.visualFocus
                    }
                }
                QQC.Button {
                    id: stopCapture
                    objectName: "nkshotRecordingStop"
                    Layout.fillWidth: true
                    implicitHeight: 44
                    text: i18nc("@action", "Finish recording")
                    onClicked: SpectacleCore.finishRecording()
                    font: Qt.application.font
                    contentItem: QQC.Label {
                        text: stopCapture.text; font: stopCapture.font
                        color: Kos.AppTheme.accentText
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    background: Kos.KosSurface {
                        fillColor: Kos.AppTheme.accent
                        radius: 12; elevation: 0; strokeWidth: 0
                        hovered: stopCapture.hovered; pressed: stopCapture.down; focused: stopCapture.visualFocus
                    }
                }
            }
            QQC.BusyIndicator {
                objectName: "nkshotRecordingFinalizing"
                Layout.alignment: Qt.AlignHCenter
                visible: root.finalizing
                running: visible
                implicitWidth: 28; implicitHeight: 28
                Accessible.name: i18nc("@info", "Finishing recording")
            }
        }
    }

    HoverHandler {
        id: tbHoverHandler
    }

    FloatingToolBar {
        id: toolBar
        objectName: "nkshotPlaybackControls"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.margins: padding * 2
        width: root.hasAnimatedImage ? implicitWidth : parent.width - anchors.margins * 2
        visible: false
        opacity: 0
        enabled: root.hasContent && !clipPopup.opened && !root.captureActive
        contentItem: RowLayout {
            spacing: parent.spacing
            TtToolButton {
                id: playPauseButton
                objectName: "nkshotPlaybackToggle"
                containmentMask: Item {
                    parent: playPauseButton
                    anchors {
                        fill: parent
                        leftMargin: toolBar.mirrored ? -toolBar.rightPadding : -toolBar.leftPadding
                        rightMargin: {
                            if (root.hasAnimatedImage) {
                                return toolBar.mirrored ? -toolBar.leftPadding : -toolBar.rightPadding
                            }
                            return -toolBar.spacing / 2
                        }
                        topMargin: -toolBar.topPadding
                        bottomMargin: -toolBar.bottomPadding
                    }
                }
                icon.name: root.playing ?
                    "media-playback-pause" : "media-playback-start"
                text: root.playing ? i18n("Pause") : i18n("Play")
                display: QQC.ToolButton.IconOnly
                onClicked: root.togglePlay()
            }
            QQC.Slider {
                id: seekBar
                objectName: "nkshotPlaybackSeek"
                Accessible.name: i18nc("@label", "Recording position")
                background: Rectangle {
                    x: seekBar.leftPadding
                    y: seekBar.topPadding + (seekBar.availableHeight - height) / 2
                    width: seekBar.availableWidth
                    height: 4
                    radius: 2
                    color: Kos.AppTheme.border
                    Rectangle {
                        width: seekBar.visualPosition * parent.width
                        height: parent.height
                        radius: parent.radius
                        color: Kos.AppTheme.accent
                    }
                }
                handle: Rectangle {
                    x: seekBar.leftPadding + seekBar.visualPosition * (seekBar.availableWidth - width)
                    y: seekBar.topPadding + (seekBar.availableHeight - height) / 2
                    width: 14
                    height: 14
                    radius: 7
                    color: Kos.AppTheme.contentBackground
                    border.color: Kos.AppTheme.accent
                    border.width: 2
                }
                containmentMask: Item {
                    parent: seekBar
                    anchors {
                        fill: parent
                        leftMargin: -toolBar.spacing / 2
                        rightMargin: -toolBar.spacing / 2
                        topMargin: -toolBar.topPadding
                        bottomMargin: -toolBar.bottomPadding
                    }
                }
                visible: !root.hasAnimatedImage
                Layout.fillWidth: true
                enabled: mediaPlayer.seekable
                wheelEnabled: false
                from: 0
                to: Math.max(1, mediaPlayer.duration)
                value: mediaPlayer.position
                onMoved: {
                    const pos = Math.round(value)
                    mediaPlayer.setPosition(pos)
                }
            }
            TtToolButton {
                id: playbackSettingsButton
                objectName: "nkshotPlaybackSettings"
                visible: !root.usesAnimatedImage
                enabled: mediaPlayer.hasVideo
                icon.name: "configure"
                text: i18nc("@action", "Playback and clips")
                display: QQC.ToolButton.IconOnly
                onClicked: playbackSettings.opened ? playbackSettings.close() : playbackSettings.open()
            }
            QQC.Label {
                visible: !root.hasAnimatedImage
                leftPadding: parent.spacing
                rightPadding: parent.spacing
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideNone
                wrapMode: Text.NoWrap
                text: {
                    let position = SpectacleCore.timeFromMilliseconds(mediaPlayer.position)
                    let duration = SpectacleCore.timeFromMilliseconds(mediaPlayer.duration)
                    return position + " / " + duration
                }
            }
        }
        layer.enabled: GraphicsInfo.api !== GraphicsInfo.Software
        state: "normal"
        states: [
            State {
                name: "hovered"
                when: (tbHoverHandler.hovered || !root.playing) && root.hasContent && !clipPopup.opened
                PropertyChanges {
                    target: toolBar
                    opacity: 1
                }
            },
            State {
                name: "normal"
                when: (root.playing && !tbHoverHandler.hovered) || !root.hasContent || clipPopup.opened
                PropertyChanges {
                    target: toolBar
                    opacity: 0
                }
            }
        ]
        transitions: [
            Transition {
                to: "hovered"
                SequentialAnimation {
                    PropertyAction {
                        target: toolBar
                        property: "visible"
                        value: true
                    }
                    OpacityAnimator {
                        duration: Settings.reduceUiMotion ? 0 : Kirigami.Units.longDuration
                        easing.type: Easing.OutCubic
                    }
                }
            },
            Transition {
                to: "normal"
                SequentialAnimation {
                    OpacityAnimator {
                        duration: Settings.reduceUiMotion ? 0 : Kirigami.Units.longDuration
                        easing.type: Easing.OutCubic
                    }
                    PropertyAction {
                        target: toolBar
                        property: "visible"
                        value: false
                    }
                }
            }
        ]
    }
    QQC.Popup {
        id: playbackSettings
        objectName: "nkshotPlaybackSettingsPopup"
        parent: root
        width: Math.min(280, root.width - 24)
        x: Math.max(12, root.width - width - 12)
        y: Math.max(12, toolBar.y - height - 8)
        padding: 16
        closePolicy: QQC.Popup.CloseOnEscape | QQC.Popup.CloseOnPressOutside
        background: Kos.KosSurface {fillColor: Kos.AppTheme.contentBackground; radius: 14; elevation: 0}
        contentItem: ColumnLayout {
            spacing: 12
            QQC.Label {
                text: i18nc("@title", "Playback")
                color: Kos.AppTheme.paletteText
                font.bold: true
            }
            RowLayout {
                Layout.fillWidth: true
                QQC.Label {text: i18nc("@label", "Speed"); color: Kos.AppTheme.paletteText; Layout.fillWidth: true}
                QQC.ComboBox {
                    id: speedChoice
                    implicitHeight: 32
                    leftPadding: 10
                    rightPadding: 28
                    palette.text: Kos.AppTheme.paletteText
                    palette.buttonText: Kos.AppTheme.paletteText
                    background: Kos.KosSurface {fillColor: Kos.AppTheme.controlBackground; radius: 8; elevation: 0; strokeWidth: 0}
                    contentItem: QQC.Label {text: speedChoice.displayText; color: Kos.AppTheme.paletteText; verticalAlignment: Text.AlignVCenter}
                    indicator: QQC.Label {text: "⌄"; color: Kos.AppTheme.mutedText; x: speedChoice.width - width - 10; anchors.verticalCenter: parent.verticalCenter}
                    objectName: "nkshotPlaybackSpeed"
                    model: ["0.5×", "0.75×", "1×", "1.25×", "1.5×", "2×"]
                    property var rates: [0.5, 0.75, 1, 1.25, 1.5, 2]
                    currentIndex: rates.indexOf(mediaPlayer.playbackRate)
                    Accessible.name: i18nc("@label", "Playback speed")
                    onActivated: mediaPlayer.playbackRate = rates[currentIndex]
                }
            }
            QQC.CheckBox {
                id: muteChoice
                palette.windowText: Kos.AppTheme.paletteText
                indicator: Rectangle {
                    x: muteChoice.leftPadding
                    y: (muteChoice.height - height) / 2
                    width: 18; height: 18; radius: 5
                    color: muteChoice.checked ? Kos.AppTheme.accent : Kos.AppTheme.controlBackground
                    border.color: muteChoice.checked ? Kos.AppTheme.accent : Kos.AppTheme.border
                    opacity: muteChoice.enabled ? 1 : 0.5
                    QQC.Label {anchors.centerIn: parent; text: "✓"; visible: muteChoice.checked; color: Kos.AppTheme.accentText}
                }
                objectName: "nkshotPlaybackMute"
                Layout.fillWidth: true
                text: i18nc("@option:check", "Mute playback")
                enabled: mediaPlayer.hasAudio
                checked: audioOutput.muted
                onToggled: audioOutput.muted = checked
            }
            RowLayout {
                Layout.fillWidth: true
                QQC.Slider {
                    id: volumeSlider
                    implicitHeight: 28
                    implicitWidth: 160
                    background: Rectangle {
                        x: volumeSlider.leftPadding
                        y: volumeSlider.topPadding + (volumeSlider.availableHeight - height) / 2
                        width: volumeSlider.availableWidth; height: 4; radius: 2
                        color: Kos.AppTheme.border
                        opacity: volumeSlider.enabled ? 1 : 0.5
                        Rectangle {width: volumeSlider.visualPosition * parent.width; height: parent.height; radius: 2; color: Kos.AppTheme.accent}
                    }
                    handle: Rectangle {
                        x: volumeSlider.leftPadding + volumeSlider.visualPosition * (volumeSlider.availableWidth - width)
                        y: volumeSlider.topPadding + (volumeSlider.availableHeight - height) / 2
                        width: 14; height: 14; radius: 7
                        color: Kos.AppTheme.contentBackground
                        border.color: Kos.AppTheme.accent; border.width: 2
                        opacity: volumeSlider.enabled ? 1 : 0.5
                    }
                    objectName: "nkshotPlaybackVolume"
                    Layout.fillWidth: true
                    from: 0; to: 1
                    stepSize: 0.05
                    enabled: mediaPlayer.hasAudio && !audioOutput.muted
                    value: audioOutput.volume
                    Accessible.name: i18nc("@label", "Playback volume")
                    onMoved: audioOutput.volume = value
                }
                QQC.Label {
                    text: Math.round(audioOutput.volume * 100) + "%"
                    color: Kos.AppTheme.mutedText
                    Layout.minimumWidth: 36
                }
            }
            QQC.Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: mediaPlayer.hasAudio ? i18nc("@info", "Playback only. The saved recording stays unchanged.") : i18nc("@info", "This recording has no audio track.")
                color: Kos.AppTheme.mutedText
            }
            QQC.Button {
                id: extractFrameButton
                objectName: "nkshotExtractVideoFrame"
                Layout.fillWidth: true
                text: i18nc("@action", "Extract current frame…")
                enabled: mediaPlayer.hasVideo && !root.captureActive && contextWindow.canOpenMedia
                font: Qt.application.font
                implicitHeight: Math.max(32, contentItem.implicitHeight + 12)
                contentItem: QQC.Label {
                    text: extractFrameButton.text; font: extractFrameButton.font
                    color: Kos.AppTheme.paletteText
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
                background: Kos.KosSurface {
                    fillColor: Kos.AppTheme.controlBackground
                    radius: 9; elevation: 0; strokeWidth: 0
                    hovered: extractFrameButton.hovered; pressed: extractFrameButton.down; focused: extractFrameButton.visualFocus
                }
                onClicked: {
                    mediaPlayer.pause()
                    playbackSettings.close()
                    contextWindow.pinVideoFrame(videoOutput.videoSink, root.sourceUrl)
                }
            }
            QQC.Button {
                id: inspectFramesButton
                objectName: "nkshotInspectVideoFrames"
                Layout.fillWidth: true
                text: i18nc("@action", "Browse exact frames…")
                enabled: mediaPlayer.hasVideo && !root.captureActive && contextWindow.canOpenMedia
                font: Qt.application.font
                implicitHeight: Math.max(32, contentItem.implicitHeight + 12)
                contentItem: QQC.Label {
                    text: inspectFramesButton.text; font: inspectFramesButton.font
                    color: Kos.AppTheme.paletteText
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
                background: Kos.KosSurface {
                    fillColor: Kos.AppTheme.controlBackground
                    radius: 9; elevation: 0; strokeWidth: 0
                    hovered: inspectFramesButton.hovered; pressed: inspectFramesButton.down; focused: inspectFramesButton.visualFocus
                }
                onClicked: {
                    mediaPlayer.pause()
                    playbackSettings.close()
                    contextWindow.inspectVideoFrames(root.sourceUrl, mediaPlayer.position)
                }
            }
            QQC.Button {
                id: openClipButton
                objectName: "nkshotOpenClip"
                Layout.fillWidth: true
                text: i18nc("@action", "Export a clip…")
                enabled: mediaPlayer.hasVideo && !SpectacleCore.videoPlatform.isRecording
                font: Qt.application.font
                implicitHeight: Math.max(32, contentItem.implicitHeight + 12)
                contentItem: QQC.Label {
                    text: openClipButton.text
                    font: openClipButton.font
                    color: Kos.AppTheme.paletteText
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 9
                    color: Kos.AppTheme.controlBackground
                    border.width: openClipButton.visualFocus ? 1 : 0
                    border.color: Kos.AppTheme.accent
                    opacity: openClipButton.down ? 0.7 : 1
                }
                onClicked: {playbackSettings.close(); clipPopup.open()}
            }
        }
    }
    VideoClipPopup {
        videoSink: videoOutput.videoSink
        id: clipPopup
        parent: root
        player: mediaPlayer
        sourceUrl: root.sourceUrl
    }
    onSourceUrlChanged: {playbackSettings.close(); clipPopup.close(); contextWindow.cancelClipExport()}

}
