// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import Kos.Ui as Kos

QQC.Pane {
    id: root
    property bool selectingArea: false
    property rect selectedArea: Qt.rect(0, 0, 0, 0)
    function cancelArea() { textMenu.close(); selectingArea = false; selectedArea = Qt.rect(0, 0, 0, 0) }
    function beginArea() { selectedArea = Qt.rect(0, 0, 0, 0); selectingArea = true; saveStatus.text = "" }
    padding: 12
    palette.windowText: Kos.AppTheme.paletteText
    palette.text: Kos.AppTheme.paletteText
    palette.buttonText: Kos.AppTheme.paletteText
    background: Kos.KosSurface {
        fillColor: Kos.AppTheme.contentSecondaryBackground
        radius: 14
        elevation: 0
    }
    component PinButton: QQC.ToolButton {
        property bool selected: false
        icon.color: Kos.AppTheme.paletteText
        implicitWidth: display === QQC.AbstractButton.IconOnly ? 36 : implicitContentWidth + leftPadding + rightPadding
        implicitHeight: 36
        QQC.ToolTip.visible: hovered
        QQC.ToolTip.text: text
        background: Kos.KosSurface {
            fillColor: parent.selected || parent.checked || parent.hovered ? Kos.AppTheme.controlBackground : "transparent"
            radius: 9
            elevation: 0
            strokeWidth: 0
        }
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        RowLayout {
            Layout.fillWidth: true
            PinButton {
                objectName: "nkshotPinTop"
                text: i18nc("@action", "Stay on Top")
                icon.name: "window-pin"
                display: QQC.AbstractButton.IconOnly
                checkable: true
                checked: pinnedWindow.stayOnTop
                onClicked: pinnedWindow.stayOnTop = checked
            }
            QQC.Label {
                // Keep all actions reachable at the 260 px minimum width.
                visible: root.width >= 440
                text: pinnedWindow.snapshotSize.width + " × " + pinnedWindow.snapshotSize.height
                color: Kos.AppTheme.paletteText
            }
            Item { Layout.fillWidth: true }
            PinButton {
                id: copyButton
                objectName: "nkshotPinCopy"
                property bool copied: false
                text: copied ? i18nc("@info", "Copied") : i18nc("@action", "Copy")
                icon.name: copied ? "dialog-ok" : "edit-copy"
                display: root.width < 500 ? QQC.AbstractButton.IconOnly : QQC.AbstractButton.TextBesideIcon
                Connections {
                    target: pinnedWindow
                    function onImageCopied() { copyButton.copied = true; copyFeedback.restart() }
                }
                Timer {
                    id: copyFeedback
                    interval: 1800
                    onTriggered: copyButton.copied = false
                }
                onClicked: pinnedWindow.copyImage()
            }
            PinButton {
                objectName: "nkshotPinExtractText"
                text: pinnedWindow.extractingText ? i18nc("@info", "Reading text…") : i18nc("@action", "Extract text")
                display: QQC.AbstractButton.IconOnly
                icon.name: pinnedWindow.extractingText ? "" : "document-scan"
                enabled: !pinnedWindow.extractingText
                onClicked: textMenu.popup()
                QQC.Menu {
                    id: textMenu
                    QQC.MenuItem { objectName: "nkshotPinReadWhole"; text: i18nc("@action", "Read whole image"); onTriggered: { root.cancelArea(); pinnedWindow.extractText() } }
                    QQC.MenuItem { objectName: "nkshotPinChooseArea"; text: i18nc("@action", "Select an area…"); onTriggered: root.beginArea() }
                }
                QQC.BusyIndicator {anchors.centerIn: parent; width: 20; height: 20; running: pinnedWindow.extractingText; visible: running}
            }
            PinButton {
                objectName: "nkshotPinSave"
                text: i18nc("@action", "Save As…")
                display: QQC.AbstractButton.IconOnly
                icon.name: "document-save-as"
                onClicked: pinnedWindow.saveAs()
            }
            PinButton {
                objectName: "nkshotPinClose"
                text: i18nc("@action", "Close")
                display: QQC.AbstractButton.IconOnly
                icon.name: "window-close"
                onClicked: pinnedWindow.close()
            }
        }
        QQC.Label {
            id: saveStatus
            objectName: "nkshotPinSaveStatus"
            Layout.fillWidth: true
            visible: text.length > 0
            elide: Text.ElideRight
            font.pixelSize: 12
            color: Kos.AppTheme.paletteText
            QQC.ToolTip.text: text
            QQC.ToolTip.visible: statusHover.hovered
            HoverHandler { id: statusHover }
            Connections {
                target: pinnedWindow
                function onSaveFinished(message, success) { saveStatus.text = message }
                function onTextExtractionStatus(message) { saveStatus.text = message }
            }
        }
        Flickable {
            id: viewport
            objectName: "nkshotPinViewport"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            interactive: !root.selectingArea
            boundsBehavior: Flickable.StopAtBounds
            property bool fitMode: true
            property bool widthMode: false
            property real manualScale: 1
            readonly property real fitScale: Math.min(1, width / Math.max(1, pinnedWindow.snapshotSize.width), height / Math.max(1, pinnedWindow.snapshotSize.height))
            readonly property real imageScale: fitMode ? fitScale : widthMode ? Math.min(1, width / Math.max(1, pinnedWindow.snapshotSize.width)) : manualScale
            contentWidth: Math.max(width, pinnedWindow.snapshotSize.width * imageScale)
            contentHeight: Math.max(height, pinnedWindow.snapshotSize.height * imageScale)
            function clampPosition() {
                contentX = Math.max(0, Math.min(contentX, contentWidth - width))
                contentY = Math.max(0, Math.min(contentY, contentHeight - height))
            }
            function fitImage() {
                widthMode = false
                fitMode = true
                contentX = 0; contentY = 0
            }
            function fitWidth() {
                fitMode = false; widthMode = true
                contentX = 0; contentY = 0
            }
            function zoomTo(scale, anchorX, anchorY) {
                const oldScale = imageScale
                const pixelX = (contentX + anchorX - snapshot.x) / oldScale
                const pixelY = (contentY + anchorY - snapshot.y) / oldScale
                manualScale = Math.max(Math.min(fitScale, 0.1), Math.min(4, scale))
                fitMode = false; widthMode = false
                contentX = pixelX * imageScale + snapshot.x - anchorX
                contentY = pixelY * imageScale + snapshot.y - anchorY
                clampPosition()
            }
            function zoomBy(factor) { zoomTo(imageScale * factor, width / 2, height / 2) }
            onWidthChanged: Qt.callLater(clampPosition)
            onHeightChanged: Qt.callLater(clampPosition)
            Image {
                id: snapshot
                objectName: "nkshotPinSnapshot"
                x: (viewport.contentWidth - width) / 2
                y: (viewport.contentHeight - height) / 2
                width: pinnedWindow.snapshotSize.width * viewport.imageScale
                height: pinnedWindow.snapshotSize.height * viewport.imageScale
                source: pinnedWindow.imageSource
                smooth: viewport.imageScale < 1
                Rectangle {
                    visible: root.selectingArea && root.selectedArea.width > 0
                    x: root.selectedArea.x * viewport.imageScale
                    y: root.selectedArea.y * viewport.imageScale
                    width: root.selectedArea.width * viewport.imageScale
                    height: root.selectedArea.height * viewport.imageScale
                    color: "#223b82f6"
                    border.color: "#3b82f6"
                    border.width: 2
                }
                MouseArea {
                    objectName: "nkshotPinAreaSurface"
                    anchors.fill: parent
                    enabled: root.selectingArea
                    cursorShape: Qt.CrossCursor
                    preventStealing: true
                    property point origin
                    function pixel(x, y) { return Qt.point(Math.max(0, Math.min(pinnedWindow.snapshotSize.width, x / viewport.imageScale)), Math.max(0, Math.min(pinnedWindow.snapshotSize.height, y / viewport.imageScale))) }
                    function update(x, y) {
                        const end = pixel(x, y)
                        const left = Math.floor(Math.min(origin.x, end.x))
                        const top = Math.floor(Math.min(origin.y, end.y))
                        root.selectedArea = Qt.rect(left, top, Math.ceil(Math.max(origin.x, end.x)) - left, Math.ceil(Math.max(origin.y, end.y)) - top)
                    }
                    onPressed: mouse => { origin = pixel(mouse.x, mouse.y); update(mouse.x, mouse.y) }
                    onPositionChanged: mouse => { if (pressed) update(mouse.x, mouse.y) }
                    onReleased: mouse => update(mouse.x, mouse.y)
                }
            }
            QQC.ScrollBar.vertical: QQC.ScrollBar {policy: viewport.contentHeight > viewport.height ? QQC.ScrollBar.AsNeeded : QQC.ScrollBar.AlwaysOff}
            QQC.ScrollBar.horizontal: QQC.ScrollBar {policy: viewport.contentWidth > viewport.width ? QQC.ScrollBar.AsNeeded : QQC.ScrollBar.AlwaysOff}
            WheelHandler {
                target: null
                acceptedModifiers: Qt.ControlModifier
                onWheel: event => viewport.zoomTo(viewport.imageScale * Math.pow(1.2, event.angleDelta.y / 120), event.x, event.y)
            }
        }
        ColumnLayout {
            visible: root.selectingArea
            Layout.fillWidth: true
            Layout.minimumHeight: 48
            Layout.maximumHeight: 48
            spacing: 4
            QQC.Label {
                Layout.fillWidth: true
                text: root.selectedArea.width >= 2 && root.selectedArea.height >= 2
                    ? i18nc("@info", "%1 × %2 px · Local text only", root.selectedArea.width, root.selectedArea.height)
                    : i18nc("@info", "Drag across text to select an area")
                color: Kos.AppTheme.paletteText
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 11
                elide: Text.ElideRight
            }
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                PinButton { implicitHeight: 28; objectName: "nkshotPinCancelArea"; text: i18nc("@action", "Cancel"); onClicked: root.cancelArea() }
                PinButton {
                    objectName: "nkshotPinReadArea"
                    implicitHeight: 28
                    text: i18nc("@action", "Read area")
                    selected: true
                    enabled: !pinnedWindow.extractingText && root.selectedArea.width >= 2 && root.selectedArea.height >= 2
                    onClicked: { pinnedWindow.extractTextArea(root.selectedArea); root.cancelArea() }
                }
            }
        }
        RowLayout {
            visible: !root.selectingArea
            Layout.alignment: Qt.AlignHCenter
            Layout.minimumHeight: 48
            Layout.maximumHeight: 48
            spacing: 2
            PinButton {
                objectName: "nkshotPinFit"
                text: i18nc("@action", "Fit")
                display: QQC.AbstractButton.TextOnly
                selected: viewport.fitMode
                implicitHeight: 28
                onClicked: viewport.fitImage()
            }
            PinButton {
                objectName: "nkshotPinFitWidth"
                text: i18nc("@action:button fit image to viewport width", "Width")
                display: QQC.AbstractButton.TextOnly
                selected: viewport.widthMode
                implicitHeight: 28
                QQC.ToolTip.text: i18nc("@action", "Fit width")
                Accessible.name: i18nc("@action", "Fit width")
                onClicked: viewport.fitWidth()
            }
            PinButton {
                objectName: "nkshotPinZoomOut"
                text: i18nc("@action", "Zoom out")
                display: QQC.AbstractButton.IconOnly
                icon.name: "zoom-out"
                implicitHeight: 28
                enabled: viewport.imageScale > Math.min(viewport.fitScale, 0.1) + 0.001
                onClicked: viewport.zoomBy(1 / 1.25)
            }
            PinButton {
                objectName: "nkshotPinActualSize"
                text: Math.round(viewport.imageScale * 100) + "%"
                display: QQC.AbstractButton.TextOnly
                implicitWidth: 52
                implicitHeight: 28
                QQC.ToolTip.text: i18nc("@action", "Actual size (100%)")
                Accessible.name: i18nc("@action", "Actual size (100%)")
                onClicked: viewport.zoomTo(1, viewport.width / 2, viewport.height / 2)
            }
            PinButton {
                objectName: "nkshotPinZoomIn"
                text: i18nc("@action", "Zoom in")
                display: QQC.AbstractButton.IconOnly
                icon.name: "zoom-in"
                implicitHeight: 28
                enabled: viewport.imageScale < 4
                onClicked: viewport.zoomBy(1.25)
            }
        }
    }
    Shortcut {sequence: "Ctrl+0"; onActivated: viewport.fitImage()}
    Shortcut {sequence: "Ctrl+1"; onActivated: viewport.zoomTo(1, viewport.width / 2, viewport.height / 2)}
    Shortcut {sequences: [StandardKey.ZoomIn]; onActivated: viewport.zoomBy(1.25)}
    Shortcut {sequences: [StandardKey.ZoomOut]; onActivated: viewport.zoomBy(1 / 1.25)}
}
