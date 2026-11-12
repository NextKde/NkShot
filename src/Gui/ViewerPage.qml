/* SPDX-FileCopyrightText: 2022 Noah Davis <noahadvs@gmail.com>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
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

    // Used in ViewerWindow::setMode()
    readonly property real minimumWidth: Math.max(
        header.implicitWidth,
        annotationsToolBar.implicitWidth + 48,
        480 // leave some room for content if necessary
    )
    readonly property real minimumHeight: header.implicitHeight
        + Math.max(annotationsToolBar.implicitHeight,
                   footerLoader.implicitHeight)

    LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    // Primary actions stay visible; the original secondary actions remain
    // available in More rather than competing with the image being edited.
    component ViewerButton: Kos.LiquidActionButton {
        id: button
        property bool primaryAction: false
        property string symbol: ""
        materialForm: true
        implicitHeight: QmlUtils.toolbarButtonHeight
        implicitWidth: iconOnly ? implicitHeight
            : Math.max(QmlUtils.toolbarButtonHeight, implicitContentWidth + leftPadding + rightPadding)
        iconOnly: display === QQC.AbstractButton.IconOnly
        iconSize: QmlUtils.toolbarIconSize
        icon.width: iconSize
        icon.height: iconSize
        icon.color: textColor
        highlighted: primaryAction
        leftPadding: iconOnly ? 0 : 12
        rightPadding: leftPadding
        reduceMotion: Settings.reduceUiMotion
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

    header: QQC.ToolBar {
        id: header
        visible: !SpectacleCore.videoPlatform.isRecording
            && SpectacleCore.videoPlatform.recordingState !== VideoPlatform.Rendering
        height: visible ? implicitHeight : 0
        leftPadding: 12
        rightPadding: 12
        topPadding: 10
        bottomPadding: 10
        background: Rectangle { color: Kos.AppTheme.contentSecondaryBackground }
        contentItem: RowLayout {
            spacing: 6
            ViewerButton {
                symbol: "camera-photo"
                text: i18nc("@action", "New Screenshot")
                Accessible.role: Accessible.ButtonMenu
                onClicked: ScreenshotModeMenu.popup(this)
            }
            QQC.ToolSeparator { Layout.preferredHeight: 24 }
            ViewerButton {
                visible: action.enabled
                action: EditAction {}
            }
            Item { Layout.fillWidth: true; Layout.minimumWidth: 12 }
            ColumnLayout {
                visible: root.width >= 800
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
                    text: Math.round(SpectacleCore.annotationDocument.canvasRect.width) + " × "
                        + Math.round(SpectacleCore.annotationDocument.canvasRect.height)
                    font.pixelSize: 12
                    color: Kos.AppTheme.mutedText
                }
            }
            Item { Layout.fillWidth: true; Layout.minimumWidth: 12 }
            UndoRedoGroup {
                editorDocument: SpectacleCore.annotationDocument
                visible: contextWindow.annotating
                buttonHeight: QmlUtils.toolbarButtonHeight
                animationsEnabled: !Settings.reduceUiMotion
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
                            text: contextWindow.resizingImage ? i18nc("@info", "Preparing copy…") : i18nc("@action", "Background and watermark…")
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

    QQC.Pane { // parent is contentItem
        id: annotationsToolBar
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: footerLoader.top
        anchors.bottomMargin: 16
        visible: contextWindow.annotating && !SpectacleCore.videoMode
        z: 2
        leftPadding: 8
        rightPadding: 8
        topPadding: 6
        bottomPadding: 6
        contentItem: RowLayout {
            spacing: 6
            AnnotationsToolBarContents {
                editorDocument: SpectacleCore.annotationDocument
                id: annotationsToolBarContents
                displayMode: QQC.AbstractButton.IconOnly
                flow: Grid.LeftToRight
                columns: -1
                rows: 1
                spacing: 6
                paletteMode: true
                essentialsOnly: true
                showUndoRedo: false
                showNoneButton: true
            }
            QQC.ToolSeparator { Layout.preferredHeight: 24 }
            ViewerButton {
                id: propertiesButton
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
                    readonly property real edgeMargin: 16
                    readonly property real anchorGap: 12
                    width: Math.min(root.availableWidth - 2 * edgeMargin,
                        Math.max(propertiesHeading.implicitWidth, propertyContents.implicitWidth) + leftPadding + rightPadding)
                    height: Math.min(propertiesButton.mapToItem(root.contentItem, 0, 0).y - edgeMargin - anchorGap,
                        propertiesHeading.implicitHeight + propertiesLayout.spacing + propertyContents.implicitHeight + topPadding + bottomPadding)
                    x: Math.max(edgeMargin - propertiesButton.mapToItem(root.contentItem, 0, 0).x, propertiesButton.width - width)
                    y: -height - anchorGap
                    padding: 12
                    onClosed: propertyContents.flushPendingChanges()
                    background: Kos.LiquidPanelSurface {
                        backdrop: editorBackdrop
                        reduceTransparency: Kos.AppTheme.reduceTransparency || !Settings.useLiquidMaterial
                        tintColor: Kos.AppTheme.contentBackground
                        cornerRadius: 14
                    }
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
        background: Kos.LiquidPanelSurface {
            objectName: "nkshotEditorMaterial"
            backdrop: editorBackdrop
            reduceTransparency: Kos.AppTheme.reduceTransparency || !Settings.useLiquidMaterial
            cornerRadius: 14
            tintColor: Kos.AppTheme.contentBackground
            darkAppearance: Kos.AppTheme.dark
            materialOpacity: 0.95
        }
    }

    Kirigami.Separator { // parent is contentItem
        id: separator
        visible: false
        anchors.left: annotationsToolBar.right
        anchors.leftMargin: 16
        opacity: 0
        anchors.top: parent.top
        anchors.bottom: parent.bottom
    }

    // Only the ordinary workbench and image are sampled. Floating controls are
    // siblings, so neither the dock nor its glass can feed back into the texture.
    Item {
        id: workbenchScene
        objectName: "nkshotWorkbenchScene"
        anchors.fill: parent
        Rectangle { anchors.fill: parent; color: Kos.AppTheme.contentSecondaryBackground }
    Loader { // parent is contentItem
        id: contentLoader
        objectName: "nkshotWorkbench"
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            bottom: parent.bottom
            topMargin: 24
            leftMargin: 24
            rightMargin: 24
            bottomMargin: annotationsToolBar.visible
                ? annotationsToolBar.height + footerLoader.height + 40 : 24
        }
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
        active: Settings.useLiquidMaterial && !SpectacleCore.videoMode
            && contextWindow.annotating && GraphicsInfo.api !== GraphicsInfo.Software
        blur: 0.35
        maximumTextureSize: 2048
    }

    Loader {
        id: footerLoader
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: contextWindow.annotating && !SpectacleCore.videoMode
        active: visible
        sourceComponent: QQC.ToolBar { // parent is contentItem
            position: QQC.ToolBar.Footer
            leftPadding: 12
            rightPadding: 16
            topPadding: 10
            bottomPadding: 10
            background: Rectangle { color: Kos.AppTheme.contentSecondaryBackground }
            contentHeight: QmlUtils.toolbarButtonHeight
            contentItem: RowLayout {
                spacing: Kirigami.Units.mediumSpacing
                Item { Layout.fillWidth: true }
                RowLayout {
                    id: zoomControls
                    spacing: 2
                    ViewerButton {
                        implicitWidth: QmlUtils.toolbarButtonHeight
                        leftPadding: 6
                        rightPadding: 6
                        display: QQC.AbstractButton.IconOnly
                        text: i18nc("@action", "Zoom out")
                        icon.name: "zoom-out"
                        enabled: contentLoader.item.currentZoom > contentLoader.item.minZoom
                        onClicked: contentLoader.item.zoomOut()
                    }
                    ViewerButton {
                        id: zoomButton
                        implicitWidth: 64
                        display: QQC.AbstractButton.TextOnly
                        text: Math.round(contentLoader.item.currentZoom * 100) + "%"
                        Accessible.name: i18nc("@action", "Image zoom")
                        onClicked: zoomPopup.open()
                        QQC.Popup {
                            id: zoomPopup
                            palette.windowText: Kos.AppTheme.paletteText
                            palette.text: Kos.AppTheme.paletteText
                            palette.buttonText: Kos.AppTheme.paletteText
                            palette.base: Kos.AppTheme.contentBackground
                            palette.button: Kos.AppTheme.controlBackground
                            palette.light: Kos.AppTheme.border
                            palette.mid: Kos.AppTheme.border
                            objectName: "nkshotZoomPopup"
                            x: zoomButton.width - width
                            y: -height - 8
                            padding: 12
                            background: Kos.KosSurface {
                                fillColor: Kos.AppTheme.contentBackground
                                radius: 14
                                elevation: 0.5
                            }
                            contentItem: ColumnLayout {
                                spacing: 8
                                Kos.LiquidActionButton {
                                    text: i18nc("@action", "Fit to view")
                                    implicitHeight: QmlUtils.toolbarButtonHeight
                                    materialForm: true
                                    Layout.fillWidth: true
                                    onClicked: {
                                        contentLoader.item.zoomToPercent(Math.min(1, contentLoader.item.fitZoom))
                                        zoomPopup.close()
                                    }
                                }
                                Kos.LiquidActionButton {
                                    text: i18nc("@action", "Actual size")
                                    implicitHeight: QmlUtils.toolbarButtonHeight
                                    materialForm: true
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
                    ViewerButton {
                        implicitWidth: QmlUtils.toolbarButtonHeight
                        leftPadding: 6
                        rightPadding: 6
                        display: QQC.AbstractButton.IconOnly
                        text: i18nc("@action", "Zoom in")
                        icon.name: "zoom-in"
                        enabled: contentLoader.item.currentZoom < contentLoader.item.maxZoom
                        onClicked: contentLoader.item.zoomIn()
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
