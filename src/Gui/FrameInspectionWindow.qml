// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import Kos.Ui as Kos

QQC.Pane {
    id: root
    objectName: "nkshotFrameInspection"
    readonly property var session: inspectionWindow.session
    readonly property int navigationIndex: session.requestedIndex >= 0 ? session.requestedIndex : session.currentIndex
    padding: 20
    font: Qt.application.font
    palette.text: Kos.AppTheme.paletteText
    palette.windowText: Kos.AppTheme.paletteText
    palette.buttonText: Kos.AppTheme.paletteText
    background: Rectangle {color: Kos.AppTheme.contentBackground}
    function step(delta) {
        const next = navigationIndex + delta
        if (next >= 0 && next < session.frameCount) session.requestFrame(next)
    }
    Shortcut {sequence: "Left"; enabled: root.navigationIndex > 0; onActivated: root.step(-1)}
    Shortcut {sequence: "Right"; enabled: root.navigationIndex >= 0 && root.navigationIndex + 1 < root.session.frameCount; onActivated: root.step(1)}
    component Action: QQC.Button {
        id: control
        implicitHeight: 36
        opacity: enabled ? 1 : 0.45
        leftPadding: 14; rightPadding: 14
        font: root.font
        background: Kos.KosSurface {
            fillColor: Kos.AppTheme.controlBackground
            radius: 10; elevation: 0; strokeWidth: 0
            hovered: control.hovered; pressed: control.down; focused: control.visualFocus
        }
    }
    contentItem: ColumnLayout {
        spacing: 12
        QQC.Label {
            text: i18nc("@title", "Browse video frames")
            font.pixelSize: 20; font.weight: Font.DemiBold
            color: Kos.AppTheme.paletteText
        }
        QQC.Label {
            Layout.fillWidth: true
            text: i18nc("@info", "Choose the exact decoded frame to extract as an image. Your video stays unchanged.")
            wrapMode: Text.Wrap
            color: Kos.AppTheme.mutedText
        }
        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true
            Layout.minimumHeight: 120
            color: Kos.AppTheme.controlBackground
            radius: 12
            Image {
                objectName: "nkshotFrameInspectionPreview"
                anchors.fill: parent; anchors.margins: 12
                source: root.session.previewUrl
                fillMode: Image.PreserveAspectFit
                cache: false
                visible: root.session.ready && !root.session.busy && status === Image.Ready
            }
            QQC.BusyIndicator {anchors.centerIn: parent; running: root.session.busy; visible: running}
            QQC.Label {
                anchors.centerIn: parent; width: parent.width - 32
                visible: !root.session.ready && !root.session.busy
                text: root.session.error.length ? root.session.error : i18nc("@info", "No frame available.")
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                color: Kos.AppTheme.mutedText
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Action {
                objectName: "nkshotFrameInspectionPrevious"
                text: i18nc("@action", "Previous")
                enabled: root.navigationIndex > 0
                onClicked: root.step(-1)
            }
            QQC.Label {
                objectName: "nkshotFrameInspectionIndex"
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: root.session.ready && !root.session.busy
                    ? i18nc("@info", "Frame %1 of %2", root.session.currentIndex + 1, root.session.frameCount)
                    : root.session.busy && root.session.frameCount > 0 && root.navigationIndex >= 0
                        ? i18nc("@info", "Loading frame %1…", root.navigationIndex + 1)
                        : root.session.busy ? i18nc("@info", "Reading frames…") : i18nc("@info", "Frame unavailable")
                wrapMode: Text.Wrap
                color: Kos.AppTheme.paletteText
            }
            Action {
                objectName: "nkshotFrameInspectionNext"
                text: i18nc("@action", "Next")
                enabled: root.navigationIndex >= 0 && root.navigationIndex + 1 < root.session.frameCount
                onClicked: root.step(1)
            }
        }
        QQC.Label {
            objectName: "nkshotFrameInspectionTime"
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: root.session.ready && !root.session.busy
                ? i18nc("@info", "%1 s · %2 × %3 pixels", (root.session.positionMs / 1000).toFixed(3), root.session.sourceSize.width, root.session.sourceSize.height)
                : i18nc("@info", "Left and right arrow keys browse frames")
            color: Kos.AppTheme.mutedText
            wrapMode: Text.Wrap
        }
        RowLayout {
            Layout.fillWidth: true
            Action {objectName: "nkshotFrameInspectionClose"; text: i18nc("@action", "Close"); onClicked: inspectionWindow.close()}
            Item {Layout.fillWidth: true}
            Action {
                id: extract
                objectName: "nkshotFrameInspectionExtract"
                text: i18nc("@action", "Extract copy")
                enabled: root.session.ready && !root.session.busy
                palette.buttonText: Kos.AppTheme.accentText
                background: Kos.KosSurface {
                    fillColor: Kos.AppTheme.accent; radius: 10; elevation: 0; strokeWidth: 0
                    hovered: extract.hovered; pressed: extract.down; focused: extract.visualFocus
                }
                onClicked: inspectionWindow.extractCopy()
            }
        }
    }
}
