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
    readonly property real fitZoom: document.canvasRect.width > 0 && document.canvasRect.height > 0
        && canvas.width > 24 && canvas.height > 24
        ? Math.min(1, (canvas.width - 24) / document.canvasRect.width, (canvas.height - 24) / document.canvasRect.height) : 1
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
            QQC.Label { visible: root.width >= 480; text: copyEditor.annotationMode ? i18nc("@title", "Video annotations") : i18nc("@title", "Edit copy"); font.bold: true; color: Kos.AppTheme.paletteText }
            Item { Layout.fillWidth: true }
            UndoRedoGroup { enabled: !copyEditor.comparing; editorDocument: root.document; buttonHeight: 36; animationsEnabled: false }
            ActionButton { objectName: "nkshotCopyEditorCopy"; enabled: !copyEditor.comparing; visible: !copyEditor.annotationMode; text: i18nc("@action", "Copy"); onClicked: { root.commitOptions(); copyEditor.copyImage() } }
            ActionButton { objectName: "nkshotCopyEditorSave"; enabled: !copyEditor.comparing; visible: !copyEditor.annotationMode; text: i18nc("@action", "Save…"); primary: true; onClicked: { root.commitOptions(); copyEditor.saveAs() } }
            ActionButton { objectName: "nkshotCopyEditorDone"; enabled: !copyEditor.comparing; primary: copyEditor.annotationMode; text: i18nc("@action", "Done"); onClicked: { root.commitOptions(); copyEditor.finishCopy() } }
        }
        QQC.Label {
            id: feedback
            Layout.fillWidth: true
            text: copyEditor.annotationMode
                ? i18nc("@info", "Fixed marks for this export. Done or closing keeps them editable here; the original stays unchanged.")
                : i18nc("@info", "Edit freely. The original stays unchanged.")
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
                objectName: "nkshotCopyEditorViewport"
                anchors.fill: parent
                contentWidth: Math.max(width, editor.width * root.viewZoom + 24)
                contentHeight: Math.max(height, editor.height * root.viewZoom + 24)
                interactive: !copyEditor.comparing && root.document.tool.type === AnnotationTool.NoTool
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
                    active: !copyEditor.annotationMode && root.document.tool.type === AnnotationTool.CropTool
                }
            }
            ImageViewportNavigation {
                id: navigation
                objectName: "nkshotCopyEditorNavigation"
                anchors.fill: parent
                viewport: view
                imageItem: editor
                zoom: root.viewZoom
                fitZoom: root.fitZoom
                fitMode: root.zoom <= 0
                enabled: !copyEditor.comparing
                onZoomRequested: value => root.zoom = value
                onFitRequested: root.zoom = 0
            }
            Rectangle {
                id: comparisonLayer
                objectName: "nkshotCopyEditorBeforeLayer"
                anchors.fill: parent
                visible: copyEditor.comparing
                color: Kos.AppTheme.contentBackground
                // Independent full-input Fit. Never reuse the cropped document's
                // canvas geometry, scale or pan for the before-editing image.
                ImageOverlayPreview {
                    objectName: "nkshotCopyEditorBeforeImage"
                    anchors.fill: parent
                    image: copyEditor.originalImage
                    readonly property real fit: copyEditor.originalSize.width > 0 && copyEditor.originalSize.height > 0
                        ? Math.max(0, Math.min((width - 24) / copyEditor.originalSize.width, (height - comparisonLabels.height - 32) / copyEditor.originalSize.height)) : 0
                    displayRect: Qt.rect((width - copyEditor.originalSize.width * fit) / 2,
                        comparisonLabels.height + 20 + (height - comparisonLabels.height - 32 - copyEditor.originalSize.height * fit) / 2,
                        copyEditor.originalSize.width * fit, copyEditor.originalSize.height * fit)
                }
                Column {
                    id: comparisonLabels
                    x: 12; y: 10; width: parent.width - 24
                    spacing: 4
                    QQC.Label {
                        objectName: "nkshotCopyEditorBeforeLabel"
                        width: parent.width
                        text: i18nc("@info", "Before editing · Full image")
                        horizontalAlignment: Text.AlignHCenter
                        color: Kos.AppTheme.paletteText
                    }
                    QQC.Label {
                        objectName: "nkshotCopyEditorCompareHint"
                        width: parent.width
                        text: i18nc("@info", "Fit to view; release to return. Copy and Save always use the edited image.")
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        color: Kos.AppTheme.mutedText
                    }
                }
                MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; onWheel: wheel => wheel.accepted = true }
            }
        }
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 6
            ActionButton { objectName: "nkshotCopyEditorFit"; enabled: !copyEditor.comparing; text: i18nc("@action", "Fit"); QQC.ToolTip.visible: hovered; QQC.ToolTip.text: i18nc("@info", "Fit canvas (Ctrl+0)"); onClicked: navigation.fit() }
            ActionButton { objectName: "nkshotCopyEditorZoomOut"; enabled: !copyEditor.comparing; text: "−"; Accessible.name: i18nc("@action", "Zoom out"); onClicked: navigation.step(1 / 1.25) }
            ActionButton {
                objectName: "nkshotCopyEditorActualSize"
                enabled: !copyEditor.comparing
                Layout.preferredWidth: 62
                leftPadding: 4; rightPadding: 4
                text: copyEditor.comparing ? i18nc("@info", "Fit") : (root.viewZoom < 0.01 ? (root.viewZoom * 100).toPrecision(2) : Math.round(root.viewZoom * 100)) + "%"
                Accessible.name: i18nc("@action", "Reset zoom to 100 percent")
                QQC.ToolTip.visible: hovered
                QQC.ToolTip.text: i18nc("@info", "Reset zoom to 100% (Ctrl+1). Ctrl+wheel zooms; middle-drag pans.")
                onClicked: navigation.zoomTo(1)
            }
            ActionButton { objectName: "nkshotCopyEditorZoomIn"; enabled: !copyEditor.comparing; text: "+"; Accessible.name: i18nc("@action", "Zoom in"); onClicked: navigation.step(1.25) }
            ActionButton {
                id: compareButton
                objectName: "nkshotCopyEditorCompare"
                visible: !copyEditor.annotationMode
                enabled: copyEditor.compareAvailable
                text: i18nc("@action", "Compare")
                focusPolicy: Qt.TabFocus
                Accessible.name: i18nc("@action", "Hold to view before editing")
                QQC.ToolTip.visible: hovered && !pressed
                QQC.ToolTip.text: i18nc("@info", "Hold to view the full input image. Release to return.")
                onPressed: copyEditor.beginComparison()
                onReleased: copyEditor.endComparison()
                onCanceled: copyEditor.endComparison()
                onDownChanged: if (!down && pressed) copyEditor.endComparison()
                onActiveFocusChanged: if (!activeFocus && !pressed) copyEditor.endComparison()
                Keys.onPressed: event => {
                    if (event.key === Qt.Key_Space) {
                        if (!event.isAutoRepeat) copyEditor.beginComparison()
                        event.accepted = true
                    } else if (event.key === Qt.Key_Escape) {
                        copyEditor.endComparison()
                        event.accepted = true
                    }
                }
                Keys.onReleased: event => {
                    if (event.key === Qt.Key_Space) {
                        if (!event.isAutoRepeat) copyEditor.endComparison()
                        event.accepted = true
                    }
                }
            }
        }
        AnnotationOptionsToolBarContents {
            id: options
            editorDocument: root.document
            Layout.alignment: Qt.AlignHCenter
            Layout.maximumWidth: root.availableWidth
            displayMode: QQC.AbstractButton.IconOnly
        }
        AnnotationsToolBarContents {
            enabled: !copyEditor.comparing
            editorDocument: root.document
            annotationOnly: copyEditor.annotationMode
            essentialsOnly: true
            paletteMode: true
            showUndoRedo: false
            showNoneButton: true
            Layout.alignment: Qt.AlignHCenter
            Layout.maximumWidth: root.availableWidth
        }
    }
    // Preserve focused text/options and unfinished crop state; intercept new
    // pointer input rather than disabling the underlying editors.
    MouseArea {
        anchors.fill: parent
        z: 1000
        visible: copyEditor.comparing
        acceptedButtons: Qt.AllButtons
        onWheel: wheel => wheel.accepted = true
    }
    Shortcut { enabled: copyEditor.comparing; sequence: "Escape"; onActivated: copyEditor.endComparison() }
    Shortcut { enabled: copyEditor.annotationMode; sequence: "Escape"; onActivated: {root.commitOptions(); copyEditor.finishCopy()} }
    Shortcut { enabled: !copyEditor.annotationMode && !copyEditor.comparing; sequences: [StandardKey.Save]; onActivated: {root.commitOptions(); copyEditor.saveAs()} }
    Shortcut { enabled: !copyEditor.annotationMode && !copyEditor.comparing; sequences: [StandardKey.SaveAs]; onActivated: {root.commitOptions(); copyEditor.saveAs()} }
    Shortcut { enabled: !copyEditor.annotationMode && !copyEditor.comparing; sequences: [StandardKey.Copy]; onActivated: {root.commitOptions(); copyEditor.copyImage()} }
}
