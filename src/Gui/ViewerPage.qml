/* SPDX-FileCopyrightText: 2022 Noah Davis <noahadvs@gmail.com>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.Controls as QQC
import Kos.Ui as Kos
import org.kde.kirigami as Kirigami
import org.kde.spectacle.private
import org.kde.kquickimageeditor

/**
 * This page is shown when a screenshot has been taken
 * or accepted from the rectangular region capture mode.
 *
 * - There is a `contextWindow` context property that can be used to
 * access the instance of the ViewerWindow.
 */
EmptyPage {
    id: root
    readonly property Kos.LiquidBackdrop contentBackdrop: editorBackdrop
    component TaskMenu: GlassMenu {
        property bool imageOnly: true
        width: Math.min(260, root.width - 32)
        palette: moreMenu.palette
    }
    component MenuSeparator: QQC.MenuSeparator {
        padding: 6
        contentItem: Rectangle { implicitHeight: 1; color: Kos.AppTheme.border }
    }
    focus: true
    palette.windowText: Kos.AppTheme.paletteText
    palette.text: Kos.AppTheme.paletteText
    palette.buttonText: Kos.AppTheme.paletteText
    palette.window: Kos.AppTheme.contentSecondaryBackground
    palette.base: Kos.AppTheme.contentBackground
    palette.button: Kos.AppTheme.controlBackground
    palette.light: Kos.AppTheme.border
    palette.mid: Kos.AppTheme.border
    background: Rectangle { color: Kos.AppTheme.contentSecondaryBackground }

    readonly property var hostWindow: Window.window

    function toggleMaximized() {
        if (!hostWindow) return
        if (hostWindow.visibility === Window.Maximized || hostWindow.visibility === Window.FullScreen) {
            hostWindow.showNormal()
        } else {
            hostWindow.showMaximized()
        }
    }

    // Keep the initial image fit independent of labels and floating controls.
    readonly property real titleBarHeight: 52
    readonly property real canvasInset: 12
    readonly property real imageHorizontalInset: canvasInset * 2
    readonly property real imageVerticalInset: titleBarHeight + canvasInset * 2
    readonly property real minimumWidth: 480
    readonly property real minimumHeight: 320
    readonly property real overlayInset: 20
    readonly property bool editingImage: contextWindow.annotating && !SpectacleCore.videoMode
    readonly property rect imageBounds: {
        const view = contentLoader.item
        if (!view || SpectacleCore.videoMode) return Qt.rect(0, 0, contentItem.width, contentItem.height)
        const rect = view.visibleImageRect
        return Qt.rect(contentLoader.x + rect.x, contentLoader.y + rect.y, rect.width, rect.height)
    }
    readonly property bool verticalTools: imageBounds.width < 400 && imageBounds.height > 440
    readonly property bool smallImage: {
        const canvas = SpectacleCore.annotationDocument.canvasRect
        const scale = Math.min(Math.max(1, SpectacleCore.annotationDocument.imageDpr),
            contentLoader.width / Math.max(1, canvas.width))
        return canvas.height * scale < 200
    }

    function boundedPosition(position, extent, available) {
        return Math.max(overlayInset, Math.min(position, available - extent - overlayInset))
    }

    LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    // Primary actions stay visible; the original secondary actions remain
    // available in More rather than competing with the image being edited.
    component ViewerButton: Kos.LiquidActionButton {
        id: button
        property bool primaryAction: false
        property string symbol: ""
        materialForm: true
        flat: !primaryAction && !checked
        checked: action ? action.checked : false
        implicitHeight: QmlUtils.toolbarButtonHeight
        implicitWidth: iconOnly ? implicitHeight
            : Math.max(QmlUtils.toolbarButtonHeight, implicitContentWidth + leftPadding + rightPadding)
        iconOnly: display === QQC.AbstractButton.IconOnly
        iconSize: QmlUtils.toolbarIconSize
        icon.width: iconSize
        icon.height: iconSize
        icon.color: textColor
        highlighted: primaryAction
        leftPadding: iconOnly ? (height - iconSize) / 2 : 12
        rightPadding: leftPadding
        reduceMotion: Settings.reduceUiMotion
        enabled: !header.captureBusy && (!action || action.enabled)
        display: root.width < 720 ? QQC.AbstractButton.IconOnly : QQC.AbstractButton.TextBesideIcon
        contentItem: SymbolLabel {
            control: button
            symbol: button.symbol
                || (button.action && button.action.symbol !== undefined ? button.action.symbol : "")
                || button.icon.name
            color: button.textColor
            iconSize: button.iconSize
        }
        QQC.ToolTip.text: text
        QQC.ToolTip.visible: (hovered || pressed) && iconOnly
        QQC.ToolTip.delay: Kirigami.Units.toolTipDelay
    }

    Kos.KosWindowResizeHandles {
        parent: root
        targetWindow: root.hostWindow
        LayoutMirroring.enabled: false
    }

    header: QQC.ToolBar {
        id: header
        objectName: "nkshotViewerTitleBar"
        readonly property bool captureBusy: SpectacleCore.videoPlatform.isRecording
            || SpectacleCore.videoPlatform.recordingState === VideoPlatform.Rendering
        implicitHeight: root.titleBarHeight
        height: implicitHeight
        leftPadding: 12
        rightPadding: 10.5
        topPadding: 10
        bottomPadding: 10
        background: Rectangle { color: Kos.AppTheme.contentSecondaryBackground }
        // Keep input above the toolbar itself and below its interactive controls.
        MouseArea {
            parent: header
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            onDoubleClicked: root.toggleMaximized()
            DragHandler {
                target: null
                acceptedButtons: Qt.LeftButton
                onActiveChanged: if (active) {
                    acceptedButtons = root.hostWindow.startSystemMove() ? Qt.NoButton : Qt.LeftButton
                } else {
                    acceptedButtons = Qt.LeftButton
                }
            }
        }
        contentItem: RowLayout {
            z: 1
            spacing: 6
            ViewerButton {
                symbol: "camera-photo"
                display: root.width >= 960 ? QQC.AbstractButton.TextBesideIcon : QQC.AbstractButton.IconOnly
                text: i18nc("@action", "New Screenshot")
                Accessible.role: Accessible.ButtonMenu
                onClicked: ScreenshotModeMenu.popup(this)
            }
            QQC.ToolSeparator { Layout.preferredHeight: 24 }
            TtToolButton {
                objectName: "nkshotToggleEditing"
                visible: action.enabled
                display: QQC.AbstractButton.IconOnly
                action: EditAction {}
                QQC.ToolTip.text: checked ? i18nc("@action", "Hide editing tools") : i18nc("@action", "Show editing tools")
            }
            RowLayout {
                objectName: "nkshotHistoryControls"
                visible: root.editingImage
                spacing: 2
                Layout.leftMargin: 6
                TtToolButton {
                    objectName: "nkshotUndo"
                    display: QQC.AbstractButton.IconOnly
                    text: i18nc("@action", "Undo")
                    symbol: "edit-undo"
                    enabled: SpectacleCore.annotationDocument.undoStackDepth > 0
                    autoRepeat: true
                    onClicked: SpectacleCore.annotationDocument.undo()
                }
                TtToolButton {
                    objectName: "nkshotRedo"
                    display: QQC.AbstractButton.IconOnly
                    text: i18nc("@action", "Redo")
                    symbol: "edit-redo"
                    enabled: SpectacleCore.annotationDocument.redoStackDepth > 0
                    autoRepeat: true
                    onClicked: SpectacleCore.annotationDocument.redo()
                }
            }
            Item { Layout.fillWidth: true; Layout.minimumWidth: 8 }
            ColumnLayout {
                visible: root.width >= 960
                Layout.maximumWidth: Math.max(0, root.width - 720)
                spacing: 2
                QQC.Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: SpectacleCore.videoMode ? i18nc("@title", "Recording") : i18nc("@title", "Screenshot")
                    font.weight: Font.DemiBold
                    color: Kos.AppTheme.paletteText
                }
                QQC.Label {
                    Layout.alignment: Qt.AlignHCenter
                    visible: !SpectacleCore.videoMode
                    text: Math.round(SpectacleCore.annotationDocument.canvasRect.width * SpectacleCore.annotationDocument.imageDpr) + " × "
                        + Math.round(SpectacleCore.annotationDocument.canvasRect.height * SpectacleCore.annotationDocument.imageDpr)
                    font.pixelSize: 10
                    color: Kos.AppTheme.mutedText
                }
            }
            Item { Layout.fillWidth: true; Layout.minimumWidth: 8 }
            Loader {
                id: zoomLoader
                objectName: "nkshotZoomControl"
                visible: !SpectacleCore.videoMode
                active: visible
                sourceComponent: zoomControlsComponent
                Layout.rightMargin: 8
            }
            ViewerButton {
                visible: action.enabled
                action: CopyImageAction {}
            }
            ViewerButton {
                visible: action.enabled
                primaryAction: true
                action: SaveAction {}
            }
            ViewerButton {
                id: moreButton
                objectName: "nkshotViewerMore"
                text: i18nc("@action", "More")
                symbol: "view-more-symbolic"
                display: QQC.AbstractButton.IconOnly
                Accessible.role: Accessible.ButtonMenu
                onClicked: moreMenu.popup(moreButton, moreButton.width - moreMenu.width, moreButton.height + 6)
                GlassMenu {
                    id: moreMenu
                    objectName: "nkshotMoreMenu"
                    width: Math.min(240, root.width - 32)
                    y: moreButton.height + 6
                    delegate: GlassMenuItem {
                        objectName: subMenu ? "nkshotTask_" + subMenu.objectName : ""
                        visible: !subMenu || !subMenu.imageOnly || !SpectacleCore.videoMode
                        height: visible ? implicitHeight : 0
                        icon.color: palette.buttonText
                    }
                    GlassMenuItem {
                        objectName: "nkshotOpenMediaAction"
                        text: i18nc("@action", "Open…")
                        icon.name: "document-open"
                        enabled: contextWindow.canOpenMedia
                        onTriggered: contextWindow.openMedia()
                    }
                    GlassMenuItem {
                        objectName: "nkshotRecentMediaAction"
                        text: i18nc("@action", "Recent files…")
                        icon.name: "document-open-recent"
                        enabled: contextWindow.canOpenMedia
                        onTriggered: recentPopup.open()
                    }
                    GlassMenuItem {
                        objectName: "nkshotClipboardImageAction"
                        text: i18nc("@action", "Edit clipboard image…")
                        icon.name: "edit-paste"
                        enabled: contextWindow.canOpenMedia
                        onTriggered: contextWindow.editClipboardImage()
                    }
                    TaskMenu {
                        id: sharingMenu
                        objectName: "nkshotSharingMenu"
                        imageOnly: false
                        title: SpectacleCore.videoMode ? i18nc("@action:menu", "Recording file") : i18nc("@action:menu", "Share a copy")
                        icon.name: "document-share"
                        GlassMenuItem {
                            action: SaveAsAction {}
                            icon.color: palette.buttonText
                        }
                        GlassMenuItem {
                            visible: SpectacleCore.videoMode
                            height: visible ? implicitHeight : 0
                            action: CopyLocationAction {}
                            icon.color: palette.buttonText
                        }
                        GlassMenuItem {
                            visible: !SpectacleCore.videoMode
                            height: visible ? implicitHeight : 0
                            text: i18nc("@action", "Export")
                            icon.name: "document-share"
                            icon.color: palette.buttonText
                            onTriggered: Qt.callLater(function() { ExportMenu.popup(moreButton) })
                        }
                        MenuSeparator { visible: !SpectacleCore.videoMode; height: visible ? implicitHeight : 0 }
                        GlassMenuItem {
                            objectName: "nkshotBeautifyImageAction"
                            visible: !SpectacleCore.videoMode
                            height: visible ? implicitHeight : 0
                            text: contextWindow.resizingImage ? i18nc("@info", "Preparing copy…") : i18nc("@action", "Frame, background and watermark…")
                            icon.name: "color-management"
                            icon.color: palette.buttonText
                            enabled: !contextWindow.resizingImage
                            onTriggered: { if (contextWindow.beginPresentationPreview()) beautifyPopup.open() }
                        }
                        GlassMenuItem {
                            objectName: "nkshotResizeImageAction"
                            visible: !SpectacleCore.videoMode
                            height: visible ? implicitHeight : 0
                            text: contextWindow.resizingImage ? i18nc("@info", "Preparing copy…") : i18nc("@action", "Size and orientation…")
                            icon.name: "transform-scale"
                            icon.color: palette.buttonText
                            enabled: !contextWindow.resizingImage
                            onTriggered: { resizePopup.sourceSize = contextWindow.imageSnapshotSize(); resizePopup.open() }
                        }
                    }
                    TaskMenu {
                        id: ocrLanguageMenu
                        objectName: "nkshotOcrLanguageMenu"
                        title: i18nc("@action:menu", "Read from image")
                        icon.name: "document-scan"
                        property var languages: SpectacleCore.ocrAvailable ? SpectacleCore.ocrAvailableLanguages() : ({})
                        GlassMenuItem {
                            action: OcrAction {}
                            text: i18nc("@action", "Extract text")
                            icon.color: palette.buttonText
                        }
                        MenuSeparator {}
                        Instantiator {
                            model: Object.keys(ocrLanguageMenu.languages).filter(function(code) { return code !== "osd" }).sort()
                            delegate: GlassMenuItem {
                                required property string modelData
                                objectName: "nkshotOcrLanguage_" + modelData
                                text: ocrLanguageMenu.languages[modelData]
                                enabled: SpectacleCore.ocrStatus !== 1
                                onTriggered: SpectacleCore.startOcrExtraction(modelData)
                            }
                            onObjectAdded: (index, object) => ocrLanguageMenu.insertItem(index + 2, object)
                            onObjectRemoved: (index, object) => ocrLanguageMenu.removeItem(object)
                        }
                        MenuSeparator {}
                        GlassMenuItem {
                            objectName: "nkshotScanCodeAction"
                            text: contextWindow.scanningCode ? i18nc("@info", "Reading code…") : i18nc("@action", "Read QR or barcode")
                            icon.name: "view-barcode-qr"
                            icon.color: palette.buttonText
                            enabled: !contextWindow.scanningCode
                            onTriggered: contextWindow.scanImageCode()
                        }
                        QQC.Label {
                            visible: !SpectacleCore.ocrAvailable
                            width: 228
                            height: visible ? implicitHeight : 0
                            padding: visible ? 10 : 0
                            text: i18nc("@info", "Local OCR is unavailable. Check that Tesseract and its language data are installed.")
                            wrapMode: Text.Wrap
                            color: Kos.AppTheme.mutedText
                        }
                    }
                    GlassMenuItem {
                        objectName: "nkshotCombineImagesAction"
                        visible: !SpectacleCore.videoMode
                        height: visible ? implicitHeight : 0
                        text: contextWindow.combiningImages ? i18nc("@info", "Combining…") : i18nc("@action", "Combine images…")
                        icon.name: "view-preview"
                        icon.color: palette.buttonText
                        enabled: !contextWindow.combiningImages
                        onTriggered: combinePopup.open()
                    }
                    TaskMenu {
                        objectName: "nkshotPinsNotesMenu"
                        imageOnly: false
                        title: i18nc("@action:menu", "Pins and notes")
                        icon.name: "window-pin"
                    GlassMenuItem {
                        objectName: "nkshotPinImageAction"
                        visible: !SpectacleCore.videoMode
                        height: visible ? implicitHeight : 0
                        text: i18nc("@action", "Pin image")
                        icon.name: "window-pin"
                        icon.color: palette.buttonText
                        onTriggered: contextWindow.pinImage()
                    }
                        GlassMenuItem {
                            objectName: "nkshotQuickNoteAction"
                            text: i18nc("@action", "Quick note…")
                            icon.name: "document-edit"
                            icon.color: palette.buttonText
                            onTriggered: contextWindow.openQuickNote()
                        }
                    }
                    MenuSeparator {}
                    GlassMenuItem {
                        objectName: "nkshotColorPickerAction"
                        text: i18nc("@action", "Pick a color…")
                        icon.name: "color-picker"
                        icon.color: palette.buttonText
                        onTriggered: contextWindow.openColorPicker()
                    }
                    GlassMenuItem {
                        objectName: "nkshotScrollPreviewEntry"
                        visible: !SpectacleCore.videoMode
                        height: visible ? implicitHeight : 0
                        text: i18nc("@action", "Manual scrolling capture (preview)…")
                        icon.name: "view-fullscreen"
                        onTriggered: contextWindow.openScrollPreview()
                    }
                    GlassMenuItem {
                        enabled: SpectacleCore.videoPlatform.supportedRecordingModes !== VideoPlatform.NoRecordingModes
                        text: i18nc("@action", "New Recording")
                        icon.name: "media-record"
                        icon.color: palette.buttonText
                        onTriggered: Qt.callLater(function() { RecordingModeMenu.popup(moreButton) })
                    }
                    GlassMenuItem {
                        text: i18nc("@action", "Options")
                        icon.name: "configure"
                        icon.color: palette.buttonText
                        onTriggered: Qt.callLater(function() { OptionsMenu.popup(moreButton) })
                    }
                }
            }
            QQC.ToolSeparator { Layout.preferredHeight: 18 }
            ViewerWindowControls {
                id: windowControls
                targetWindow: root.hostWindow
                actions: ["maximize", "minimize", "close"]
                maximizeMode: true
                circleSize: 14
                buttonWidth: 22
                buttonHeight: 22
                spacing: 6
                Layout.alignment: Qt.AlignVCenter
                LayoutMirroring.enabled: false
            }
        }
    }

    // Needed for scrolling via keyboard input
    Keys.priority: Keys.AfterItem
    Keys.enabled: !SpectacleCore.videoMode && contentLoader.item !== null
    Keys.forwardTo: contentLoader.item

    InlineMessageList {
        id: inlineMessageList
        // Notifications overlay the workbench without changing image zoom/geometry.
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        width: Math.min(520, parent.width - 32)
        maximumHeight: Math.max(80, parent.height * 0.4)
        z: 3
    }

    component EditorSurface: Kos.LiquidPanelSurface {
        // Dense frost over the image, with one contour for both body and shadow.
        backdrop: editorBackdrop
        reduceTransparency: Kos.AppTheme.reduceTransparency || !Settings.useLiquidMaterial
        cornerRadius: 14
        tintColor: Kos.AppTheme.contentBackground
        tintStrength: 0.88
        blurStrength: 1.0
        refractionStrength: 0.5
        materialOpacity: 1
        shadowEnabled: false
        layer.enabled: GraphicsInfo.api !== GraphicsInfo.Software
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowOpacity: 0.10
            shadowBlur: 0.25
            shadowVerticalOffset: 2
            blurMax: 12
            autoPaddingEnabled: true
        }
    }

    component FloatingPanel: QQC.Pane {
        padding: 10
        z: 2
        background: EditorSurface {}
    }

    FloatingPanel {
        id: annotationsToolBar
        objectName: "nkshotFloatingTools"
        x: root.boundedPosition(root.verticalTools
            ? root.imageBounds.x >= width + root.overlayInset * 2
                ? root.imageBounds.x - width - root.overlayInset
                : root.imageBounds.x + root.overlayInset
            : root.imageBounds.x + (root.imageBounds.width - width) / 2, width, parent.width)
        y: root.boundedPosition(root.verticalTools
            ? root.imageBounds.y + (root.imageBounds.height - height) / 2
            : root.smallImage ? parent.height - height - root.canvasInset
            : root.imageBounds.y + root.imageBounds.height - height - root.overlayInset, height, parent.height)
        visible: root.editingImage
        contentItem: GridLayout {
            columns: root.verticalTools ? 1 : 3
            rowSpacing: 10
            columnSpacing: 10
            AnnotationsToolBarContents {
                editorDocument: SpectacleCore.annotationDocument
                id: annotationsToolBarContents
                displayMode: QQC.AbstractButton.IconOnly
                // Keep one dimension unlimited while switching orientation.
                flow: Grid.LeftToRight
                columns: root.verticalTools ? 1 : 8
                rows: -1
                spacing: 6
                paletteMode: true
                essentialsOnly: true
                showUndoRedo: false
                showNoneButton: true
            }
            Rectangle {
                color: Qt.alpha(Kos.AppTheme.paletteText, 0.14)
                Layout.preferredWidth: root.verticalTools ? 20 : 1
                Layout.preferredHeight: root.verticalTools ? 1 : 20
                Layout.alignment: Qt.AlignCenter
            }
            TtToolButton {
                id: propertiesButton
                objectName: "nkshotToolProperties"
                implicitWidth: QmlUtils.toolbarButtonHeight
                display: QQC.AbstractButton.IconOnly
                text: i18nc("@action", "Tool properties")
                icon.name: "configure"
                enabled: propertyContents.options !== AnnotationTool.NoOptions
                onClicked: propertiesPopup.open()
                QQC.Popup {
                    id: propertiesPopup
                    palette.windowText: Kos.AppTheme.paletteText
                    palette.text: Kos.AppTheme.paletteText
                    palette.buttonText: Kos.AppTheme.paletteText
                    palette.base: Kos.AppTheme.contentBackground
                    palette.button: Kos.AppTheme.controlBackground
                    palette.light: Kos.AppTheme.border
                    palette.mid: Kos.AppTheme.border
                    objectName: "nkshotPropertiesPopup"
                    parent: root.contentItem
                    readonly property real edgeMargin: root.overlayInset
                    readonly property real anchorGap: 12
                    width: Math.min(root.availableWidth - 2 * edgeMargin,
                        Math.max(propertiesHeading.implicitWidth, propertyContents.implicitWidth) + leftPadding + rightPadding)
                    height: Math.min(root.contentItem.height - 2 * edgeMargin,
                        propertiesHeading.implicitHeight + propertiesLayout.spacing + propertyContents.implicitHeight + topPadding + bottomPadding)
                    x: root.boundedPosition(root.verticalTools ? annotationsToolBar.x + annotationsToolBar.width + anchorGap
                        : annotationsToolBar.x + annotationsToolBar.width - width, width, root.contentItem.width)
                    y: root.boundedPosition(root.verticalTools ? annotationsToolBar.y + (annotationsToolBar.height - height) / 2
                        : annotationsToolBar.y - height - anchorGap, height, root.contentItem.height)
                    padding: 16
                    onClosed: propertyContents.flushPendingChanges()
                    background: EditorSurface {}
                    contentItem: ColumnLayout {
                        id: propertiesLayout
                        spacing: 12
                        RowLayout {
                            id: propertiesHeading
                            Layout.fillWidth: true
                            QQC.Label {
                                Layout.fillWidth: true
                                text: i18nc("@title", "Appearance")
                                font.weight: Font.DemiBold
                                color: Kos.AppTheme.paletteText
                            }
                            ViewerButton {
                                objectName: "nkshotCloseProperties"
                                implicitWidth: QmlUtils.toolbarButtonHeight
                                leftPadding: 4
                                rightPadding: 4
                                display: QQC.AbstractButton.IconOnly
                                icon.name: "dialog-close"
                                text: i18nc("@action", "Close tool properties")
                                onClicked: propertiesPopup.close()
                            }
                        }
                        QQC.ScrollView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            AnnotationOptionsToolBarContents {
                                editorDocument: SpectacleCore.annotationDocument
                                id: propertyContents
                                verticalLayout: true
                            }
                        }
                    }
                }
            }
        }
    }

    // Only the ordinary workbench and image are sampled. Floating controls are
    // siblings, so neither the dock nor its glass can feed back into the texture.
    Item {
        id: workbenchScene
        objectName: "nkshotWorkbenchScene"
        anchors.fill: parent
        Rectangle { anchors.fill: parent; color: Kos.AppTheme.contentSecondaryBackground }
        Loader {
            id: contentLoader
            objectName: "nkshotWorkbench"
            anchors.fill: parent
            anchors.margins: root.canvasInset
            // Tiny images need a real gap above the dock instead of an overlay.
            // Keep it stable when tools are hidden so the image does not jump.
            anchors.bottomMargin: !SpectacleCore.videoMode && root.smallImage
                ? root.canvasInset + annotationsToolBar.height + root.overlayInset : root.canvasInset
            sourceComponent: SpectacleCore.videoMode ? recordingViewComponent : screenshotViewComponent
            Component {
                id: recordingViewComponent
                RecordingView {}
            }
            Component {
                id: screenshotViewComponent
                ScreenshotView {}
            }
        }
    }
    Kos.LiquidBackdrop {
        id: editorBackdrop
        objectName: "nkshotEditorBackdrop"
        sourceItem: workbenchScene
        registerAsDefault: false
        active: Settings.useLiquidMaterial && !Kos.AppTheme.reduceTransparency
            && root.editingImage && GraphicsInfo.api !== GraphicsInfo.Software
        blur: 0.35
        maximumTextureSize: 2048
    }

    Component {
        id: zoomControlsComponent
        TtToolButton {
                    id: zoomButton
                    objectName: "nkshotZoomMenu"
                    implicitWidth: 56
                    display: QQC.AbstractButton.TextOnly
                    text: Math.round(contentLoader.item.currentZoom * 100) + "%"
                    Accessible.name: i18nc("@action", "Image zoom")
                    Accessible.role: Accessible.ButtonMenu
                    onClicked: zoomPopup.open()
                    QQC.Popup {
                        id: zoomPopup
                        parent: root.contentItem
                        objectName: "nkshotZoomPopup"
                        width: Math.min(228, root.availableWidth - 24)
                        x: root.boundedPosition(zoomLoader.mapToItem(root.contentItem, 0, 0).x + zoomLoader.width - width,
                            width, root.contentItem.width)
                        y: root.canvasInset
                        padding: 16
                        background: EditorSurface {}
                        contentItem: ColumnLayout {
                            spacing: 8
                            RowLayout {
                                Layout.fillWidth: true
                                TtToolButton {
                                    objectName: "nkshotCompactZoomOut"
                                    display: QQC.AbstractButton.IconOnly
                                    text: i18nc("@action", "Zoom out")
                                    symbol: "zoom-out"
                                    enabled: contentLoader.item.currentZoom > contentLoader.item.minZoom
                                    onClicked: contentLoader.item.zoomOut()
                                }
                                QQC.Label {
                                    Layout.fillWidth: true
                                    horizontalAlignment: Text.AlignHCenter
                                    text: zoomButton.text
                                    color: Kos.AppTheme.paletteText
                                }
                                TtToolButton {
                                    objectName: "nkshotCompactZoomIn"
                                    display: QQC.AbstractButton.IconOnly
                                    text: i18nc("@action", "Zoom in")
                                    symbol: "zoom-in"
                                    enabled: contentLoader.item.currentZoom < contentLoader.item.maxZoom
                                    onClicked: contentLoader.item.zoomIn()
                                }
                            }
                            ViewerButton {
                                objectName: "nkshotFitImage"
                                text: i18nc("@action", "Fit to view")
                                display: QQC.AbstractButton.TextOnly
                                Layout.fillWidth: true
                                onClicked: { contentLoader.item.fitToView(); zoomPopup.close() }
                            }
                            ViewerButton {
                                text: i18nc("@action", "Actual size")
                                display: QQC.AbstractButton.TextOnly
                                Layout.fillWidth: true
                                onClicked: { contentLoader.item.zoomToPercent(1); zoomPopup.close() }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                Kos.LiquidTextField {
                                    id: zoomEditor
                                    objectName: "nkshotZoomInput"
                                    Layout.fillWidth: true
                                    implicitHeight: QmlUtils.toolbarButtonHeight
                                    specificationForm: true
                                    horizontalAlignment: TextInput.AlignHCenter
                                    inputMethodHints: Qt.ImhDigitsOnly
                                    readonly property var numberLocale: Qt.locale()
                                    validator: IntValidator {
                                        bottom: Math.ceil(contentLoader.item.minZoom * 100)
                                        top: Math.floor(contentLoader.item.maxZoom * 100)
                                        locale: zoomEditor.numberLocale.name
                                    }
                                    Binding on text {
                                        when: !zoomEditor.activeFocus
                                        value: Number(Math.round(contentLoader.item.currentZoom * 100)).toLocaleString(zoomEditor.numberLocale, 'f', 0)
                                    }
                                    onEditingFinished: {
                                        if (acceptableInput)
                                            contentLoader.item.zoomToPercent(Number.fromLocaleString(numberLocale, text) / 100)
                                        text = Number(Math.round(contentLoader.item.currentZoom * 100)).toLocaleString(numberLocale, 'f', 0)
                                    }
                                    Accessible.name: i18nc("@label", "Custom zoom percentage")
                                }
                                QQC.Label { text: zoomEditor.numberLocale.percent; color: Kos.AppTheme.mutedText }
                            }
                        }
                    }
        }
    }

    BeautifyImagePopup {
        id: beautifyPopup
        previewModel: contextWindow.presentationPreview
    }

    ImageCombinePopup {
        id: combinePopup
        busy: contextWindow.combiningImages
        onChooseImages: (keepAllPixels, horizontal) => contextWindow.combineImages(keepAllPixels, horizontal)
    }

    ResizeImagePopup {
        id: resizePopup
        onCreateCopy: (pixelWidth, quarterTurns, mirror) => contextWindow.resizeImageCopy(pixelWidth, quarterTurns, mirror)
    }

    Shortcut {
        enabled: contextWindow.annotating && !SpectacleCore.videoMode && contentLoader.item !== null
        sequences: [StandardKey.ZoomIn]
        onActivated: contentLoader.item.zoomIn()
    }
    Shortcut {
        enabled: contextWindow.annotating && !SpectacleCore.videoMode && contentLoader.item !== null
        sequences: [StandardKey.ZoomOut]
        onActivated: contentLoader.item.zoomOut()
    }
    // FIXME: This shortcut only exists here because spectacle interprets "Ctrl+Shift+,"
    // as "Ctrl+Shift+<" for some reason unless we use a QML Shortcut.
    Shortcut {
        sequences: [StandardKey.Preferences]
        onActivated: contextWindow.showPreferencesDialog()
    }

    RecentMediaPopup { id: recentPopup; parent: QQC.Overlay.overlay; sceneSource: root }

    MediaDropTarget {
        id: mediaDrop
        anchors.fill: parent
        z: 100
        enabled: contextWindow.canDropMedia && !moreMenu.visible && !propertiesPopup.visible
            && !beautifyPopup.visible && !resizePopup.visible
            && !combinePopup.visible && !recentPopup.visible
        canAcceptCallback: urls => contextWindow.canAcceptMediaDrop(urls)
        openCallback: urls => contextWindow.openDroppedMedia(urls)
        Connections {
            target: SpectacleCore
            function onCurrentVideoChanged() { mediaDrop.clear() }
            function onVideoModeChanged() { mediaDrop.clear() }
        }
        Connections {
            target: SpectacleCore.annotationDocument
            function onRepaintNeeded(types) { mediaDrop.clear() }
        }
    }

}
