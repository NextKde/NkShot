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
    component TaskMenu: QQC.Menu {
        property bool imageOnly: true
        width: Math.min(260, root.width - 32)
        palette: moreMenu.palette
        padding: 6
        background: Kos.KosSurface { fillColor: Kos.AppTheme.contentBackground; radius: 14; elevation: 0.5 }
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
    component ViewerButton: TtToolButton {
        property bool primaryAction: false
        icon.width: 18
        icon.height: 18
        palette.buttonText: primaryAction ? Kos.AppTheme.accentText : Kos.AppTheme.paletteText
        implicitHeight: 36
        implicitWidth: Math.max(40, implicitContentWidth + 24)
        leftPadding: 12
        rightPadding: 12
        display: root.width < 720 ? QQC.AbstractButton.IconOnly : QQC.AbstractButton.TextBesideIcon
        background: Kos.KosSurface {
            fillColor: parent.primaryAction ? Kos.AppTheme.accent : parent.checked ? Kos.AppTheme.controlBackground
                : parent.hovered ? Kos.AppTheme.contentSecondaryBackground : "transparent"
            strokeColor: "transparent"
            strokeWidth: 0
            radius: 10
            elevation: 0
            hovered: parent.hovered
            pressed: parent.down
            focused: parent.visualFocus
            showInnerHighlight: false
        }
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
        background: Rectangle {
            color: Kos.AppTheme.contentSecondaryBackground
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: Kos.AppTheme.border
            }
        }
        contentItem: RowLayout {
            spacing: 6
            ViewerButton {
                icon.name: "camera-photo"
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
                    font.pixelSize: 11
                    color: Kos.AppTheme.mutedText
                }
            }
            Item { Layout.fillWidth: true; Layout.minimumWidth: 12 }
            UndoRedoGroup {
                editorDocument: SpectacleCore.annotationDocument
                visible: contextWindow.annotating
                buttonHeight: 36
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
                icon.name: "view-more-symbolic"
                display: QQC.AbstractButton.IconOnly
                Accessible.role: Accessible.ButtonMenu
                onClicked: moreMenu.popup(moreButton, moreButton.width - moreMenu.width, moreButton.height + 6)
                QQC.Menu {
                    id: moreMenu
                    objectName: "nkshotMoreMenu"
                    width: Math.min(240, root.width - 32)
                    padding: 6
                    y: moreButton.height + 6
                    palette.windowText: Kos.AppTheme.paletteText
                    palette.text: Kos.AppTheme.paletteText
                    palette.buttonText: Kos.AppTheme.paletteText
                    palette.base: Kos.AppTheme.contentBackground
                    palette.button: Kos.AppTheme.controlBackground
                    palette.light: Kos.AppTheme.border
                    palette.mid: Kos.AppTheme.border
                    delegate: QQC.MenuItem {
                        objectName: subMenu ? "nkshotTask_" + subMenu.objectName : ""
                        visible: !subMenu || !subMenu.imageOnly || !SpectacleCore.videoMode
                        height: visible ? implicitHeight : 0
                        opacity: enabled ? 1 : 0.38
                        icon.color: palette.buttonText
                    }
                    background: Kos.KosSurface { fillColor: Kos.AppTheme.contentBackground; radius: 14; elevation: 0.5 }
                    QQC.MenuItem {
                        objectName: "nkshotOpenMediaAction"
                        text: i18nc("@action", "Open…")
                        icon.name: "document-open"
                        enabled: contextWindow.canOpenMedia
                        onTriggered: contextWindow.openMedia()
                    }
                    QQC.MenuItem {
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
                        QQC.MenuItem {
                            action: SaveAsAction {}
                            icon.color: palette.buttonText
                        }
                        QQC.MenuItem {
                            visible: SpectacleCore.videoMode
                            height: visible ? implicitHeight : 0
                            action: CopyLocationAction {}
                            icon.color: palette.buttonText
                        }
                        QQC.MenuItem {
                            visible: !SpectacleCore.videoMode
                            height: visible ? implicitHeight : 0
                            text: i18nc("@action", "Export")
                            icon.name: "document-share"
                            icon.color: palette.buttonText
                            onTriggered: Qt.callLater(function() { ExportMenu.popup(moreButton) })
                        }
                        QQC.MenuSeparator { visible: !SpectacleCore.videoMode; height: visible ? implicitHeight : 0 }
                        QQC.MenuItem {
                            objectName: "nkshotBeautifyImageAction"
                            visible: !SpectacleCore.videoMode
                            height: visible ? implicitHeight : 0
                            text: contextWindow.resizingImage ? i18nc("@info", "Preparing copy…") : i18nc("@action", "Background and watermark…")
                            icon.name: "color-management"
                            icon.color: palette.buttonText
                            enabled: !contextWindow.resizingImage
                            onTriggered: { beautifyPopup.sourceSize = contextWindow.imageSnapshotSize(); beautifyPopup.open() }
                        }
                        QQC.MenuItem {
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
                        QQC.MenuItem {
                            action: OcrAction {}
                            text: i18nc("@action", "Extract text")
                            icon.color: palette.buttonText
                        }
                        QQC.MenuSeparator {}
                        Instantiator {
                            model: Object.keys(ocrLanguageMenu.languages).filter(function(code) { return code !== "osd" }).sort()
                            delegate: QQC.MenuItem {
                                required property string modelData
                                objectName: "nkshotOcrLanguage_" + modelData
                                text: ocrLanguageMenu.languages[modelData]
                                enabled: SpectacleCore.ocrStatus !== 1
                                onTriggered: SpectacleCore.startOcrExtraction(modelData)
                            }
                            onObjectAdded: (index, object) => ocrLanguageMenu.insertItem(index + 2, object)
                            onObjectRemoved: (index, object) => ocrLanguageMenu.removeItem(object)
                        }
                        QQC.MenuSeparator {}
                        QQC.MenuItem {
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
                    TaskMenu {
                        objectName: "nkshotCombineMenu"
                        title: i18nc("@action:menu", "Combine images")
                        icon.name: "view-preview"
                        QQC.MenuItem {
                            objectName: "nkshotCombineImagesAction"
                            text: contextWindow.combiningImages ? i18nc("@info", "Combining…") : i18nc("@action", "Match overlapping screenshots…")
                            icon.name: "view-preview"
                            icon.color: palette.buttonText
                            enabled: !contextWindow.combiningImages
                            onTriggered: contextWindow.combineImages()
                        }
                        QQC.MenuItem {
                            objectName: "nkshotStackImagesAction"
                            text: i18nc("@action", "Stack without overlap…")
                            icon.name: "view-sort-descending"
                            icon.color: palette.buttonText
                            enabled: !contextWindow.combiningImages
                            onTriggered: contextWindow.combineImages(true)
                        }
                    }
                    TaskMenu {
                        objectName: "nkshotPinsNotesMenu"
                        imageOnly: false
                        title: i18nc("@action:menu", "Pins and notes")
                        icon.name: "window-pin"
                    QQC.MenuItem {
                        objectName: "nkshotPinImageAction"
                        visible: !SpectacleCore.videoMode
                        height: visible ? implicitHeight : 0
                        text: i18nc("@action", "Pin image")
                        icon.name: "window-pin"
                        icon.color: palette.buttonText
                        onTriggered: contextWindow.pinImage()
                    }
                        QQC.MenuItem {
                            objectName: "nkshotQuickNoteAction"
                            text: i18nc("@action", "Quick note…")
                            icon.name: "document-edit"
                            icon.color: palette.buttonText
                            onTriggered: contextWindow.openQuickNote()
                        }
                    }
                    QQC.MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Kos.AppTheme.border } }
                    QQC.MenuItem {
                        objectName: "nkshotColorPickerAction"
                        text: i18nc("@action", "Pick a color…")
                        icon.name: "color-picker"
                        icon.color: palette.buttonText
                        onTriggered: contextWindow.openColorPicker()
                    }
                    QQC.MenuItem {
                        objectName: "nkshotScrollPreviewEntry"
                        visible: !SpectacleCore.videoMode
                        height: visible ? implicitHeight : 0
                        text: i18nc("@action", "Manual scrolling capture (preview)…")
                        icon.name: "view-fullscreen"
                        onTriggered: contextWindow.openScrollPreview()
                    }
                    QQC.MenuItem {
                        enabled: SpectacleCore.videoPlatform.supportedRecordingModes !== VideoPlatform.NoRecordingModes
                        opacity: enabled ? 1 : 0.38
                        text: i18nc("@action", "New Recording")
                        icon.name: "media-record"
                        icon.color: palette.buttonText
                        onTriggered: Qt.callLater(function() { RecordingModeMenu.popup(moreButton) })
                    }
                    QQC.MenuItem {
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
        leftPadding: 10
        rightPadding: 10
        topPadding: 7
        bottomPadding: 7
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
                implicitWidth: 36
                leftPadding: 8
                rightPadding: 8
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
                    width: Math.min(root.width - 32, Math.max(300, propertyContents.implicitWidth + 32))
                    height: Math.min(root.height - 120, propertyContents.implicitHeight + 72)
                    x: Math.max(16 - propertiesButton.mapToItem(root, 0, 0).x, propertiesButton.width - width)
                    y: -height - 12
                    padding: 16
                    onClosed: propertyContents.flushPendingChanges()
                    background: Kos.KosSurface {
                        fillColor: Kos.AppTheme.contentBackground
                        radius: 18
                        elevation: 0.5
                    }
                    contentItem: ColumnLayout {
                        spacing: 10
                        RowLayout {
                            Layout.fillWidth: true
                            QQC.Label {
                                Layout.fillWidth: true
                                text: i18nc("@title", "Appearance")
                                font.weight: Font.DemiBold
                                color: Kos.AppTheme.paletteText
                            }
                            ViewerButton {
                                objectName: "nkshotCloseProperties"
                                implicitWidth: 28
                                implicitHeight: 28
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
        background: Item {
            readonly property bool useGlass: editorBackdrop.active && !Kos.AppTheme.reduceTransparency
            Kos.LiquidPanelSurface {
                objectName: "nkshotEditorMaterial"
                anchors.fill: parent
                visible: parent.useGlass
                backdrop: editorBackdrop
                cornerRadius: 18
                tintColor: Kos.AppTheme.contentBackground
                darkAppearance: Kos.AppTheme.dark
                materialOpacity: 0.95
            }
            Kos.KosSurface {
                anchors.fill: parent
                visible: !parent.useGlass
                fillColor: Kos.AppTheme.contentBackground
                radius: 18
                elevation: 0.5
                strokeColor: Kos.AppTheme.border
                showInnerHighlight: false
            }
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
            contentHeight: QmlUtils.iconTextButtonHeight
            contentItem: RowLayout {
                spacing: Kirigami.Units.mediumSpacing
                Item { Layout.fillWidth: true }
                RowLayout {
                    id: zoomControls
                    spacing: 2
                    ViewerButton {
                        implicitHeight: 28
                        implicitWidth: 28
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
                        implicitHeight: 28
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
                                QQC.Button {
                                    text: i18nc("@action", "Fit to view")
                                    Layout.fillWidth: true
                                    onClicked: {
                                        contentLoader.item.zoomToPercent(Math.min(1, contentLoader.item.fitZoom))
                                        zoomPopup.close()
                                    }
                                }
                                QQC.Button {
                                    text: i18nc("@action", "Actual size")
                                    Layout.fillWidth: true
                                    onClicked: { contentLoader.item.zoomToPercent(1); zoomPopup.close() }
                                }
                                QQC.SpinBox {
                                    id: zoomEditor
                                    editable: true
                                    from: Math.ceil(contentLoader.item.minZoom * 100)
                                    to: Math.floor(contentLoader.item.maxZoom * 100)
                                    stepSize: 25
                                    value: Math.round(contentLoader.item.currentZoom * 100)
                                    Accessible.name: i18nc("@label", "Custom zoom percentage")
                                    textFromValue: (value, locale) => Number(value).toLocaleString(locale, 'f', 0) + locale.percent
                                    valueFromText: (text, locale) => Number.fromLocaleString(locale, text.replace(/\D/g, ''))
                                    onValueModified: contentLoader.item.zoomToPercent(value / 100)
                                }
                            }
                        }
                    }
                    ViewerButton {
                        implicitHeight: 28
                        implicitWidth: 28
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
        onCreateCopy: (padding, style, watermark, rounded) => contextWindow.beautifyImageCopy(padding, style, watermark, rounded)
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

}
