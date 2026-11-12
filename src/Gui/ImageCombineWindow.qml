// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import Kos.Ui as Kos

QQC.Pane {
    id: root
    objectName: "nkshotCombineReview"
    readonly property var session: combineWindow.session
    readonly property bool compact: width < 650
    property string selectedId: ""
    property int selectedIndex: -1
    padding: 20
    font: Qt.application.font
    palette.text: Kos.AppTheme.paletteText
    palette.windowText: Kos.AppTheme.paletteText
    palette.buttonText: Kos.AppTheme.paletteText
    background: Rectangle {color: Kos.AppTheme.contentBackground}
    function select(index) {
        selectedIndex = index >= 0 && index < session.items.length ? index : -1
        selectedId = selectedIndex >= 0 ? String(session.items[selectedIndex].id) : ""
        files.currentIndex = selectedIndex
        if (selectedIndex >= 0) files.positionViewAtIndex(selectedIndex, ListView.Contain)
    }
    function restoreSelection() {
        for (let i = 0; i < session.items.length; ++i) {
            if (String(session.items[i].id) === selectedId) { select(i); return }
        }
        select(Math.min(Math.max(0, selectedIndex), session.items.length - 1))
    }
    function moveSelection(delta) {
        const target = selectedIndex + delta
        if (selectedIndex < 0 || target < 0 || target >= session.items.length) return
        session.move(selectedIndex, target)
        restoreSelection()
    }
    function removeSelection() {
        if (selectedIndex < 0) return
        session.remove(selectedIndex)
        restoreSelection()
    }
    Connections {
        target: root.session
        function onChanged() { root.restoreSelection(); Qt.callLater(root.restoreSelection) }
    }
    function revealFocus(item) {
        if (!item || !body.contentItem) return
        // Only scroll controls inside the body; the fixed footer must stay fixed.
        let ancestor = item
        while (ancestor && ancestor !== body.contentItem) ancestor = ancestor.parent
        if (!ancestor) return
        const viewport = body.contentItem
        const p = item.mapToItem(viewport, 0, 0)
        let y = viewport.contentY
        if (p.y < 2) y += p.y - 2
        else if (p.y + item.height > viewport.height - 2) y += p.y + item.height - viewport.height + 2
        viewport.contentY = Math.max(0, Math.min(Math.max(0, viewport.contentHeight - viewport.height), y))
    }
    Connections {
        target: combineWindow
        function onActiveFocusItemChanged() { root.revealFocus(combineWindow.activeFocusItem) }
    }
    component Action: QQC.Button {
        id: control
        implicitHeight: 36
        leftPadding: 12; rightPadding: 12
        font: root.font
        background: Kos.KosSurface {
            fillColor: Kos.AppTheme.controlBackground
            radius: 9; elevation: 0; strokeWidth: 0
            hovered: control.hovered; pressed: control.down; focused: control.visualFocus
        }
    }
    component Selector: QQC.ComboBox {
        id: control
        implicitHeight: 36
        font: root.font
        leftPadding: 12; rightPadding: 32
        background: Rectangle {
            radius: 9; color: Kos.AppTheme.controlBackground
            border.width: control.visualFocus ? 2 : 1
            border.color: control.visualFocus ? Kos.AppTheme.accent : Kos.AppTheme.border
        }
        contentItem: Text {
            text: control.displayText; font: control.font
            color: Kos.AppTheme.paletteText
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        indicator: Text {
            text: "⌄"; color: Kos.AppTheme.mutedText
            x: control.width - width - 12; anchors.verticalCenter: parent.verticalCenter
        }
        delegate: QQC.ItemDelegate {
            required property int index
            required property var modelData
            width: control.width - 8
            height: 36
            text: modelData
            font: control.font
            highlighted: control.highlightedIndex === index
            contentItem: Text {
                text: parent.text; font: parent.font
                color: Kos.AppTheme.paletteText
                verticalAlignment: Text.AlignVCenter
                leftPadding: 8
            }
            background: Rectangle {radius: 7; color: parent.highlighted ? Kos.AppTheme.controlBackground : "transparent"}
        }
        popup: QQC.Popup {
            y: control.height + 4
            width: control.width
            padding: 4
            implicitHeight: contentItem.implicitHeight + 8
            contentItem: ListView {
                implicitHeight: contentHeight
                model: control.popup.visible ? control.delegateModel : null
                currentIndex: control.highlightedIndex
                clip: true
            }
            background: Rectangle {
                color: Kos.AppTheme.contentBackground; radius: 10
                border.color: Kos.AppTheme.border; border.width: 1
            }
        }
    }
    Component.onCompleted: Qt.callLater(function() {
        files.forceActiveFocus(Qt.OtherFocusReason)
        if (body.contentItem) body.contentItem.contentY = 0
    })
    contentItem: ColumnLayout {
        spacing: 14
        QQC.Label {
            text: i18nc("@title", "Review your combination")
            font.pixelSize: 20; font.weight: Font.DemiBold
            color: Kos.AppTheme.paletteText
        }
        QQC.Label {
            Layout.fillWidth: true
            text: i18nc("@info", "Arrange images, then create a separate copy. Your files and current edit stay unchanged.")
            color: Kos.AppTheme.mutedText
            wrapMode: Text.Wrap
        }
        QQC.ScrollView {
            id: body
            objectName: "nkshotCombineReviewScroll"
            Layout.fillWidth: true; Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            GridLayout {
                width: body.availableWidth
                columns: root.compact ? 1 : 2
                columnSpacing: 20; rowSpacing: 16
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.preferredWidth: root.compact ? 1 : 300
                    Layout.alignment: Qt.AlignTop
                    spacing: 10
                    QQC.Label {
                        Layout.fillWidth: true
                        objectName: "nkshotCombineReviewImagesTitle"
                        Layout.minimumHeight: 24
                        text: root.session.direction === "horizontal" ? i18nc("@label", "Images · left to right") : i18nc("@label", "Images · top to bottom")
                        color: Kos.AppTheme.paletteText
                        font.weight: Font.DemiBold
                    }
                    ListView {
                        id: files
                        objectName: "nkshotCombineReviewFiles"
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.max(84, Math.min(root.compact ? 180 : 288, files.count * 84 - 6))
                        model: root.session.items
                        clip: true
                        spacing: 6
                        keyNavigationEnabled: false
                        activeFocusOnTab: true
                        QQC.ScrollBar.vertical: QQC.ScrollBar {width: 8}
                        QQC.Label {
                            anchors.centerIn: parent
                            visible: files.count === 0
                            text: root.session.busy ? i18nc("@info", "Loading images…") : i18nc("@info", "Choose images to start again.")
                            color: Kos.AppTheme.mutedText
                        }
                        Keys.onPressed: event => {
                            if (event.key === Qt.Key_Up || event.key === Qt.Key_Down) {
                                const delta = event.key === Qt.Key_Up ? -1 : 1
                                if (event.modifiers & Qt.AltModifier) root.moveSelection(delta)
                                else root.select(Math.max(0, Math.min(count - 1, root.selectedIndex + delta)))
                                event.accepted = true
                            }
                        }
                        delegate: QQC.ItemDelegate {
                            id: row
                            required property int index
                            required property var modelData
                            objectName: "nkshotCombineReviewItem_" + index
                            width: files.width - 12; height: 78
                            activeFocusOnTab: false
                            onClicked: {root.select(index); files.forceActiveFocus(Qt.MouseFocusReason)}
                            Accessible.name: i18nc("@label", "Image %1: %2", index + 1, modelData.name)
                            background: Rectangle {
                                radius: 10
                                color: root.selectedIndex === row.index || row.hovered ? Kos.AppTheme.controlBackground : "transparent"
                                border.width: 1
                                border.color: root.selectedIndex === row.index ? Kos.AppTheme.accent : Kos.AppTheme.border
                            }
                            contentItem: RowLayout {
                                spacing: 10
                                Image {
                                    objectName: "nkshotCombineReviewThumbnail_" + row.index
                                    Layout.preferredWidth: 70; Layout.preferredHeight: 58
                                    source: row.modelData.thumbnailUrl
                                    cache: false
                                    fillMode: Image.PreserveAspectFit
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true; spacing: 4
                                    QQC.Label {
                                        Layout.fillWidth: true
                                        text: row.modelData.name
                                        textFormat: Text.PlainText
                                        elide: Text.ElideMiddle
                                        color: Kos.AppTheme.paletteText
                                    }
                                    QQC.Label {
                                        text: i18nc("@info", "%1 · %2 × %3", row.index + 1, row.modelData.size.width, row.modelData.size.height)
                                        color: Kos.AppTheme.mutedText
                                    }
                                }
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Action {objectName: "nkshotCombineReviewUp"; text: i18nc("@action", "Up"); enabled: root.selectedIndex > 0; onClicked: root.moveSelection(-1)}
                        Action {objectName: "nkshotCombineReviewDown"; text: i18nc("@action", "Down"); enabled: root.selectedIndex >= 0 && root.selectedIndex < files.count - 1; onClicked: root.moveSelection(1)}
                        Item {Layout.fillWidth: true}
                        Action {objectName: "nkshotCombineReviewRemove"; text: i18nc("@action", "Remove"); enabled: root.selectedIndex >= 0; onClicked: root.removeSelection()}
                    }
                    QQC.Label {Layout.fillWidth: true; text: i18nc("@info", "Arrow keys select · Alt+arrows reorder"); color: Kos.AppTheme.mutedText; wrapMode: Text.Wrap}
                    Action {
                        objectName: "nkshotCombineReviewChoose"
                        Layout.fillWidth: true
                        text: i18nc("@action", "Choose images…")
                        enabled: !root.session.busy && !combineWindow.choosingImages
                        onClicked: combineWindow.chooseImages()
                        Accessible.description: i18nc("@info", "Replace the entire list. Cancel keeps the current images and order.")
                    }
                    QQC.Label {
                        Layout.fillWidth: true
                        text: i18nc("@info", "Choosing images replaces the whole list.")
                        color: Kos.AppTheme.mutedText
                        wrapMode: Text.Wrap
                    }
                    Selector {
                        objectName: "nkshotCombineReviewDirection"
                        Layout.fillWidth: true
                        model: [i18nc("@option", "Vertical"), i18nc("@option", "Horizontal")]
                        currentIndex: root.session.direction === "horizontal" ? 1 : 0
                        onActivated: root.session.direction = currentIndex === 1 ? "horizontal" : "vertical"
                        Accessible.name: i18nc("@label", "Combine direction")
                    }
                    Selector {
                        objectName: "nkshotCombineReviewMethod"
                        Layout.fillWidth: true
                        model: [i18nc("@option", "Keep all pixels"), i18nc("@option", "Match overlap")]
                        currentIndex: root.session.keepAll ? 0 : 1
                        onActivated: root.session.keepAll = currentIndex === 0
                        Accessible.name: i18nc("@label", "Join method")
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.preferredWidth: root.compact ? 1 : 440
                    Layout.alignment: Qt.AlignTop
                    spacing: 10
                    QQC.Label {objectName: "nkshotCombineReviewPreviewTitle"; Layout.minimumHeight: 24; text: i18nc("@label", "Combined preview"); color: Kos.AppTheme.paletteText; font.weight: Font.DemiBold}
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: root.compact ? 230 : 370
                        color: Kos.AppTheme.controlBackground
                        radius: 12
                        Image {
                            objectName: "nkshotCombineReviewPreview"
                            anchors.fill: parent; anchors.margins: 12
                            source: root.session.previewUrl
                            fillMode: Image.PreserveAspectFit
                            cache: false
                            visible: root.session.ready
                        }
                        QQC.BusyIndicator {anchors.centerIn: parent; running: root.session.busy; visible: running}
                        QQC.Label {
                            anchors.centerIn: parent; width: parent.width - 32
                            visible: !root.session.ready && !root.session.busy
                            text: i18nc("@info", "Adjust the images or join method to preview your copy.")
                            wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter
                            color: Kos.AppTheme.mutedText
                        }
                    }
                    QQC.Label {
                        objectName: "nkshotCombineReviewStatus"
                        Layout.fillWidth: true
                        text: combineWindow.selectionError.length ? combineWindow.selectionError
                            : root.session.error.length ? root.session.error
                            : root.session.ready ? i18nc("@info", "%1 × %2 pixels", root.session.outputSize.width, root.session.outputSize.height)
                            : root.session.busy ? i18nc("@info", "Preparing preview…") : ""
                        textFormat: Text.PlainText
                        color: Kos.AppTheme.mutedText
                        wrapMode: Text.Wrap
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Action {objectName: "nkshotCombineReviewCancel"; text: i18nc("@action", "Cancel"); onClicked: combineWindow.close()}
            Item {Layout.fillWidth: true}
            Action {
                id: create
                objectName: "nkshotCombineReviewCreate"
                text: i18nc("@action", "Create copy")
                enabled: root.session.ready && !root.session.busy && !combineWindow.choosingImages
                palette.buttonText: Kos.AppTheme.accentText
                background: Kos.KosSurface {
                    fillColor: Kos.AppTheme.accent; radius: 9; elevation: 0; strokeWidth: 0
                    hovered: create.hovered; pressed: create.down; focused: create.visualFocus
                }
                onClicked: combineWindow.createCopy()
            }
        }
    }
}
