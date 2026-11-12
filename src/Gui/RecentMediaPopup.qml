// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import Kos.Ui as Kos
import org.kde.kirigami as Kirigami

Kos.LiquidDialog {
    id: root
    objectName: "nkshotRecentMediaPopup"
    readonly property var history: contextWindow.recentMedia
    // The overlay contains this dialog, so sample the owning page instead.
    property Item sceneSource: null
    sourceItem: sceneSource
    width: Math.min(640, parent ? parent.width - 24 : 640)
    height: Math.min(500, parent ? parent.height - 32 : 500)
    x: parent ? (parent.width-width)/2 : 0
    y: parent ? (parent.height-height)/2 : 0
    focus: true
    header: null
    footer: null
    font: Kirigami.Theme.defaultFont
    closePolicy: QQC.Popup.CloseOnEscape | QQC.Popup.CloseOnPressOutside
    function openUrl(url) { root.close(); contextWindow.openRecentMedia(url) }
    onOpened: {history.refresh(); contextWindow.beginRecentThumbnails(); files.currentIndex = history.count ? 0 : -1; files.forceActiveFocus()}
    onClosed: contextWindow.clearRecentThumbnails()
    contentItem: ColumnLayout {
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            QQC.Label {text: i18nc("@title", "Recent files"); font.pixelSize: 17; font.weight: Font.DemiBold; color: Kos.AppTheme.paletteText}
            Item {Layout.fillWidth: true}
            QQC.Label {text: root.history.count; color: Kos.AppTheme.mutedText}
        }
        QQC.Label {
            Layout.fillWidth: true
            text: i18nc("@info", "Stored on this device. Clearing this list never deletes files.")
            color: Kos.AppTheme.mutedText
            wrapMode: Text.Wrap
        }
        ListView {
            id: files
            objectName: "nkshotRecentFiles"
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: root.history
            clip: true
            spacing: 8
            keyNavigationEnabled: true
            highlightMoveDuration: 0
            QQC.ScrollBar.vertical: QQC.ScrollBar {
                id: listBar
                implicitWidth: 10
                leftPadding: 2
                background: Item {}
            }
            function openCurrent() {if(currentItem && currentItem.available && contextWindow.canOpenMedia) root.openUrl(currentItem.url)}
            Keys.onReturnPressed: openCurrent()
            Keys.onEnterPressed: openCurrent()
            delegate: Item {
                id: row
                required property int index
                required property url url
                required property string name
                required property string folder
                required property string kind
                required property bool available
                readonly property url thumbnail: {
                    const revision = contextWindow.recentThumbnails.revision
                    return contextWindow.recentThumbnails.urlFor(row.url)
                }
                width: files.width - (listBar.visible ? listBar.width + 4 : 0)
                height: 96
                QQC.ItemDelegate {
                    id: openButton
                    objectName: "nkshotRecentOpen_" + row.index
                    anchors.fill: parent
                    anchors.rightMargin: 38
                    padding: 8
                    enabled: row.available && contextWindow.canOpenMedia
                    Accessible.name: row.name + ", " + row.folder
                    onClicked: root.openUrl(row.url)
                    background: Rectangle {
                        color: openButton.down || openButton.visualFocus || files.currentIndex === row.index
                            ? Qt.alpha(Kos.AppTheme.paletteText, 0.13)
                            : openButton.hovered ? Qt.alpha(Kos.AppTheme.paletteText, 0.07) : "transparent"
                        radius: 14
                    }
                    contentItem: RowLayout {
                        spacing: 12
                        Rectangle {
                            Layout.preferredWidth: root.width < 480 ? 80 : 104
                            Layout.preferredHeight: root.width < 480 ? 64 : 72
                            radius: 6
                            color: Kos.AppTheme.controlBackground
                            Image {
                                id: thumbnailImage
                                objectName: "nkshotRecentThumbnail_" + row.index
                                anchors.fill: parent
                                anchors.margins: 4
                                source: row.available && row.kind !== "video" ? row.thumbnail : ""
                                cache: false
                                asynchronous: false
                                fillMode: Image.PreserveAspectFit
                                visible: status === Image.Ready
                                Accessible.name: i18nc("@info", "Thumbnail of %1", row.name)
                            }
                            ColumnLayout {
                                anchors.centerIn: parent
                                width: parent.width - 8
                                visible: !thumbnailImage.visible
                                spacing: 2
                                Kirigami.Icon {
                                    Layout.alignment: Qt.AlignHCenter
                                    Layout.preferredWidth: 24
                                    Layout.preferredHeight: 24
                                    source: !row.available ? "edit-find" : row.kind === "video" ? "video-x-generic" : "image-x-generic"
                                    color: openButton.enabled ? Kos.AppTheme.mutedText : Kos.AppTheme.disabledText
                                }
                                QQC.Label {
                                    Layout.fillWidth: true
                                    text: !row.available ? i18nc("@label", "Missing") : row.kind === "video" ? i18nc("@label", "Video") : i18nc("@label", "Image")
                                    horizontalAlignment: Text.AlignHCenter
                                    color: openButton.enabled ? Kos.AppTheme.mutedText : Kos.AppTheme.disabledText
                                }
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            QQC.Label {text: row.name; textFormat: Text.PlainText; font.bold: true; color: openButton.enabled ? Kos.AppTheme.paletteText : Kos.AppTheme.disabledText; elide: Text.ElideMiddle; Layout.fillWidth: true}
                            QQC.Label {text: row.folder; textFormat: Text.PlainText; color: openButton.enabled ? Kos.AppTheme.mutedText : Kos.AppTheme.disabledText; elide: Text.ElideMiddle; Layout.fillWidth: true}
                            QQC.Label {
                                Layout.fillWidth: true
                                text: !row.available ? i18nc("@info", "Moved or missing") : row.kind === "video" ? i18nc("@label", "Video file") : i18nc("@label", "Image file")
                                color: openButton.enabled ? Kos.AppTheme.mutedText : Kos.AppTheme.disabledText
                                elide: Text.ElideRight
                            }
                        }
                    }
                    opacity: 1
                }
                Kos.LiquidActionButton {
                    objectName: "nkshotRecentRemove_" + row.index
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    implicitHeight: 28
                    materialForm: true
                    flat: true
                    iconOnly: true
                    iconName: "trashRounded"
                    Accessible.name: i18nc("@action", "Remove %1 from this list", row.name)
                    QQC.ToolTip.visible: hovered
                    QQC.ToolTip.text: i18nc("@info", "Remove from list, not from disk")
                    onClicked: root.history.removeAt(row.index)
                }
            }
            Kos.KosEmptyState {
                anchors.centerIn: parent
                width: parent.width - 24
                visible: root.history.count === 0
                title: i18nc("@title", "No recent files yet")
                description: i18nc("@info", "Open or save an image or recording to find it here.")
                symbolColor: Kos.AppTheme.mutedText
            }
        }
        QQC.Label {Layout.fillWidth: true; visible: text.length > 0; text: root.history.error; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: Kos.AppTheme.paletteText}
        RowLayout {
            Layout.fillWidth: true
            Kos.LiquidActionButton {
                objectName: "nkshotRecentClear"
                implicitHeight: root.actionHeight
                materialForm: true
                text: i18nc("@action", "Clear list")
                enabled: root.history.count > 0 || root.history.error.length > 0
                onClicked: root.history.clear()
            }
            Item {Layout.fillWidth: true}
            Kos.LiquidActionButton {
                objectName: "nkshotRecentClose"
                implicitHeight: root.actionHeight
                materialForm: true
                highlighted: true
                text: i18nc("@action", "Close")
                onClicked: root.close()
            }
        }
    }
}
