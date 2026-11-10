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
        implicitHeight: 36
        implicitWidth: Math.max(40, implicitContentWidth + 24)
        leftPadding: 12
        rightPadding: 12
        display: root.width < 720 ? QQC.AbstractButton.IconOnly : QQC.AbstractButton.TextBesideIcon
        background: Kos.KosSurface {
            fillColor: parent.checked ? Kos.AppTheme.controlBackground
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
                text: i18nc("@action", "Capture")
                Accessible.role: Accessible.ButtonMenu
                onClicked: ScreenshotModeMenu.popup(this)
            }
            QQC.ToolSeparator { Layout.preferredHeight: 24 }
            ViewerButton {
                visible: action.enabled
                action: EditAction {}
            }
            Item { Layout.fillWidth: true; Layout.minimumWidth: 12 }
            UndoRedoGroup {
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
                action: SaveAction {}
            }
            ViewerButton {
                id: moreButton
                text: i18nc("@action", "More")
                icon.name: "view-more-symbolic"
                display: QQC.AbstractButton.IconOnly
                Accessible.role: Accessible.ButtonMenu
                onClicked: moreMenu.popup()
                QQC.Menu {
                    id: moreMenu
                    palette.windowText: Kos.AppTheme.paletteText
                    palette.text: Kos.AppTheme.paletteText
                    palette.buttonText: Kos.AppTheme.paletteText
                    palette.base: Kos.AppTheme.contentBackground
                    palette.button: Kos.AppTheme.controlBackground
                    palette.light: Kos.AppTheme.border
                    palette.mid: Kos.AppTheme.border
                    objectName: "nkshotMoreMenu"
                    padding: 6
                    background: Kos.KosSurface {
                        fillColor: Kos.AppTheme.contentBackground
                        radius: 14
                        elevation: 0.5
                    }
                    y: moreButton.height + 6
                    QQC.MenuItem { action: SaveAsAction {} }
                    QQC.MenuItem {
                        visible: !SpectacleCore.videoMode && SpectacleCore.ocrAvailable
                        height: visible ? implicitHeight : 0
                        action: OcrAction {}
                    }
                    QQC.MenuItem {
                        visible: SpectacleCore.videoMode
                        height: visible ? implicitHeight : 0
                        action: CopyLocationAction {}
                    }
                    QQC.MenuItem {
                        visible: !SpectacleCore.videoMode
                        height: visible ? implicitHeight : 0
                        text: i18nc("@action", "Export")
                        icon.name: "document-share"
                        onTriggered: Qt.callLater(function() { ExportMenu.popup(moreButton) })
                    }
                    QQC.MenuSeparator {}
                    QQC.MenuItem {
                        enabled: SpectacleCore.videoPlatform.supportedRecordingModes !== VideoPlatform.NoRecordingModes
                        text: i18nc("@action", "New Recording")
                        icon.name: "media-record"
                        onTriggered: Qt.callLater(function() { RecordingModeMenu.popup(moreButton) })
                    }
                    QQC.MenuItem {
                        text: i18nc("@action", "Options")
                        icon.name: "configure"
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
        // parent is contentItem
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
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
        topPadding: 12
        bottomPadding: 12
        contentItem: RowLayout {
            spacing: 6
            AnnotationsToolBarContents {
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
                    height: Math.min(root.height - 120, propertyContents.implicitHeight + 32)
                    x: Math.max(16 - propertiesButton.mapToItem(root, 0, 0).x, propertiesButton.width - width)
                    y: -height - 12
                    padding: 16
                    onClosed: propertyContents.flushPendingChanges()
                    background: Kos.KosSurface {
                        fillColor: Kos.AppTheme.contentBackground
                        radius: 18
                        elevation: 0.5
                    }
                    contentItem: QQC.ScrollView {
                        clip: true
                        AnnotationOptionsToolBarContents {
                            id: propertyContents
                            verticalLayout: true
                        }
                    }
                }
            }
        }
        background: Kos.KosSurface {
            fillColor: Kos.AppTheme.contentBackground
            radius: 20
            elevation: 0.5
            strokeColor: Kos.AppTheme.border
            showInnerHighlight: false
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

    Loader { // parent is contentItem
        id: contentLoader
        anchors {
            left: parent.left
            right: parent.right
            top: inlineMessageList.bottom
            bottom: annotationsToolBar.visible ? annotationsToolBar.top : parent.bottom
            topMargin: 24
            leftMargin: 24
            rightMargin: 24
            bottomMargin: 24
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
