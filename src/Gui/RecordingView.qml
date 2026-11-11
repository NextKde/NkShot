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
    readonly property bool playbackFailed: (animatedImage.source.toString().length > 0 && animatedImage.status === Image.Error)
        || (!hasAnimatedImage && mediaPlayer.error !== MediaPlayer.NoError)
    function reloadMedia() {
        mediaPlayer.source = ""
        animatedImage.source = ""
        Qt.callLater(function() {
            animatedImage.source = Qt.binding(function() {
                const format = SpectacleCore.videoPlatform.formatForPath(root.sourceUrl.toString())
                return (format === VideoPlatform.WebP || format === VideoPlatform.Gif) ? root.sourceUrl : ""
            })
            mediaPlayer.source = Qt.binding(function() { return root.hasAnimatedImage ? "" : root.sourceUrl })
        })
    }
    readonly property bool hasAnimatedImage: animatedImage.status === Image.Ready
    readonly property bool hasContent: hasAnimatedImage || mediaPlayer.hasVideo
    readonly property bool playing: hasAnimatedImage ? animatedImage.playing : mediaPlayer.playing

    function togglePlay() {
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
        enabled: root.hasContent
        onPositionChanged: {
            contextWindow.startDrag()
        }
        onClicked: root.togglePlay()
    }

    // When QtMultimedia uses the FFmpeg backend, it can't play certain animated
    // image formats such as WebP.
    AnimatedImage {
        id: animatedImage
        anchors.fill: parent
        source: {
            const format = SpectacleCore.videoPlatform.formatForPath(root.sourceUrl.toString())
            if (format === VideoPlatform.WebP || format === VideoPlatform.Gif) {
                return root.sourceUrl
            }
            return ""
        }
        visible: status === Image.Ready
        fillMode: Image.PreserveAspectFit
    }

    VideoOutput {
        id: videoOutput
        visible: mediaPlayer.hasVideo && GraphicsInfo.api !== GraphicsInfo.Software
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
    }

    SoftwareVideoPreview {
        objectName: "nkshotSoftwareVideoPreview"
        anchors.fill: parent
        sink: videoOutput.videoSink
        active: GraphicsInfo.api === GraphicsInfo.Software && mediaPlayer.hasVideo
        visible: active
    }

    AudioOutput {
        id: audioOutput
    }

    MediaPlayer {
        id: mediaPlayer
        objectName: "nkshotMediaPlayer"
        source: root.hasAnimatedImage ? "" : root.sourceUrl
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
        visible: root.playbackFailed && !SpectacleCore.videoPlatform.isRecording
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

    Kirigami.Heading {
        anchors.fill: parent
        visible: SpectacleCore.videoPlatform.isRecording
        text: i18nc("Recording in progress, %1 is the length of the recording so far", "Recording:\n%1", SpectacleCore.recordedTime)
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    HoverHandler {
        id: tbHoverHandler
    }

    FloatingToolBar {
        id: toolBar
        objectName: "nkshotPlaybackControls"
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: padding * 2
        width: root.hasAnimatedImage ? implicitWidth : parent.width - anchors.margins * 2
        visible: false
        opacity: 0
        enabled: root.hasContent
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
                when: (tbHoverHandler.hovered || !root.playing) && root.hasContent
                PropertyChanges {
                    target: toolBar
                    opacity: 1
                }
            },
            State {
                name: "normal"
                when: (root.playing && !tbHoverHandler.hovered) || !root.hasContent
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
}
