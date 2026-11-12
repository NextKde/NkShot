// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import Kos.Ui as Kos

QQC.Pane {
    id: root
    readonly property string draftHint: i18nc("@info", "Draft stays only while NkShot is open. Copy or pin the image to keep it.")
    padding: 20
    font: Qt.application.font
    palette.text: Kos.AppTheme.paletteText
    palette.windowText: Kos.AppTheme.paletteText
    palette.buttonText: Kos.AppTheme.paletteText
    // A decorated QQuickView needs an opaque, square background across its client area.
    background: Rectangle { color: Kos.AppTheme.contentSecondaryBackground }

    function refresh() {
        previewTimer.stop()
        noteWindow.updatePreview(editor.text, styleChoice.currentIndex)
    }
    Timer { id: previewTimer; interval: 120; onTriggered: root.refresh() }

    component NoteButton: Kos.LiquidActionButton {
        id: button
        implicitHeight: 30
        // Measure the caption independently of SymbolLabel's allocated width.
        implicitWidth: iconOnly ? implicitHeight : Math.max(implicitHeight,
            captionMetrics.advanceWidth(text) + (icon.name.length && display !== QQC.AbstractButton.TextOnly ? 20 : 0)
            + leftPadding + rightPadding)
        FontMetrics { id: captionMetrics; font: button.font }
        materialForm: true
        iconOnly: display === QQC.AbstractButton.IconOnly
        font.pixelSize: 13
        // Utility-window actions use feedback from the surface, without a focus ring.
        focusRingColor: "transparent"
        contentItem: SymbolLabel { control: parent; color: parent.textColor }
        QQC.ToolTip.visible: hovered
        QQC.ToolTip.text: text
        Accessible.name: text
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            QQC.Label {
                text: i18nc("@title", "Quick Note")
                font.pixelSize: 17
                font.weight: Font.DemiBold
                color: Kos.AppTheme.paletteText
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                elide: Text.ElideRight
            }
            NoteButton {
                objectName: "nkshotNoteCopy"
                text: i18nc("@action", "Copy image")
                icon.name: "edit-copy"
                display: root.width < 520 ? QQC.AbstractButton.IconOnly : QQC.AbstractButton.TextBesideIcon
                enabled: noteWindow.canExport && !previewTimer.running
                onClicked: { root.refresh(); noteWindow.copyImage() }
            }
            NoteButton {
                objectName: "nkshotNotePin"
                text: i18nc("@action", "Pin note")
                enabled: noteWindow.canExport && !previewTimer.running
                highlighted: true
                onClicked: { root.refresh(); noteWindow.pinImage() }
            }
            NoteButton {
                objectName: "nkshotNoteClose"
                text: i18nc("@action", "Close")
                icon.name: "window-close"
                display: QQC.AbstractButton.IconOnly
                onClicked: noteWindow.close()
            }
        }
        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: root.width >= 700 ? 2 : 1
            columnSpacing: 12
            rowSpacing: 12
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 320
                Layout.preferredHeight: root.width >= 700 ? -1 : 180
                spacing: 12
                RowLayout {
                    Layout.fillWidth: true
                    Layout.minimumHeight: styleChoice.implicitHeight
                    QQC.Label {
                        text: i18nc("@label", "Note text")
                        color: Kos.AppTheme.mutedText
                        font.pixelSize: 12
                        Layout.fillWidth: true
                    }
                    QQC.Label {
                        text: editor.text.length + " / 2000"
                        color: Kos.AppTheme.mutedText
                        font.pixelSize: 12
                    }
                }
                QQC.ScrollView {
                    // The layout owns the viewport; text wrapping must not feed back into its hints.
                    implicitWidth: 0
                    implicitHeight: 0
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    padding: 0
                    contentWidth: availableWidth
                    background: Kos.KosSurface {
                        fillColor: Kos.AppTheme.contentBackground
                        radius: 12
                        elevation: 0
                        strokeWidth: 0
                    }
                    QQC.TextArea {
                        id: editor
                        focus: true
                        Component.onCompleted: forceActiveFocus()
                        objectName: "nkshotNoteEditor"
                        placeholderText: i18nc("@info:placeholder", "Write a reminder, steps, or a quote…")
                        wrapMode: TextEdit.Wrap
                        textFormat: TextEdit.PlainText
                        font.pixelSize: 13
                        padding: 12
                        color: Kos.AppTheme.paletteText
                        placeholderTextColor: Kos.AppTheme.mutedText
                        selectionColor: Kos.AppTheme.accent
                        selectedTextColor: Kos.AppTheme.accentText
                        selectByMouse: true
                        background: null
                        onTextChanged: { feedback.text = root.draftHint; previewTimer.restart() }
                    }
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 460
                spacing: 12
                RowLayout {
                    Layout.fillWidth: true
                    QQC.Label {
                        text: i18nc("@label", "Image preview")
                        color: Kos.AppTheme.mutedText
                        font.pixelSize: 12
                        Layout.fillWidth: true
                    }
                    Kos.LiquidSelect {
                        id: styleChoice
                        objectName: "nkshotNoteStyle"
                        model: [i18nc("@item", "Paper"), i18nc("@item", "Blue"), i18nc("@item", "Night")]
                        implicitHeight: 30
                        materialForm: true
                        plainTrigger: true
                        indicatorStyle: "upDown"
                        popupBorderWidth: 0
                        appearanceMode: Kos.AppTheme.dark ? "dark" : "light"
                        accentColor: Kos.AppTheme.accent
                        focusRingColor: "transparent"
                        opacity: 1
                        textColor: enabled ? Kos.AppTheme.paletteText : Kos.AppTheme.disabledText
                        mutedTextColor: enabled ? Kos.AppTheme.mutedText : Kos.AppTheme.disabledText
                        Accessible.name: i18nc("@label", "Note style")
                        onActivated: index => currentIndex = index
                        onCurrentIndexChanged: previewTimer.restart()
                    }
                }
                Kos.KosSurface {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 120
                    fillColor: Kos.AppTheme.contentBackground
                    radius: 12
                    elevation: 0
                    strokeWidth: 0
                    Image {
                        objectName: "nkshotNotePreview"
                        anchors.fill: parent
                        anchors.margins: 12
                        source: noteWindow.imageSource
                        fillMode: Image.PreserveAspectFit
                        cache: false
                        smooth: true
                    }
                    QQC.Label {
                        anchors.centerIn: parent
                        width: Math.min(260, parent.width - 24)
                        visible: !noteWindow.canExport
                        text: i18nc("@info", "Your note becomes a shareable image here.")
                        wrapMode: Text.Wrap
                        horizontalAlignment: Text.AlignHCenter
                        color: Kos.AppTheme.mutedText
                        font.pixelSize: 12
                    }
                }
            }
        }
        QQC.Label {
            Layout.fillWidth: true
            visible: noteWindow.error.length > 0
            text: noteWindow.error
            color: Kos.AppTheme.paletteText
            wrapMode: Text.Wrap
            font.pixelSize: 12
        }
        QQC.Label {
            id: feedback
            objectName: "nkshotNoteFeedback"
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Kos.AppTheme.mutedText
            font.pixelSize: 12
            text: root.draftHint
            Connections {
                target: noteWindow
                function onStatusMessage(message) { feedback.text = message }
            }
        }
    }
}
