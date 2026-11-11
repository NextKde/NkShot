// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import Kos.Ui as Kos

Item {
    id: root
    // Kept as a binding so isolated visual tests can inspect the selector layout
    // without enabling a backend or accepting synthetic capture results.
    property int phase: scrollWindow.phase
    component ActionButton: QQC.Button {
        property bool primary: false
        implicitHeight: 36
        leftPadding: 16; rightPadding: 16
        palette.buttonText: primary ? Kos.AppTheme.accentText : Kos.AppTheme.paletteText
        background: Kos.KosSurface {
            fillColor: parent.primary ? Kos.AppTheme.accent : Kos.AppTheme.controlBackground
            radius: 10; elevation: 0; strokeWidth: 0
            focused: parent.visualFocus; hovered: parent.hovered; pressed: parent.down
            opacity: parent.enabled ? 1 : 0.35
        }
    }
    Rectangle {
        anchors.fill: parent
        color: root.phase === 1 ? "#66000000" : Kos.AppTheme.contentBackground
    }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 20
        visible: root.phase !== 1
        spacing: 12
        RowLayout {
            QQC.Label { text: i18nc("@title", "Scrolling capture"); font.pixelSize: 18; font.bold: true; color: Kos.AppTheme.paletteText }
            Item { Layout.fillWidth: true }
            QQC.Label { text: i18nc("@info", "Preview"); font.pixelSize: 11; color: Kos.AppTheme.mutedText }
        }
        QQC.Label {
            Layout.fillWidth: true
            text: root.phase === 0 ? i18nc("@info", "One region. You scroll. NkShot joins matching frames.")
                : i18ncp("@info", "%1 frame", "%1 frames", scrollWindow.frameCount)
            color: Kos.AppTheme.mutedText; wrapMode: Text.Wrap
        }
        QQC.ComboBox {
            id: outputPicker
            implicitHeight: 36
            palette.text: Kos.AppTheme.paletteText
            palette.buttonText: Kos.AppTheme.paletteText
            palette.base: Kos.AppTheme.contentBackground
            palette.window: Kos.AppTheme.contentBackground
            background: Kos.KosSurface {fillColor: Kos.AppTheme.controlBackground; radius: 10; elevation: 0; strokeWidth: 0}
            contentItem: QQC.Label {text: outputPicker.displayText; color: Kos.AppTheme.paletteText; leftPadding: 12; verticalAlignment: Text.AlignVCenter; opacity: outputPicker.enabled ? 1 : 0.45}
            indicator: QQC.Label {x: outputPicker.width-width-12; y: (outputPicker.height-height)/2; text: "⌄"; color: Kos.AppTheme.mutedText}
            objectName: "nkshotScrollOutput"
            visible: root.phase === 0
            Layout.fillWidth: true
            model: scrollWindow.outputs
            currentIndex: scrollWindow.outputIndex
            enabled: scrollWindow.available
            onActivated: scrollWindow.outputIndex = currentIndex
            Accessible.name: i18nc("@label", "Display to capture")
        }
        QQC.ScrollView {
            id: statusScroll
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true
            QQC.Label {
                width: statusScroll.availableWidth
                text: scrollWindow.statusText
                color: Kos.AppTheme.mutedText
                wrapMode: Text.Wrap
                Accessible.role: Accessible.StaticText
            }
        }
        RowLayout {
            Layout.fillWidth: true
            ActionButton { text: i18nc("@action", "Cancel"); onClicked: scrollWindow.cancelPreview() }
            Item { Layout.fillWidth: true }
            ActionButton {
                visible: root.phase === 2
                text: i18nc("@action", "Finish")
                enabled: !scrollWindow.busy && scrollWindow.canFinish
                onClicked: scrollWindow.finishCapture()
            }
            ActionButton {
                objectName: "nkshotScrollPrimary"
                primary: true
                text: root.phase === 0 ? i18nc("@action", "Select region")
                    : scrollWindow.busy ? i18nc("@info", "Capturing…")
                    : scrollWindow.frameCount === 0 ? i18nc("@action", "Capture first") : i18nc("@action", "Capture next")
                enabled: scrollWindow.available && !scrollWindow.busy && (root.phase === 0 || scrollWindow.canCapture)
                onClicked: root.phase === 0 ? scrollWindow.selectRegion() : scrollWindow.captureNext()
            }
        }
    }
    MouseArea {
        id: selector
        objectName: "nkshotScrollSelectionArea"
        anchors.fill: parent
        visible: root.phase === 1
        cursorShape: Qt.CrossCursor
        property point origin
        property point end
        property bool drawing: false
        onPressed: event => { origin = Qt.point(event.x, event.y); end = origin; drawing = true }
        onPositionChanged: event => { if (drawing) end = Qt.point(Math.max(0, Math.min(width, event.x)), Math.max(0, Math.min(height, event.y))) }
        onReleased: {
            drawing = false
            if (selection.width > 0 && selection.height > 0)
                scrollWindow.acceptRegion(Qt.rect(selection.x, selection.y, selection.width, selection.height))
        }
        onCanceled: drawing = false
        Rectangle {
            id: selection
            x: Math.min(selector.origin.x, selector.end.x); y: Math.min(selector.origin.y, selector.end.y)
            width: Math.abs(selector.end.x - selector.origin.x); height: Math.abs(selector.end.y - selector.origin.y)
            color: "#11007aff"; border.color: "#ffffff"; border.width: 2
            visible: width > 0 && height > 0
        }
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter; y: 24
            width: Math.min(parent.width - 32, 460); height: hint.implicitHeight + 24
            radius: 12; color: "#ee202126"
            QQC.Label {
                id: hint
                anchors.centerIn: parent; width: parent.width - 28
                text: scrollWindow.phase === 1 ? scrollWindow.statusText : i18nc("@info", "Drag a region on this display. Esc cancels.\nKeep fixed headers and changing content outside the region.")
                wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter; color: "white"
            }
        }
    }
}
