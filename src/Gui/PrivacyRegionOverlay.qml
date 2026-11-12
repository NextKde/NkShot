// SPDX-License-Identifier: GPL-3.0-only
import QtQuick

// Shared source-coordinate region controller; no media/export ownership.
Item {
    id: root
    required property size sourceSize
    required property rect region
    required property rect displayRect
    property bool interactive: false
    property int regionIndex: 0
    property int gestureRevision: 0
    onRegionIndexChanged: ++gestureRevision
    onInteractiveChanged: ++gestureRevision
    onVisibleChanged: ++gestureRevision
    onSourceSizeChanged: ++gestureRevision
    onDisplayRectChanged: ++gestureRevision
    onActiveFocusChanged: { if (!activeFocus) ++gestureRevision }
    Connections {
        target: root.Window.window
        function onActiveChanged() { if (!root.Window.window.active) ++root.gestureRevision }
    }
    signal rectRequested(int x, int y, int width, int height)
    function requestMove(x, y, width, height) {
        rectRequested(Math.max(0,Math.min(sourceSize.width-width,x)),
            Math.max(0,Math.min(sourceSize.height-height,y)),width,height)
    }
    activeFocusOnTab: interactive
    Accessible.name: qsTr("Privacy cover. Drag to move, drag corners to resize. Arrow keys move; Control and arrows resize.")
    Keys.onPressed: event => {
        if (!interactive) return
        const step = event.modifiers & Qt.ShiftModifier ? 10 : 2
        let dx = 0, dy = 0
        if (event.key === Qt.Key_Left) dx = -step
        else if (event.key === Qt.Key_Right) dx = step
        else if (event.key === Qt.Key_Up) dy = -step
        else if (event.key === Qt.Key_Down) dy = step
        else return
        const resize = event.modifiers & Qt.ControlModifier
        if(resize) rectRequested(region.x,region.y,region.width+dx,region.height+dy)
        else requestMove(region.x+dx,region.y+dy,region.width,region.height)
        event.accepted = true
    }
    Item {
        id: imageArea
        x: root.displayRect.x; y: root.displayRect.y
        width: Math.max(0, root.displayRect.width); height: Math.max(0, root.displayRect.height)
        clip: true
        Rectangle {
            id: cover
            objectName: "nkshotVideoPrivacyFrame"
            x: root.sourceSize.width > 0 ? root.region.x / root.sourceSize.width * imageArea.width : 0
            y: root.sourceSize.height > 0 ? root.region.y / root.sourceSize.height * imageArea.height : 0
            width: root.sourceSize.width > 0 ? root.region.width / root.sourceSize.width * imageArea.width : 0
            height: root.sourceSize.height > 0 ? root.region.height / root.sourceSize.height * imageArea.height : 0
            color: "black"
            border.width: root.interactive ? 1 : 0
            border.color: "white"
            MouseArea {
                anchors.fill: parent
                enabled: root.interactive
                cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                property point startPointer
                property rect startRegion
                property int revision: -1
                onPressed: mouse => {
                    root.forceActiveFocus(Qt.MouseFocusReason)
                    startPointer = mapToItem(imageArea, mouse.x, mouse.y); startRegion = root.region; revision = root.gestureRevision
                }
                onPositionChanged: mouse => {
                    if (!pressed || revision !== root.gestureRevision || imageArea.width <= 0 || imageArea.height <= 0) return
                    const p = mapToItem(imageArea, mouse.x, mouse.y)
                    root.requestMove(startRegion.x + (p.x-startPointer.x)*root.sourceSize.width/imageArea.width,
                        startRegion.y + (p.y-startPointer.y)*root.sourceSize.height/imageArea.height,
                        startRegion.width, startRegion.height)
                }
            }
            Repeater {
                model: 4
                delegate: Item {
                    id: handle
                    required property int index
                    objectName: "nkshotVideoPrivacyHandle_" + index
                    readonly property bool leftSide: index % 2 === 0
                    readonly property bool topSide: index < 2
                    x: (leftSide ? 0 : cover.width) - width/2
                    y: (topSide ? 0 : cover.height) - height/2
                    // Keep corner targets out of the center, so tiny covers remain draggable.
                    // The visible glyph stays 8 px; only its hit area shrinks.
                    width: Math.max(0, Math.min(24, cover.width / 2))
                    height: Math.max(0, Math.min(24, cover.height / 2))
                    visible: root.interactive
                    Rectangle {anchors.centerIn: parent; width: 8; height: 8; radius: 2; color: "white"; border.color: "#606060"}
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: handle.leftSide === handle.topSide ? Qt.SizeFDiagCursor : Qt.SizeBDiagCursor
                        property point startPointer
                        property rect startRegion
                        property int revision: -1
                        onPressed: mouse => {
                            root.forceActiveFocus(Qt.MouseFocusReason)
                            startPointer=mapToItem(imageArea,mouse.x,mouse.y); startRegion=root.region; revision=root.gestureRevision
                        }
                        onPositionChanged: mouse => {
                            if (!pressed || revision !== root.gestureRevision || imageArea.width <= 0 || imageArea.height <= 0) return
                            const p=mapToItem(imageArea,mouse.x,mouse.y)
                            const dx=(p.x-startPointer.x)*root.sourceSize.width/imageArea.width
                            const dy=(p.y-startPointer.y)*root.sourceSize.height/imageArea.height
                            const right=startRegion.x+startRegion.width, bottom=startRegion.y+startRegion.height
                            const x=handle.leftSide ? Math.max(0,Math.min(right-2,startRegion.x+dx)) : startRegion.x
                            const y=handle.topSide ? Math.max(0,Math.min(bottom-2,startRegion.y+dy)) : startRegion.y
                            root.rectRequested(x,y,handle.leftSide ? right-x : startRegion.width+dx,
                                handle.topSide ? bottom-y : startRegion.height+dy)
                        }
                    }
                }
            }
        }
    }
}
