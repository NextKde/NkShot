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
        padding: 12
        background: Kos.KosSurface {
            radius: 12
            fillColor: Kos.AppTheme.contentSecondaryBackground
            strokeWidth: 0
            elevation: 0
            showInnerHighlight: false
        }
        contentItem: RowLayout {
            spacing: 8
            Kirigami.Icon {
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20
                Layout.alignment: Qt.AlignVCenter
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
                    + Kos.AppTheme.accent + ';" ')
                onLinkActivated: (link) => Qt.openUrlExternally(link)
            }
            RowLayout {
                spacing: 0
                Layout.alignment: Qt.AlignVCenter
                // Optically align the glyphs while retaining 28px button targets.
                Layout.leftMargin: delegate.mirrored ? -4 : 0
                Layout.rightMargin: delegate.mirrored ? 0 : -4
                NotificationButton {
                    objectName: "nkshotNotificationFolder" + delegate.index
                    visible: delegate.localFile
                    text: i18nc("@action:button", "Open Containing Folder")
                    symbol: "document-open-folder"
                    onClicked: InlineMessageModel.openContainingFolder(delegate.dataString)
                }
                NotificationButton {
                    objectName: "nkshotNotificationCopy" + delegate.index
                    visible: delegate.dataString.length > 0
                    text: i18nc("@action:button", "Copy to Clipboard")
                    symbol: "edit-copy"
                    onClicked: InlineMessageModel.copyToClipboard(delegate.model.data)
                }
                NotificationButton {
                    objectName: "nkshotCloseNotification" + delegate.index
                    text: i18nc("@action:button close inline notification", "Close")
                    symbol: "dialog-close"
                    onClicked: InlineMessageModel.pop(delegate.index)
                }
            }
        }
    }
    component NotificationButton: Kos.LiquidActionButton {
        id: button
        implicitWidth: 28
        implicitHeight: 28
        materialForm: true
        flat: true
        iconOnly: true
        reduceMotion: Settings.reduceUiMotion
        // Pinned-font glyph, named as the freedesktop icon Spectacle uses.
        property string symbol: ""
        icon.width: 16
        icon.height: 16
        icon.color: textColor
        display: QQC.AbstractButton.IconOnly
        contentItem: Item {
            implicitWidth: glyph.implicitWidth
            implicitHeight: glyph.implicitHeight
            SymbolLabel {
                id: glyph
                anchors.centerIn: parent
                width: implicitWidth
                height: implicitHeight
                control: button
                symbol: button.symbol
            }
        }
        QQC.ToolTip.text: text
        QQC.ToolTip.visible: hovered || (activeFocus && !down)
    }
}
