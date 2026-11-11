// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import Kos.Ui as Kos
import org.kde.kirigami as Kirigami

QQC.Popup {
    id: root
    objectName: "nkshotRecentMediaPopup"
    readonly property var history: contextWindow.recentMedia
    width: Math.min(600, parent ? parent.width - 24 : 600)
    height: Math.min(460, parent ? parent.height - 32 : 460)
    x: parent ? (parent.width-width)/2 : 0
    y: parent ? (parent.height-height)/2 : 0
    padding: 16
    modal: true
    focus: true
    font: Kirigami.Theme.defaultFont
    closePolicy: QQC.Popup.CloseOnEscape | QQC.Popup.CloseOnPressOutside
    background: Kos.KosSurface {fillColor: Kos.AppTheme.contentBackground; radius: 16; elevation: 1}
    function openUrl(url) { root.close(); contextWindow.openRecentMedia(url) }
    onOpened: {history.refresh(); files.currentIndex = history.count ? 0 : -1; files.forceActiveFocus()}
    contentItem: ColumnLayout {
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            QQC.Label {text: i18nc("@title", "Recent files"); font.bold: true; font.pointSize: Qt.application.font.pointSize * 1.2; color: Kos.AppTheme.paletteText}
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
            spacing: 4
            keyNavigationEnabled: true
            highlightMoveDuration: 0
            QQC.ScrollBar.vertical: QQC.ScrollBar {}
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
                width: files.width
                height: 72
                QQC.ItemDelegate {
                    id: openButton
                    objectName: "nkshotRecentOpen_" + row.index
                    anchors.fill: parent
                    anchors.rightMargin: 38
                    padding: 10
                    enabled: row.available && contextWindow.canOpenMedia
                    Accessible.name: row.name + ", " + row.folder
                    onClicked: root.openUrl(row.url)
                    background: Rectangle {
                        color: openButton.hovered || (files.activeFocus && files.currentIndex === row.index)
                            ? Kos.AppTheme.withAlpha(Kos.AppTheme.accent, 0.12) : "transparent"
                        radius: 10
                        border.width: openButton.visualFocus ? 1 : 0
                        border.color: Kos.AppTheme.accent
                    }
                    contentItem: RowLayout {
                        spacing: 12
                        QQC.Label {text: row.kind === "video" ? i18nc("@label", "Video") : i18nc("@label", "Image"); color: Kos.AppTheme.mutedText; Layout.preferredWidth: 45}
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            QQC.Label {text: row.name; textFormat: Text.PlainText; font.bold: true; color: Kos.AppTheme.paletteText; elide: Text.ElideMiddle; Layout.fillWidth: true}
                            QQC.Label {text: row.available ? row.folder : i18nc("@info", "Moved or missing"); textFormat: Text.PlainText; color: Kos.AppTheme.mutedText; elide: Text.ElideMiddle; Layout.fillWidth: true}
                        }
                    }
                    opacity: enabled ? 1 : 0.5
                }
                QQC.ToolButton {
                    objectName: "nkshotRecentRemove_" + row.index
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    width: 34; height: 34
                    text: "×"
                    font.pixelSize: 20
                    palette.buttonText: Kos.AppTheme.mutedText
                    Accessible.name: i18nc("@action", "Remove %1 from this list", row.name)
                    QQC.ToolTip.visible: hovered
                    QQC.ToolTip.text: i18nc("@info", "Remove from list, not from disk")
                    onClicked: root.history.removeAt(row.index)
                }
            }
            QQC.Label {
                anchors.centerIn: parent
                width: parent.width - 24
                visible: root.history.count === 0
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: i18nc("@info", "No recent files yet. Open or save an image or recording to find it here.")
                color: Kos.AppTheme.mutedText
            }
        }
        QQC.Label {Layout.fillWidth: true; visible: text.length > 0; text: root.history.error; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: Kos.AppTheme.paletteText}
        RowLayout {
            Layout.fillWidth: true
            Kos.KosButton {objectName: "nkshotRecentClear"; secondary: true; text: i18nc("@action", "Clear list"); enabled: root.history.count > 0 || root.history.error.length > 0; onClicked: root.history.clear(); palette.buttonText: Kos.AppTheme.paletteText}
            Item {Layout.fillWidth: true}
            Kos.KosButton {objectName: "nkshotRecentClose"; highlighted: true; flatPrimary: true; lightLabel: true; text: i18nc("@action", "Close"); onClicked: root.close(); palette.buttonText: Kos.AppTheme.paletteText}
        }
    }
}
