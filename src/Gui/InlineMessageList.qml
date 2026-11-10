/* SPDX-FileCopyrightText: 2022 Noah Davis <noahadvs@gmail.com>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import Kos.Ui as Kos
import org.kde.kirigami as Kirigami
import org.kde.spectacle.private

ListView {
    id: root
    objectName: "nkshotNotifications"
    property real maximumHeight: Number.POSITIVE_INFINITY
    model: InlineMessageModel
    interactive: contentHeight > height
    clip: true
    spacing: 8
    QQC.ScrollBar.vertical: QQC.ScrollBar {}
    visible: height > 0
    height: Math.min(implicitHeight, maximumHeight)
    implicitHeight: contentHeight
    contentWidth: width
    Behavior on height {
        NumberAnimation {
            duration: Settings.reduceUiMotion ? 0 : Kirigami.Units.longDuration
            easing.type: Easing.OutCubic
        }
    }
    add: Transition {
        NumberAnimation {
            property: "opacity"
            from: 0
            to: 1
            duration: Settings.reduceUiMotion ? 0 : Kirigami.Units.longDuration
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            property: "y"
            duration: Settings.reduceUiMotion ? 0 : Kirigami.Units.longDuration
            easing.type: Easing.OutCubic
        }
    }
    populate: Transition {
        NumberAnimation {
            property: "opacity"
            from: 0
            to: 1
            duration: Settings.reduceUiMotion ? 0 : Kirigami.Units.longDuration
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            property: "y"
            duration: Settings.reduceUiMotion ? 0 : Kirigami.Units.longDuration
            easing.type: Easing.OutCubic
        }
    }
    remove: Transition {
        NumberAnimation {
            property: "opacity"
            from: 1
            to: 0
            duration: Settings.reduceUiMotion ? 0 : Kirigami.Units.longDuration
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            property: "y"
            duration: Settings.reduceUiMotion ? 0 : Kirigami.Units.longDuration
            easing.type: Easing.OutCubic
        }
    }
    displaced: Transition {
        NumberAnimation {
            property: "y"
            duration: Settings.reduceUiMotion ? 0 : Kirigami.Units.longDuration
            easing.type: Easing.OutCubic
        }
    }
    delegate: QQC.Pane {
        id: delegate
        required property int index
        required property var model
        readonly property string dataString: model.data !== null && model.data !== undefined
            ? model.data.toString() : ""
        readonly property bool localFile: {
            try { return new URL(dataString).protocol === "file:" } catch (error) { return false }
        }
        z: -index
        width: root.width
        padding: 10
        leftPadding: 12
        rightPadding: 6
        background: Kos.KosSurface {
            radius: 12
            fillColor: Kos.AppTheme.contentBackground
            strokeColor: Kos.AppTheme.border
            elevation: 0.3
        }
        contentItem: RowLayout {
            spacing: 8
            Kirigami.Icon {
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
                Layout.alignment: Qt.AlignTop
                Layout.topMargin: 5
                source: delegate.model.type === InlineMessageModel.Error ? "dialog-error"
                    : delegate.model.type === InlineMessageModel.Warning ? "dialog-warning" : "dialog-information"
            }
            QQC.Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 40
                color: Kos.AppTheme.paletteText
                textFormat: Text.RichText
                wrapMode: Text.Wrap
                text: delegate.model.text.replace(/<a /g, '<a style="color: '
                    + (Kos.AppTheme.dark ? "#8cc8ff" : "#0066cc") + ';" ')
                onLinkActivated: (link) => Qt.openUrlExternally(link)
            }
            RowLayout {
                spacing: 0
                Layout.alignment: Qt.AlignTop
                NotificationButton {
                    objectName: "nkshotNotificationFolder" + delegate.index
                    visible: delegate.localFile
                    text: i18nc("@action:button", "Open Containing Folder")
                    icon.name: "document-open-folder"
                    onClicked: InlineMessageModel.openContainingFolder(delegate.dataString)
                }
                NotificationButton {
                    objectName: "nkshotNotificationCopy" + delegate.index
                    visible: delegate.dataString.length > 0
                    text: i18nc("@action:button", "Copy to Clipboard")
                    icon.name: "edit-copy"
                    onClicked: InlineMessageModel.copyToClipboard(delegate.model.data)
                }
                NotificationButton {
                    objectName: "nkshotCloseNotification" + delegate.index
                    text: i18nc("@action:button close inline notification", "Close")
                    icon.name: "dialog-close"
                    onClicked: InlineMessageModel.pop(delegate.index)
                }
            }
        }
    }
    component NotificationButton: QQC.ToolButton {
        implicitWidth: 28
        implicitHeight: 28
        icon.width: 16
        icon.height: 16
        icon.color: Kos.AppTheme.paletteText
        display: QQC.AbstractButton.IconOnly
        QQC.ToolTip.text: text
        QQC.ToolTip.visible: hovered || (activeFocus && !down)
        background: Rectangle {
            radius: 7
            color: parent.hovered || parent.down ? Kos.AppTheme.controlBackground : "transparent"
            border.width: parent.visualFocus ? 2 : 0
            border.color: Kos.AppTheme.accent
        }
    }
}
