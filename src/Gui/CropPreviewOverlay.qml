// SPDX-License-Identifier: GPL-3.0-only
import QtQuick

// Display-only crop guide. All geometry is injected; no player/document mutation.
Item {
    id: root
    required property size sourceSize
    required property rect cropRect
    required property rect displayRect
    Accessible.name: qsTr("Centered export crop")
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
            color: "transparent"; border.width: 1; border.color: "white"
        }
    }
}
