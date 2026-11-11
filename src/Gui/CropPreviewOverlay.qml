// SPDX-License-Identifier: GPL-3.0-only
import QtQuick

// Reusable crop guide/controller. Geometry is injected; edits are explicit signals.
Item {
    id: root
    required property size sourceSize
    required property rect cropRect
    required property rect displayRect
    property bool interactive: false
    property bool freeResize: false
    property int gestureRevision: 0
    onInteractiveChanged: ++gestureRevision
    onFreeResizeChanged: ++gestureRevision
    onSourceSizeChanged: ++gestureRevision
    onDisplayRectChanged: ++gestureRevision
    onVisibleChanged: ++gestureRevision
    onActiveFocusChanged: { if (!activeFocus) ++gestureRevision }
    Connections {
        target: root.Window.window
        function onActiveChanged() { if (!root.Window.window.active) ++root.gestureRevision }
    }
    signal rectRequested(int x, int y, int width, int height)
    function even(value) { return Math.round(value / 2) * 2 }
    function resizeCorner(rect, leftSide, topSide, dx, dy) {
        const right = rect.x + rect.width, bottom = rect.y + rect.height
        const maxRight = Math.floor(sourceSize.width / 2) * 2
        const maxBottom = Math.floor(sourceSize.height / 2) * 2
        const x = leftSide ? Math.max(0, Math.min(right - 2, even(rect.x + dx))) : rect.x
        const y = topSide ? Math.max(0, Math.min(bottom - 2, even(rect.y + dy))) : rect.y
        const r = leftSide ? right : Math.max(x + 2, Math.min(maxRight, even(right + dx)))
        const b = topSide ? bottom : Math.max(y + 2, Math.min(maxBottom, even(bottom + dy)))
        rectRequested(x, y, r - x, b - y)
    }
    signal positionRequested(int x, int y)
    signal centerRequested()
    activeFocusOnTab: interactive
    Accessible.name: freeResize
        ? qsTr("Video crop. Drag corners to resize. Arrow keys move; Control and arrows resize. Home centers.")
        : qsTr("Video crop position. Drag or use arrow keys. Home centers.")
    Keys.onPressed: event => {
        if (!root.interactive) return
        const step = event.modifiers & Qt.ShiftModifier ? 10 : 2
        let x = root.cropRect.x, y = root.cropRect.y
        if (event.key === Qt.Key_Home) { root.centerRequested(); event.accepted = true; return }
        if (event.key === Qt.Key_Left) x -= step
        else if (event.key === Qt.Key_Right) x += step
        else if (event.key === Qt.Key_Up) y -= step
        else if (event.key === Qt.Key_Down) y += step
        else return
        if (root.freeResize && (event.modifiers & Qt.ControlModifier))
            root.resizeCorner(root.cropRect, false, false, x - root.cropRect.x, y - root.cropRect.y)
        else root.positionRequested(x, y)
        event.accepted = true
    }
    Item {
        id: imageArea
        x: root.displayRect.x
        y: root.displayRect.y
        width: Math.max(0, root.displayRect.width)
        height: Math.max(0, root.displayRect.height)
        readonly property real cropLeft: root.sourceSize.width > 0 ? root.cropRect.x / root.sourceSize.width * width : 0
        readonly property real cropTop: root.sourceSize.height > 0 ? root.cropRect.y / root.sourceSize.height * height : 0
        readonly property real cropWidth: root.sourceSize.width > 0 ? root.cropRect.width / root.sourceSize.width * width : width
        readonly property real cropHeight: root.sourceSize.height > 0 ? root.cropRect.height / root.sourceSize.height * height : height
        Rectangle {x: 0; y: 0; width: imageArea.width; height: imageArea.cropTop; color: "#99000000"}
        Rectangle {x: 0; y: imageArea.cropTop+imageArea.cropHeight; width: imageArea.width; height: Math.max(0,imageArea.height-y); color: "#99000000"}
        Rectangle {x: 0; y: imageArea.cropTop; width: imageArea.cropLeft; height: imageArea.cropHeight; color: "#99000000"}
        Rectangle {x: imageArea.cropLeft+imageArea.cropWidth; y: imageArea.cropTop; width: Math.max(0,imageArea.width-x); height: imageArea.cropHeight; color: "#99000000"}
        Rectangle {
            id: frame
            objectName: "nkshotVideoCropFrame"
            x: imageArea.cropLeft; y: imageArea.cropTop
            width: imageArea.cropWidth; height: imageArea.cropHeight
            color: "transparent"; border.width: root.activeFocus ? 2 : 1; border.color: "white"
            MouseArea {
                anchors.fill: parent
                enabled: root.interactive
                cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                property point startPointer
                property rect startCrop
                property int revision: -1
                onPressed: mouse => {
                    root.forceActiveFocus(Qt.MouseFocusReason)
                    startPointer = mapToItem(imageArea, mouse.x, mouse.y)
                    startCrop = root.cropRect
                    revision = root.gestureRevision
                }
                onPositionChanged: mouse => {
                    if (!pressed || revision !== root.gestureRevision || imageArea.width <= 0 || imageArea.height <= 0) return
                    const p = mapToItem(imageArea, mouse.x, mouse.y)
                    root.positionRequested(startCrop.x + (p.x-startPointer.x)*root.sourceSize.width/imageArea.width,
                                           startCrop.y + (p.y-startPointer.y)*root.sourceSize.height/imageArea.height)
                }
                onCanceled: revision = -1
                onReleased: revision = -1
            }
            Repeater {
                model: 4
                delegate: Item {
                    id: handle
                    required property int index
                    objectName: "nkshotVideoCropHandle_" + index
                    readonly property bool leftSide: index % 2 === 0
                    readonly property bool topSide: index < 2
                    // Keep the full hit target inside the displayed image, even at its edges.
                    x: Math.max(-frame.x, Math.min(imageArea.width - frame.x - width,
                        (leftSide ? 0 : frame.width) - width / 2))
                    y: Math.max(-frame.y, Math.min(imageArea.height - frame.y - height,
                        (topSide ? 0 : frame.height) - height / 2))
                    width: 24; height: 24
                    visible: root.freeResize && root.interactive
                    Rectangle {
                        x: Math.max(0, Math.min(handle.width - width, (handle.leftSide ? 0 : frame.width) - handle.x - width / 2))
                        y: Math.max(0, Math.min(handle.height - height, (handle.topSide ? 0 : frame.height) - handle.y - height / 2))
                        width: 9; height: 9; radius: 2
                        color: "white"; border.color: "#606060"
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: handle.visible && root.interactive
                        cursorShape: handle.leftSide === handle.topSide ? Qt.SizeFDiagCursor : Qt.SizeBDiagCursor
                        property point startPointer
                        property rect startCrop
                        property int revision: -1
                        onPressed: mouse => {
                            root.forceActiveFocus(Qt.MouseFocusReason)
                            startPointer = mapToItem(imageArea, mouse.x, mouse.y)
                            startCrop = root.cropRect
                            revision = root.gestureRevision
                        }
                        onPositionChanged: mouse => {
                            if (!pressed || revision !== root.gestureRevision || imageArea.width <= 0 || imageArea.height <= 0) return
                            const p = mapToItem(imageArea, mouse.x, mouse.y)
                            root.resizeCorner(startCrop, handle.leftSide, handle.topSide,
                                (p.x - startPointer.x) * root.sourceSize.width / imageArea.width,
                                (p.y - startPointer.y) * root.sourceSize.height / imageArea.height)
                        }
                        onCanceled: revision = -1
                        onReleased: revision = -1
                    }
                }
            }
        }
    }
}
