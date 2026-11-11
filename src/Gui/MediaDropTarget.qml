// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import Kos.Ui as Kos
import QtQuick.Controls as QQC

// Local-file admission and opening remain with the owner. A successful callback
// means the open/confirmation workflow started, not that decoding succeeded.
Item {
    id: root
    objectName: "nkshotMediaDropTarget"
    property var canAcceptCallback
    property var openCallback
    property bool dragValid: false
    function clear() { dragValid = false }
    function accepts(event) {
        return enabled && visible && event.hasUrls
            && (event.supportedActions & Qt.CopyAction) !== 0
            && typeof canAcceptCallback === "function" && canAcceptCallback(event.urls)
    }
    onEnabledChanged: if (!enabled) clear()
    onVisibleChanged: if (!visible) clear()
    Connections {
        target: root.Window.window
        function onActiveChanged() {
            if (!root.Window.window || !root.Window.window.active) root.clear()
        }
    }
    DropArea {
        anchors.fill: parent
        keys: ["text/uri-list"]
        onEntered: drag => {
            root.dragValid = root.accepts(drag)
            if (root.dragValid) drag.accept(Qt.CopyAction)
            else drag.accepted = false
        }
        onPositionChanged: drag => {
            root.dragValid = root.accepts(drag)
            if (root.dragValid) drag.accept(Qt.CopyAction)
            else drag.accepted = false
        }
        onExited: root.clear()
        onDropped: drop => {
            const valid = root.accepts(drop)
            root.clear()
            drop.accepted = false
            if (valid && typeof root.openCallback === "function" && root.openCallback(drop.urls))
                drop.accept(Qt.CopyAction)
        }
    }
    Rectangle {
        objectName: "nkshotMediaDropOverlay"
        anchors.fill: parent
        anchors.margins: 10
        visible: root.dragValid && root.enabled && root.visible
        radius: 16
        color: Qt.rgba(Kos.AppTheme.accent.r, Kos.AppTheme.accent.g, Kos.AppTheme.accent.b, 0.13)
        border.color: Kos.AppTheme.accent
        border.width: 2
        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width - 32, 300)
            height: guidance.implicitHeight + 32
            radius: 14
            color: Kos.AppTheme.contentBackground
            ColumnLayout {
                id: guidance
                anchors.centerIn: parent
                width: parent.width - 32
                spacing: 6
                QQC.Label {
                    Layout.fillWidth: true
                    text: i18nc("@info", "Drop to open")
                    font.pixelSize: 19
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    color: Kos.AppTheme.paletteText
                }
                QQC.Label {
                    Layout.fillWidth: true
                    text: i18nc("@info", "Unsaved edits stay protected")
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    color: Kos.AppTheme.mutedText
                }
            }
        }
    }
}
