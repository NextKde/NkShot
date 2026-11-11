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
    function beginArea() { pinnedWindow.cancelTextSelection(); selectedArea = Qt.rect(0, 0, 0, 0); selectingArea = true; saveStatus.text = "" }
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
        implicitWidth: display === QQC.AbstractButton.IconOnly ? 32 : implicitContentWidth + leftPadding + rightPadding
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
                text: copied ? i18nc("@info", "Copied") : pinnedWindow.textSelectionActive ? i18nc("@action", "Copy image") : i18nc("@action", "Copy")
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
                objectName: "nkshotPinEditCopy"
                text: i18nc("@action", "Edit a copy…")
                display: QQC.AbstractButton.IconOnly
                icon.name: "document-edit"
                enabled: pinnedWindow.snapshotSize.width * pinnedWindow.snapshotSize.height <= 40000000
                onClicked: { root.cancelArea(); pinnedWindow.cancelTextSelection(); pinnedWindow.editCopy() }
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
                    QQC.MenuItem { objectName: "nkshotPinSelectText"; text: i18nc("@action", "Select text on image"); onTriggered: { root.cancelArea(); pinnedWindow.selectTextOnImage() } }
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
            interactive: !root.selectingArea && !pinnedWindow.textSelectionActive
            activeFocusOnTab: true
            Rectangle {
                parent: viewport
                anchors.fill: parent
                z: 3
                visible: pinnedWindow.textSelectionActive && viewport.activeFocus
                color: "transparent"
                border.color: Kos.AppTheme.accent
                border.width: 1
            }
            Accessible.role: Accessible.Canvas
            Accessible.name: pinnedWindow.textSelectionActive ? i18nc("@info", "Recognized image text") : i18nc("@info", "Pinned image")
            Accessible.description: !pinnedWindow.textSelectionActive ? i18nc("@info", "Use zoom shortcuts or scroll to read the image.") : pinnedWindow.selectedText.length ? pinnedWindow.selectedText : i18nc("@info", "Use arrow keys to select a word, Shift to extend, and Ctrl+C to copy text.")
            Keys.onPressed: event => {
                if (!pinnedWindow.textSelectionActive || pinnedWindow.extractingText) return
                if (wordSurface.dragActive) return
                const extend = (event.modifiers & Qt.ShiftModifier) !== 0
                if (event.key === Qt.Key_Left || event.key === Qt.Key_Up) pinnedWindow.moveTextSelection(-1, extend)
                else if (event.key === Qt.Key_Right || event.key === Qt.Key_Down) pinnedWindow.moveTextSelection(1, extend)
                else if (event.key === Qt.Key_Home) pinnedWindow.moveTextSelection(-2, extend)
                else if (event.key === Qt.Key_End) pinnedWindow.moveTextSelection(2, extend)
                else if (event.key === Qt.Key_A && (event.modifiers & Qt.ControlModifier)) pinnedWindow.selectAllText()
                else return
                event.accepted = true
            }
            function revealActiveWord() {
                const index = pinnedWindow.activeWord
                const words = pinnedWindow.textWords
                if (!pinnedWindow.textSelectionActive || index < 0 || index >= words.length) return
                const box = words[index].bounds
                const left = snapshot.x + box.x * imageScale
                const top = snapshot.y + box.y * imageScale
                const right = left + box.width * imageScale
                const bottom = top + box.height * imageScale
                if (right - left > width) contentX = (left + right - width) / 2
                else if (left < contentX) contentX = left
                else if (right > contentX + width) contentX = right - width
                if (bottom - top > height) contentY = (top + bottom - height) / 2
                else if (top < contentY) contentY = top
                else if (bottom > contentY + height) contentY = bottom - height
                clampPosition()
            }
            Connections {
                target: pinnedWindow
                function onTextWordsChanged() { if (pinnedWindow.textSelectionActive && pinnedWindow.textWords.length) viewport.forceActiveFocus() }
                function onTextSelectionChanged() { if (!wordSurface.dragActive) viewport.revealActiveWord() }
            }
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
                Repeater {
                    model: pinnedWindow.textWords
                    Rectangle {
                        required property var modelData
                        required property int index
                        readonly property bool selectedWord: index >= pinnedWindow.selectionFirst && index <= pinnedWindow.selectionLast && pinnedWindow.selectionFirst >= 0
                        x: modelData.bounds.x * viewport.imageScale
                        y: modelData.bounds.y * viewport.imageScale
                        width: modelData.bounds.width * viewport.imageScale
                        height: modelData.bounds.height * viewport.imageScale
                        color: selectedWord ? "#343b82f6" : "transparent"
                        border.width: selectedWord ? 1 : 0
                        border.color: Kos.AppTheme.accent
                        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Kos.AppTheme.accent; opacity: 0.45; visible: !parent.selectedWord }
                    }
                }
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
            // Fixed-viewport pointer coordinates remain valid while the image scrolls.
            MouseArea {
                id: wordSurface
                objectName: "nkshotPinWordSurface"
                parent: viewport
                anchors.fill: parent
                enabled: pinnedWindow.textSelectionActive && !pinnedWindow.extractingText
                cursorShape: Qt.IBeamCursor
                preventStealing: true
                property bool dragActive: false
                property point pointer: Qt.point(0, 0)
                readonly property real velocityX: edgeSpeed(pointer.x, viewport.width)
                readonly property real velocityY: edgeSpeed(pointer.y, viewport.height)
                readonly property bool canScroll: (velocityX < 0 && viewport.contentX > 0)
                    || (velocityX > 0 && viewport.contentX < viewport.contentWidth - viewport.width)
                    || (velocityY < 0 && viewport.contentY > 0)
                    || (velocityY > 0 && viewport.contentY < viewport.contentHeight - viewport.height)
                function stop() { dragActive = false }
                function updateEndpoint(nearest) {
                    pinnedWindow.selectWordAt((viewport.contentX + Math.max(0, Math.min(viewport.width, pointer.x)) - snapshot.x) / viewport.imageScale,
                                              (viewport.contentY + Math.max(0, Math.min(viewport.height, pointer.y)) - snapshot.y) / viewport.imageScale,
                                              true, nearest)
                }
                function edgeSpeed(position, extent) {
                    const band = Math.min(36, extent / 3)
                    if (position < band) return -480 * Math.min(1, (band - position) / band)
                    if (position > extent - band) return 480 * Math.min(1, (position - extent + band) / band)
                    return 0
                }
                onPressed: mouse => {
                    viewport.forceActiveFocus()
                    pointer = Qt.point(mouse.x, mouse.y)
                    // Suppress keyboard-style reveal for the pointer's exact initial hit.
                    dragActive = true
                    pinnedWindow.selectWordAt((viewport.contentX + mouse.x - snapshot.x) / viewport.imageScale,
                                              (viewport.contentY + mouse.y - snapshot.y) / viewport.imageScale,
                                              (mouse.modifiers & Qt.ShiftModifier) !== 0)
                    dragActive = pinnedWindow.activeWord >= 0
                }
                onPositionChanged: mouse => {
                    pointer = Qt.point(mouse.x, mouse.y)
                    if (pressed && dragActive) updateEndpoint(true)
                }
                onReleased: mouse => {
                    pointer = Qt.point(mouse.x, mouse.y)
                    if (dragActive) updateEndpoint(true)
                    stop()
                }
                onCanceled: stop()
                onEnabledChanged: if (!enabled) stop()
                Timer {
                    interval: 16
                    repeat: true
                    running: wordSurface.dragActive && wordSurface.pressed && wordSurface.enabled && wordSurface.canScroll
                    property double lastTick: 0
                    onRunningChanged: lastTick = Date.now()
                    onTriggered: {
                        const now = Date.now()
                        const elapsed = Math.max(0, Math.min(50, now - lastTick)) / 1000
                        lastTick = now
                        viewport.contentX += wordSurface.velocityX * elapsed
                        viewport.contentY += wordSurface.velocityY * elapsed
                        viewport.clampPosition()
                        wordSurface.updateEndpoint(true)
                    }
                }
            }
            QQC.ScrollBar.vertical: QQC.ScrollBar {policy: viewport.contentHeight > viewport.height ? QQC.ScrollBar.AsNeeded : QQC.ScrollBar.AlwaysOff}
            QQC.ScrollBar.horizontal: QQC.ScrollBar {policy: viewport.contentWidth > viewport.width ? QQC.ScrollBar.AsNeeded : QQC.ScrollBar.AlwaysOff}
            WheelHandler {
                target: null
                enabled: pinnedWindow.textSelectionActive
                acceptedModifiers: Qt.NoModifier
                onWheel: event => {
                    viewport.contentX -= event.pixelDelta.x !== 0 ? event.pixelDelta.x : event.angleDelta.x / 3
                    viewport.contentY -= event.pixelDelta.y !== 0 ? event.pixelDelta.y : event.angleDelta.y / 3
                    viewport.clampPosition()
                    event.accepted = true
                }
            }
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
        ColumnLayout {
            visible: pinnedWindow.textSelectionActive
            Layout.fillWidth: true
            Layout.minimumHeight: 48
            Layout.maximumHeight: 48
            spacing: 4
            QQC.Label {
                objectName: "nkshotPinSelectedText"
                Layout.fillWidth: true
                text: pinnedWindow.extractingText ? i18nc("@info", "Reading text locally…")
                    : pinnedWindow.selectedText.length ? pinnedWindow.selectedText.replace(/\n/g, " · ")
                    : i18nc("@info", "Click or drag across words · Shift extends")
                textFormat: Text.PlainText
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                font.pixelSize: 11
                color: Kos.AppTheme.paletteText
            }
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                PinButton { objectName: "nkshotPinEndText"; implicitHeight: 28; text: pinnedWindow.extractingText ? i18nc("@action", "Cancel") : i18nc("@action", "Done"); onClicked: pinnedWindow.cancelTextSelection() }
                PinButton { objectName: "nkshotPinCopyText"; implicitHeight: 28; text: i18nc("@action", "Copy text"); selected: true; enabled: pinnedWindow.selectedText.length > 0; onClicked: pinnedWindow.copySelectedText() }
            }
        }
        RowLayout {
            visible: !root.selectingArea && !pinnedWindow.textSelectionActive
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
