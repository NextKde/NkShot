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
        Keys.onReturnPressed: if (enabled) clicked()
        Keys.onEnterPressed: if (enabled) clicked()
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
        color: root.phase === 1 ? "transparent" : Kos.AppTheme.contentBackground
    }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 20
        visible: root.phase !== 1
        spacing: 12
        RowLayout {
            QQC.Label { text: i18nc("@title", "Manual scrolling capture"); font.pixelSize: 18; font.bold: true; color: Kos.AppTheme.paletteText }
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
        focus: visible
        activeFocusOnTab: true
        Accessible.role: Accessible.Pane
        Accessible.name: i18nc("@label", "Capture region selector")
        Accessible.description: i18nc("@info", "Position %1, %2. Size %3 by %4. Arrows move; Alt and arrows resize; Shift adjusts faster. Enter confirms; Escape cancels.", Math.round(selectionRect.x*scrollWindow.selectionScale), Math.round(selectionRect.y*scrollWindow.selectionScale), pixelWidth, pixelHeight)
        onVisibleChanged: if (visible) forceActiveFocus()
        Component.onCompleted: if (visible) forceActiveFocus()
        readonly property rect selectionRect: Qt.rect(Math.min(origin.x,end.x), Math.min(origin.y,end.y), Math.abs(end.x-origin.x), Math.abs(end.y-origin.y))
        readonly property int pixelWidth: Math.round(Math.max(origin.x,end.x)*scrollWindow.selectionScale)-Math.round(Math.min(origin.x,end.x)*scrollWindow.selectionScale)
        readonly property int pixelHeight: Math.round(Math.max(origin.y,end.y)*scrollWindow.selectionScale)-Math.round(Math.min(origin.y,end.y)*scrollWindow.selectionScale)
        readonly property bool regionValid: pixelWidth>0 && pixelHeight>24 && pixelWidth<=8192 && pixelHeight<=32000 && pixelWidth*pixelHeight<=40000000
        function adjust(dx,dy,resizeRegion,fast) {
            if (selectionRect.width<=0 || selectionRect.height<=0) {
                origin=Qt.point(Math.floor(width/4),Math.floor(height/4))
                end=Qt.point(Math.floor(width*3/4),Math.floor(height*3/4))
            }
            let x=selectionRect.x, y=selectionRect.y
            let w=selectionRect.width, h=selectionRect.height
            let step=fast?10:1
            if(resizeRegion) {
                w=Math.max(Math.min(width-x,Math.ceil(1/scrollWindow.selectionScale)),Math.min(width-x,w+dx*step))
                h=Math.max(Math.min(height-y,Math.ceil(25/scrollWindow.selectionScale)),Math.min(height-y,h+dy*step))
            } else {
                x=Math.max(0,Math.min(width-w,x+dx*step))
                y=Math.max(0,Math.min(height-h,y+dy*step))
            }
            origin=Qt.point(x,y);end=Qt.point(x+w,y+h)
        }
        function confirm() {if(regionValid) scrollWindow.acceptRegion(selectionRect)}
        Keys.onPressed: event => {
            if(!selector.activeFocus) return
            if(event.key===Qt.Key_Return || event.key===Qt.Key_Enter) {confirm();event.accepted=true;return}
            let dx=event.key===Qt.Key_Left?-1:event.key===Qt.Key_Right?1:0
            let dy=event.key===Qt.Key_Up?-1:event.key===Qt.Key_Down?1:0
            if(dx || dy) {adjust(dx,dy,(event.modifiers&Qt.AltModifier)!==0,(event.modifiers&Qt.ShiftModifier)!==0);event.accepted=true}
        }
        property point origin
        property point end
        property bool drawing: false
        onPressed: event => { forceActiveFocus(); origin = Qt.point(event.x, event.y); end = origin; drawing = true }
        onPositionChanged: event => { if (drawing) end = Qt.point(Math.max(0, Math.min(width, event.x)), Math.max(0, Math.min(height, event.y))) }
        onReleased: drawing = false
        onCanceled: drawing = false
        // Dim only outside the chosen region, leaving its real desktop pixels
        // unobscured. This is a transparent overlay, not a captured image preview.
        Rectangle {anchors.fill: parent; color: "#66000000"; visible: selector.selectionRect.width<=0 || selector.selectionRect.height<=0}
        Item {
            anchors.fill: parent
            visible: selector.selectionRect.width>0 && selector.selectionRect.height>0
            Rectangle {x: 0; y: 0; width: parent.width; height: selector.selectionRect.y; color: "#66000000"}
            Rectangle {x: 0; y: selector.selectionRect.y+selector.selectionRect.height; width: parent.width; height: parent.height-y; color: "#66000000"}
            Rectangle {x: 0; y: selector.selectionRect.y; width: selector.selectionRect.x; height: selector.selectionRect.height; color: "#66000000"}
            Rectangle {x: selector.selectionRect.x+selector.selectionRect.width; y: selector.selectionRect.y; width: parent.width-x; height: selector.selectionRect.height; color: "#66000000"}
        }
        Rectangle {
            id: selection
            x: Math.min(selector.origin.x, selector.end.x); y: Math.min(selector.origin.y, selector.end.y)
            width: Math.abs(selector.end.x - selector.origin.x); height: Math.abs(selector.end.y - selector.origin.y)
            color: "transparent"; border.color: "#ffffff"; border.width: 2
            visible: width > 0 && height > 0
        }
        Kos.KosSurface {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom; anchors.bottomMargin: 24
            width: Math.min(parent.width-32,520); height: 64
            radius: 14; fillColor: Kos.AppTheme.contentBackground; elevation: 0.5
            MouseArea {anchors.fill: parent}
            RowLayout {
                anchors.fill: parent; anchors.margins: 14; spacing: 10
                ActionButton {objectName: "nkshotScrollSelectorCancel"; text: i18nc("@action", "Cancel"); onClicked: scrollWindow.cancelPreview()}
                QQC.Label {
                    Layout.fillWidth: true; Layout.minimumWidth: 0
                    text: selector.pixelWidth>0 && selector.pixelHeight>0 ? selector.pixelWidth + " × " + selector.pixelHeight : i18nc("@info", "Select a region")
                    elide: Text.ElideRight; horizontalAlignment: Text.AlignHCenter
                    color: Kos.AppTheme.mutedText
                    Accessible.name: i18nc("@label", "Selected region size")
                }
                ActionButton {
                    objectName: "nkshotScrollConfirmRegion"
                    text: i18nc("@action", "Use region")
                    primary: true; enabled: selector.regionValid
                    onClicked: selector.confirm()
                    QQC.ToolTip.visible: hovered
                    QQC.ToolTip.text: i18nc("@info", "Enter confirms; Escape cancels")
                }
            }
        }
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter; y: 24
            width: Math.min(parent.width - 32, 460); height: hint.implicitHeight + 24
            radius: 12; color: "#ee202126"
            QQC.Label {
                id: hint
                anchors.centerIn: parent; width: parent.width - 28
                text: selector.pixelWidth>0 && selector.pixelHeight>0 && !selector.regionValid
                    ? i18nc("@info", "Use a region at least 25 px tall, no wider than 8192 px and no more than 40 MP.")
                    : scrollWindow.phase === 1 ? scrollWindow.statusText : i18nc("@info", "Drag a region, or use arrows to move.\nAlt + arrows resize; Shift adjusts faster.")
                wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter; color: "white"
            }
        }
    }
}
