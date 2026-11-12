// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import Kos.Ui as Kos
Kos.LiquidDialog {
    id: root
    objectName: "nkshotTextCoverPopup"
    property var session
    property bool submitted: false
    signal cancelled()
    readonly property bool wideLayout: width >= 650
    width: Math.min(780, parent.width - 32)
    height: Math.min(wideLayout ? 500 : 600, parent.height - 32)
    anchors.centerIn: parent
    sourceItem: parent
    focus: true
    header: null
    footer: null
    readonly property int matchRowHeight: 28
    readonly property int visibleMatchRows: 6
    onOpened: {submitted = false; query.text = ""}
    onClosed: {query.text = ""; if (!submitted) cancelled()}
    function findMatches() {if (session && session.ready && query.text.trim().length) session.search(query.text)}
    function revealInScroll(item, scrollView) {
        const viewport = scrollView.contentItem
        if (!viewport || typeof viewport.contentY === "undefined") return
        const top = item.mapToItem(viewport, 0, 0).y
        const bottom = top + item.height
        let next = viewport.contentY
        if (top < 2) next += top - 2
        else if (bottom > viewport.height - 2) next += bottom - viewport.height + 2
        viewport.contentY = Math.max(0, Math.min(next, Math.max(0, viewport.contentHeight - viewport.height)))
    }
    function revealMatch(item) {
        revealInScroll(item, matchesScroll)
        // Re-map after the inner scroll so the outer viewport sees the final
        // position, not the offscreen position of a later result in the list.
        revealInScroll(item, scroll)
    }
    contentItem: ColumnLayout {
        spacing: 12
        QQC.Label {text: i18nc("@title", "Find and cover text"); font.pixelSize: 17; font.bold: true; color: Kos.AppTheme.paletteText}
        QQC.Label {
            objectName: "nkshotTextCoverWarning"
            Layout.fillWidth: true
            text: i18nc("@info", "OCR can miss text. Review the whole copy before sharing.")
            wrapMode: Text.Wrap; color: Kos.AppTheme.paletteText
        }
        QQC.ScrollView {
            id: scroll
            objectName: "nkshotTextCoverScroll"
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth; contentHeight: body.implicitHeight
            // Reserve a stable gutter so wrapping cannot toggle the scrollbar's width.
            rightPadding: mirrored ? 0 : 14
            leftPadding: mirrored ? 14 : 0
            QQC.ScrollBar.horizontal.policy: QQC.ScrollBar.AlwaysOff
            QQC.ScrollBar.vertical: QQC.ScrollBar {
                id: bar
                objectName: "nkshotTextCoverScrollbar"
                parent: scroll; x: scroll.mirrored ? 0 : scroll.width - width
                y: scroll.topPadding; height: scroll.availableHeight
                implicitWidth: 10
                leftPadding: 2
                background: Item {}
                policy: body.implicitHeight > scroll.availableHeight ? QQC.ScrollBar.AlwaysOn : QQC.ScrollBar.AlwaysOff
            }
            GridLayout {
                id: body
                width: scroll.availableWidth
                columns: root.wideLayout ? 2 : 1
                columnSpacing: 20; rowSpacing: 14
                Rectangle {
                    Layout.fillWidth: true; Layout.minimumWidth: 0
                    Layout.preferredWidth: root.wideLayout ? 400 : 320
                    Layout.preferredHeight: root.wideLayout ? 330 : 160
                    Layout.alignment: Qt.AlignTop
                    radius: 14; color: Kos.AppTheme.controlBackground
                    clip: true
                    Image {
                        id: preview
                        objectName: "nkshotTextCoverPreview"
                        anchors.fill: parent; anchors.margins: 12
                        source: root.session ? root.session.imageSource : ""
                        cache: false; asynchronous: false
                        fillMode: Image.PreserveAspectFit
                        Repeater {
                            model: root.session ? root.session.matches : []
                            delegate: Item {
                                id: matchLayer
                                required property var modelData
                                anchors.fill: parent
                                visible: modelData.selected
                                Repeater {
                                    model: matchLayer.modelData.regions
                                    delegate: Rectangle {
                                        required property var modelData
                                        x: (preview.width - preview.paintedWidth) / 2 + modelData.x / root.session.sourceSize.width * preview.paintedWidth
                                        y: (preview.height - preview.paintedHeight) / 2 + modelData.y / root.session.sourceSize.height * preview.paintedHeight
                                        width: modelData.width / root.session.sourceSize.width * preview.paintedWidth
                                        height: modelData.height / root.session.sourceSize.height * preview.paintedHeight
                                        // Match the exported copy's opaque black privacy covers, independent of theme.
                                        color: "black"
                                    }
                                }
                            }
                        }
                    }
                    QQC.Label {
                        anchors.centerIn: parent
                        visible: preview.status !== Image.Ready
                        text: i18nc("@info", "Preparing image…")
                        color: Kos.AppTheme.mutedText
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true; Layout.minimumWidth: 0
                    Layout.preferredWidth: root.wideLayout ? 280 : 320
                    Layout.alignment: Qt.AlignTop
                    spacing: 10
                    QQC.Label {
                        Layout.fillWidth: true
                        text: i18nc("@info", "Search locally, then choose the matches to cover. The separate copy has a white background and opaque black covers; your edit stays unchanged.")
                        wrapMode: Text.Wrap; color: Kos.AppTheme.mutedText
                    }
                    Kos.LiquidTextField {
                        id: query
                        objectName: "nkshotTextCoverQuery"
                        Layout.fillWidth: true
                        implicitHeight: 28
                        topPadding: 4
                        bottomPadding: 4
                        leftPadding: 12
                        rightPadding: 12
                        specificationForm: true
                        opacity: 1
                        color: enabled ? Kos.AppTheme.paletteText : Kos.AppTheme.disabledText
                        maximumLength: 256
                        placeholderText: i18nc("@info:placeholder", "Text to cover")
                        Accessible.name: i18nc("@label", "Find text to cover")
                        enabled: root.session && root.session.ready
                        onTextChanged: if (root.session) root.session.clearMatches()
                        onAccepted: root.findMatches()
                    }
                    Kos.LiquidActionButton {
                        implicitHeight: 28
                        materialForm: true
                        objectName: "nkshotTextCoverFind"
                        text: i18nc("@action", "Find")
                        enabled: root.session && root.session.ready && query.text.trim().length > 0
                        onClicked: root.findMatches()
                    }
                    QQC.BusyIndicator {visible: root.session && root.session.busy; running: visible; Layout.preferredHeight: 30; Layout.preferredWidth: 30}
                    QQC.Label {
                        Layout.fillWidth: true
                        visible: root.session && root.session.busy
                        text: i18nc("@info", "Recognizing text locally…")
                        color: Kos.AppTheme.mutedText; wrapMode: Text.Wrap
                    }
                    QQC.Label {
                        objectName: "nkshotTextCoverStatus"
                        Layout.fillWidth: true
                        text: root.session ? (root.session.error.length ? root.session.error : root.session.matchCount ? i18nc("@info", "%1 of %2 matches selected", root.session.selectedCount, root.session.matchCount) : "") : ""
                        visible: text.length > 0
                        textFormat: Text.PlainText
                        wrapMode: Text.Wrap; color: Kos.AppTheme.mutedText
                    }
                    QQC.ScrollView {
                        id: matchesScroll
                        objectName: "nkshotTextCoverMatchesScroll"
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(root.visibleMatchRows * root.matchRowHeight
                            + (root.visibleMatchRows - 1) * matchRows.spacing, matchRows.implicitHeight)
                        visible: root.session && root.session.matchCount > 0
                        contentWidth: availableWidth
                        contentHeight: matchRows.implicitHeight
                        clip: true
                        rightPadding: mirrored ? 0 : 14
                        leftPadding: mirrored ? 14 : 0
                        QQC.ScrollBar.horizontal.policy: QQC.ScrollBar.AlwaysOff
                        QQC.ScrollBar.vertical: QQC.ScrollBar {
                            id: matchesBar
                            objectName: "nkshotTextCoverMatchesScrollbar"
                            parent: matchesScroll
                            x: matchesScroll.mirrored ? 0 : matchesScroll.width - width
                            y: matchesScroll.topPadding
                            height: matchesScroll.availableHeight
                            implicitWidth: 10
                            leftPadding: 2
                            background: Item {}
                            policy: matchRows.implicitHeight > matchesScroll.availableHeight ? QQC.ScrollBar.AlwaysOn : QQC.ScrollBar.AlwaysOff
                        }
                        ColumnLayout {
                            id: matchRows
                            width: matchesScroll.availableWidth
                            spacing: 6
                            Repeater {
                                model: root.session ? root.session.candidates : []
                                delegate: PopupCheckBox {
                                    id: matchBox
                                    required property int index
                                    required property var modelData
                                    objectName: "nkshotTextCoverMatch_" + index
                                    Layout.fillWidth: true
                                    implicitHeight: root.matchRowHeight
                                    text: modelData.text
                                    QQC.ToolTip.visible: hovered && contentItem.truncated
                                    QQC.ToolTip.text: text
                                    checked: {
                                        const selected = root.session ? root.session.selectedCount : 0
                                        return root.session && root.session.isSelected(index)
                                    }
                                    Accessible.name: modelData.text
                                    onActiveFocusChanged: if (activeFocus) root.revealMatch(matchBox)
                                    onToggled: root.session.setSelected(index, checked)
                                    background: Rectangle {
                                        radius: height / 2
                                        color: matchBox.hovered || matchBox.activeFocus ? Kos.AppTheme.controlBackground : "transparent"
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: 8
            Kos.LiquidActionButton {
                objectName: "nkshotTextCoverCancel"
                text: i18nc("@action", "Cancel")
                implicitHeight: root.actionHeight
                materialForm: true
                onClicked: root.close()
            }
            Kos.LiquidActionButton {
                objectName: "nkshotTextCoverCreate"
                text: i18nc("@action", "Create copy")
                enabled: root.session && root.session.ready && !root.session.busy && root.session.selectedCount > 0
                implicitHeight: root.actionHeight
                materialForm: true
                highlighted: true
                onClicked: root.accepted()
            }
        }
    }
}
