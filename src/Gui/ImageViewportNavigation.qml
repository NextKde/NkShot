// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Window

// Shared view-only navigation. The owner applies zoomRequested synchronously;
// this component never edits an image document or changes its annotation tool.
Item {
    id: root
    required property var viewport
    required property Item imageItem
    required property real zoom
    required property real fitZoom
    property bool fitMode: false
    property real maximumZoom: 4
    readonly property real minimumZoom: Math.min(fitZoom, 1)
    readonly property Item targetItem: imageItem
    readonly property real currentZoom: zoom
    readonly property bool panActive: pointer.dragging && enabled
    readonly property bool panning: panActive
    readonly property bool textInputFocused: {
        const item = Window.window ? Window.window.activeFocusItem : null
        return item !== null && (typeof item.cursorPosition !== "undefined" || typeof item.inputMethodComposing !== "undefined")
    }
    property int revision: 0
    property var pendingAnchor: null
    signal zoomRequested(real value)
    signal fitRequested()
    function clamp() {
        if (!enabled) return
        viewport.contentX = Math.max(0, Math.min(viewport.contentX, Math.max(0, viewport.contentWidth - viewport.width)))
        viewport.contentY = Math.max(0, Math.min(viewport.contentY, Math.max(0, viewport.contentHeight - viewport.height)))
    }
    function settlePending() {
        if (!enabled || pendingAnchor === null) return
        const pending = pendingAnchor
        pendingAnchor = null
        ++revision
        const current = imageItem.mapToItem(viewport, pending.source.x, pending.source.y)
        viewport.contentX += current.x - pending.point.x
        viewport.contentY += current.y - pending.point.y
        clamp()
    }
    function zoomTo(value, anchor) {
        if (!enabled) return
        // A burst can deliver another wheel before the previous callLater.
        // Settle its scale/scroll pair before measuring the next document point.
        settlePending()
        const point = anchor === undefined ? Qt.point(viewport.width / 2, viewport.height / 2) : anchor
        const source = imageItem.mapFromItem(viewport, point.x, point.y)
        const ticket = ++revision
        zoomRequested(Math.max(minimumZoom, Math.min(maximumZoom, value)))
        pendingAnchor = { source: source, point: point }
        Qt.callLater(function() {
            if (root.enabled && ticket === root.revision) root.settlePending()
        })
    }
    function step(factor, anchor) { zoomTo(zoom * factor, anchor) }
    function fit() {
        if (!enabled) return
        settlePending()
        ++revision
        fitRequested()
        viewport.contentX = 0
        viewport.contentY = 0
        Qt.callLater(clamp)
    }
    function cancelPan() { pointer.dragging = false; settlePending() }
    onEnabledChanged: {
        if (!enabled) cancelPan()
        else Qt.callLater(function() { root.settlePending(); root.clamp() })
    }
    onVisibleChanged: if (!visible) cancelPan()
    onFitZoomChanged: {
        if (enabled && !fitMode && zoom < minimumZoom) zoomTo(minimumZoom)
        Qt.callLater(clamp)
    }
    Connections {
        target: root.Window.window
        function onActiveChanged() { if (!root.Window.window.active) root.cancelPan() }
        function onVisibleChanged() { if (!root.Window.window.visible) root.cancelPan() }
    }
    Connections {
        target: root.viewport
        function onWidthChanged() { Qt.callLater(root.clamp) }
        function onHeightChanged() { Qt.callLater(root.clamp) }
        function onContentWidthChanged() { Qt.callLater(root.clamp) }
        function onContentHeightChanged() { Qt.callLater(root.clamp) }
    }
    MouseArea {
        id: pointer
        objectName: "nkshotImageNavigationPointer"
        anchors.fill: parent
        acceptedButtons: Qt.MiddleButton
        preventStealing: true
        property bool dragging: false
        property point previous
        cursorShape: dragging ? Qt.ClosedHandCursor : Qt.ArrowCursor
        onPressed: mouse => {
            root.settlePending()
            root.viewport.cancelFlick()
            previous = Qt.point(mouse.x, mouse.y)
            dragging = true
        }
        onPositionChanged: mouse => {
            if (!dragging || !pressed) return
            root.viewport.contentX -= mouse.x - previous.x
            root.viewport.contentY -= mouse.y - previous.y
            previous = Qt.point(mouse.x, mouse.y)
            root.clamp()
        }
        onReleased: dragging = false
        onCanceled: dragging = false
        onWheel: wheel => {
            root.settlePending()
            root.viewport.cancelFlick()
            if (wheel.modifiers & Qt.ControlModifier) {
                const delta = wheel.angleDelta.y || wheel.angleDelta.x || wheel.pixelDelta.y || wheel.pixelDelta.x
                if (delta) root.step(Math.pow(1.25, delta / 120), Qt.point(wheel.x, wheel.y))
            } else {
                const dx = wheel.pixelDelta.x || wheel.angleDelta.x / 120 * 60
                const dy = wheel.pixelDelta.y || wheel.angleDelta.y / 120 * 60
                if (wheel.modifiers & Qt.ShiftModifier) root.viewport.contentX -= dy || dx
                else { root.viewport.contentX -= dx; root.viewport.contentY -= dy }
                root.clamp()
            }
            wheel.accepted = true
        }
    }
    Shortcut { enabled: root.enabled && root.visible && !root.textInputFocused; sequence: "Ctrl+0"; onActivated: root.fit() }
    Shortcut { enabled: root.enabled && root.visible && !root.textInputFocused; sequence: "Ctrl+1"; onActivated: root.zoomTo(1) }
    Shortcut { enabled: root.enabled && root.visible && !root.textInputFocused; sequences: [StandardKey.ZoomIn]; onActivated: root.step(1.25) }
    Shortcut { enabled: root.enabled && root.visible && !root.textInputFocused; sequences: [StandardKey.ZoomOut]; onActivated: root.step(1 / 1.25) }
}
