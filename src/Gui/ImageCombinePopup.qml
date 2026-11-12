// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import Kos.Ui as Kos

Kos.LiquidDialog {
    id: root
    objectName: "nkshotCombinePopup"
    property bool horizontal: false
    property bool keepAllPixels: false
    property bool busy: false
    signal chooseImages(bool keepAllPixels, bool horizontal)
    width: Math.min(380, parent.width - 32)
    anchors.centerIn: parent
    sourceItem: parent
    focus: true
    // Keep the title and actions in the content layout, including in compact windows.
    header: null
    footer: null
    contentItem: ColumnLayout {
        spacing: 14
        QQC.Label {
            text: i18nc("@title", "Combine images")
            font.pixelSize: 17
            font.bold: true
            color: Kos.AppTheme.paletteText
        }
        QQC.Label {
            Layout.fillWidth: true
            text: i18nc("@info", "Create one image from 2–16 files. Your sources and current edit stay unchanged.")
            wrapMode: Text.Wrap
            color: Kos.AppTheme.mutedText
        }
        QQC.Label { text: i18nc("@label", "Direction"); color: Kos.AppTheme.paletteText }
        Kos.LiquidSegmentedControl {
            objectName: "nkshotCombineDirection"
            Layout.fillWidth: true
            implicitHeight: 28
            opacity: 1
            textColor: enabled ? Kos.AppTheme.paletteText : Kos.AppTheme.disabledText
            mutedTextColor: enabled ? Kos.AppTheme.mutedText : Kos.AppTheme.disabledText
            labels: [i18nc("@option", "Vertical"), i18nc("@option", "Horizontal")]
            currentIndex: root.horizontal ? 1 : 0
            onSelectionRequested: index => root.horizontal = index === 1
        }
        QQC.Label { text: i18nc("@label", "Join method"); color: Kos.AppTheme.paletteText }
        Kos.LiquidSegmentedControl {
            objectName: "nkshotCombineMethod"
            Layout.fillWidth: true
            implicitHeight: 28
            opacity: 1
            textColor: enabled ? Kos.AppTheme.paletteText : Kos.AppTheme.disabledText
            mutedTextColor: enabled ? Kos.AppTheme.mutedText : Kos.AppTheme.disabledText
            labels: [i18nc("@option", "Match overlap"), i18nc("@option", "Keep all pixels")]
            currentIndex: root.keepAllPixels ? 1 : 0
            onSelectionRequested: index => root.keepAllPixels = index === 1
        }
        QQC.Label {
            objectName: "nkshotCombineMethodHint"
            Layout.fillWidth: true
            text: root.keepAllPixels
                ? i18nc("@info", "Join every pixel edge to edge, with no overlap removed.")
                : i18nc("@info", "Remove matching overlap between screenshots. Keep at least 24 pixels of unchanged overlap.")
            wrapMode: Text.Wrap
            color: Kos.AppTheme.mutedText
        }
        QQC.Label {
            objectName: "nkshotCombineOrderHint"
            Layout.fillWidth: true
            text: root.horizontal
                ? i18nc("@info", "Starts left → right by filename (1, 2, 10). Review and reorder next. Images need the same height, scale and color profile.")
                : i18nc("@info", "Starts top → bottom by filename (1, 2, 10). Review and reorder next. Images need the same width, scale and color profile.")
            wrapMode: Text.Wrap
            color: Kos.AppTheme.mutedText
        }
        // Dialog actions are right-aligned throughout: Cancel precedes the primary action.
        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: 8
            Kos.LiquidActionButton {
                objectName: "nkshotCombineCancel"
                text: i18nc("@action", "Cancel")
                implicitHeight: root.actionHeight
                materialForm: true
                onClicked: root.close()
            }
            Kos.LiquidActionButton {
                objectName: "nkshotCombineChoose"
                text: i18nc("@action", "Choose images…")
                enabled: !root.busy
                implicitHeight: root.actionHeight
                materialForm: true
                highlighted: true
                onClicked: {
                    root.close()
                    root.chooseImages(root.keepAllPixels, root.horizontal)
                }
            }
        }
    }
}
