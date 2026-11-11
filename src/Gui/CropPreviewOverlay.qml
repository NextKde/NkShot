// SPDX-License-Identifier: GPL-3.0-only
import QtQuick

// Reusable crop guide/controller. Geometry is injected; edits are explicit signals.
Item {
    id: root
    required property size sourceSize
    required property rect cropRect
    required property rect displayRect
    property bool interactive: false
    signal positionRequested(int x, int y)
    signal centerRequested()
    activeFocusOnTab: interactive
    Accessible.name: qsTr("Video crop position. Drag or use arrow keys. Home centers.")
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
        root.positionRequested(x, y)
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
                onPressed: mouse => {
                    root.forceActiveFocus(Qt.MouseFocusReason)
                    startPointer = mapToItem(imageArea, mouse.x, mouse.y)
                    startCrop = root.cropRect
                }
                onPositionChanged: mouse => {
                    if (!pressed || imageArea.width <= 0 || imageArea.height <= 0) return
                    const p = mapToItem(imageArea, mouse.x, mouse.y)
                    root.positionRequested(startCrop.x + (p.x-startPointer.x)*root.sourceSize.width/imageArea.width,
                                           startCrop.y + (p.y-startPointer.y)*root.sourceSize.height/imageArea.height)
                }
            }
        }
    }
}
