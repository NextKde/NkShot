// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import Kos.Ui as Kos
import org.kde.spectacle.private
import org.kde.kquickimageeditor

QQC.Pane {
    id: root
    padding: 12
    property real zoom: 0
    readonly property var document: copyEditor.document
    readonly property real fitZoom: Math.min(1, Math.max(0.01, Math.min((canvas.width-24)/document.canvasRect.width, (canvas.height-24)/document.canvasRect.height)))
    readonly property real viewZoom: zoom > 0 ? zoom : fitZoom
    palette.text: Kos.AppTheme.paletteText
    palette.buttonText: Kos.AppTheme.paletteText
    palette.windowText: Kos.AppTheme.paletteText
    background: Rectangle { color: Kos.AppTheme.contentSecondaryBackground }
    function commitOptions() { root.forceActiveFocus(); options.flushPendingChanges() }
    component ActionButton: QQC.Button {
        id: button
        property bool primary: false
        implicitHeight: 36
        leftPadding: 12; rightPadding: 12
        font: Qt.application.font
        contentItem: QQC.Label { text: button.text; font: button.font; color: button.primary ? Kos.AppTheme.accentText : Kos.AppTheme.paletteText; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Kos.KosSurface {
            fillColor: button.primary ? Kos.AppTheme.accent : Kos.AppTheme.controlBackground
            radius: 10; elevation: 0; strokeWidth: 0
            hovered: button.hovered; pressed: button.down; focused: button.visualFocus
        }
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        RowLayout {
            Layout.fillWidth: true
            QQC.Label { visible: root.width >= 480; text: i18nc("@title", "Edit copy"); font.bold: true; color: Kos.AppTheme.paletteText }
            Item { Layout.fillWidth: true }
            UndoRedoGroup { editorDocument: root.document; buttonHeight: 36; animationsEnabled: false }
            ActionButton { objectName: "nkshotCopyEditorCopy"; text: i18nc("@action", "Copy"); onClicked: { root.commitOptions(); copyEditor.copyImage() } }
            ActionButton { objectName: "nkshotCopyEditorSave"; text: i18nc("@action", "Save…"); primary: true; onClicked: { root.commitOptions(); copyEditor.saveAs() } }
            ActionButton { objectName: "nkshotCopyEditorDone"; text: i18nc("@action", "Done"); onClicked: { root.commitOptions(); copyEditor.finishCopy() } }
        }
        QQC.Label {
            id: feedback
            Layout.fillWidth: true
            text: i18nc("@info", "Edit freely. The original stays unchanged.")
            color: Kos.AppTheme.mutedText
            wrapMode: Text.Wrap
            Connections { target: copyEditor; function onSaveFinished(message, success) { feedback.text = message } function onImageCopied() { feedback.text = i18nc("@info", "Edited image copied. Save a PNG or use Done to keep a separate image.") } }
        }
        Rectangle {
            id: canvas
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Kos.AppTheme.contentBackground
            radius: 12
            clip: true
            Flickable {
                id: view
                anchors.fill: parent
                contentWidth: Math.max(width, editor.width * root.viewZoom + 24)
                contentHeight: Math.max(height, editor.height * root.viewZoom + 24)
                interactive: root.document.tool.type === AnnotationTool.NoTool
                boundsBehavior: Flickable.StopAtBounds
                QQC.ScrollBar.vertical: QQC.ScrollBar { }
                QQC.ScrollBar.horizontal: QQC.ScrollBar { }
                AnnotationEditor {
                    id: editor
                    editorDocument: root.document
                    width: root.document.canvasRect.width
                    height: root.document.canvasRect.height
                    x: Math.max(12, (view.contentWidth-width*scale)/2)
                    y: Math.max(12, (view.contentHeight-height*scale)/2)
                    scale: root.viewZoom
                    transformOrigin: Item.TopLeft
                    Keys.forwardTo: crop
                    Keys.priority: Keys.AfterItem
                }
                CropTool {
                    id: crop
                    anchors.fill: editor
                    transformOrigin: editor.transformOrigin
                    scale: editor.scale
                    viewport: editor
                    active: root.document.tool.type === AnnotationTool.CropTool
                }
            }
        }
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 6
            ActionButton { text: i18nc("@action", "Fit"); onClicked: root.zoom = 0 }
            ActionButton { objectName: "nkshotCopyEditorZoomOut"; text: "−"; Accessible.name: i18nc("@action", "Zoom out"); onClicked: root.zoom = Math.max(root.fitZoom, root.viewZoom / 1.25) }
            QQC.Label { text: Math.round(root.viewZoom*100)+"%"; color: Kos.AppTheme.mutedText }
            ActionButton { objectName: "nkshotCopyEditorZoomIn"; text: "+"; Accessible.name: i18nc("@action", "Zoom in"); onClicked: root.zoom = Math.min(4, root.viewZoom * 1.25) }
        }
        AnnotationOptionsToolBarContents {
            id: options
            editorDocument: root.document
            Layout.alignment: Qt.AlignHCenter
            Layout.maximumWidth: root.availableWidth
            displayMode: QQC.AbstractButton.IconOnly
        }
        AnnotationsToolBarContents {
            editorDocument: root.document
            essentialsOnly: true
            paletteMode: true
            showUndoRedo: false
            showNoneButton: true
            Layout.alignment: Qt.AlignHCenter
            Layout.maximumWidth: root.availableWidth
        }
    }
    Shortcut { sequence: StandardKey.Save; onActivated: {root.commitOptions(); copyEditor.saveAs()} }
    Shortcut { sequence: StandardKey.SaveAs; onActivated: {root.commitOptions(); copyEditor.saveAs()} }
    Shortcut { sequence: StandardKey.Copy; onActivated: {root.commitOptions(); copyEditor.copyImage()} }
}
