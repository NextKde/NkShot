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
    background: Rectangle { color: Kos.AppTheme.contentSecondaryBackground }

    // Used in ViewerWindow::setMode()
    readonly property real minimumWidth: Math.max(
        header.implicitWidth,
        annotationsToolBar.implicitWidth + separator.implicitWidth + footerLoader.implicitWidth,
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
                    objectName: "nkshotMoreMenu"
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
        anchors.left: annotationsToolBar.right
        anchors.right: parent.right
        anchors.top: parent.top
    }

    QQC.Pane { // parent is contentItem
        id: annotationsToolBar
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.left
        anchors.leftMargin: 16
        visible: false
        leftPadding: 10
        rightPadding: 10
        topPadding: 12
        bottomPadding: 12
        contentItem: AnnotationsToolBarContents {
            id: annotationsToolBarContents
            displayMode: QQC.AbstractButton.IconOnly
            flow: Grid.LeftToRight
            columns: 2
            rows: -1
            spacing: 6
            paletteMode: true
            showUndoRedo: false
            showNoneButton: true
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
            left: footerLoader.left
            right: parent.right
            top: inlineMessageList.bottom
            bottom: footerLoader.top
            topMargin: 24
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
        anchors.left: separator.right
        anchors.right: parent.right
        anchors.top: parent.bottom
        visible: false
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
                AnimatedLoader {
                    id: loader
                    Layout.fillWidth: true
                    active: opacity > 0
                    visible: true
                    state: if (SpectacleCore.annotationDocument.tool.options !== AnnotationTool.NoOptions
                              || (SpectacleCore.annotationDocument.tool.type === AnnotationTool.SelectTool
                                 && SpectacleCore.annotationDocument.selectedItem.options !== AnnotationTool.NoOptions)
                    ) {
                        return "active"
                    } else {
                        return "inactive"
                    }
                    source: "AnnotationOptionsToolBarContents.qml"
                }
                QQC.ToolSeparator {
                    Layout.fillHeight: true
                    visible: loader.implicitWidth
                        + implicitWidth
                        + zoomLabel.implicitWidth
                        + zoomEditor.implicitWidth
                        + parent.spacing * 3 >= parent.width
                }
                QQC.Label {
                    id: zoomLabel
                    text: i18n("Zoom:")
                }
                QQC.SpinBox {
                    id: zoomEditor
                    from: contentLoader.item.minZoom * 100
                    to: contentLoader.item.maxZoom * 100
                    stepSize: 25
                    value: contentLoader.item.currentZoom * 100
                    textFromValue: (value, locale) => {
                        return Number(Math.round(value)).toLocaleString(locale, 'f', 0) + locale.percent
                    }
                    valueFromText: (text, locale) => {
                        return Number.fromLocaleString(locale, text.replace(/\D/g,''))
                    }
                    QQC.ToolTip.text: i18n("Image Zoom")
                    QQC.ToolTip.visible: hovered
                    QQC.ToolTip.delay: Kirigami.Units.toolTipDelay
                    Binding {
                        target: zoomEditor.contentItem
                        property: "horizontalAlignment"
                        value: Text.AlignRight
                        restoreMode: Binding.RestoreNone
                    }
                    TextContextMenuConnection {
                        target: zoomEditor.contentItem
                    }
                    onValueModified: contentLoader.item.zoomToPercent(Math.round(value) / 100)
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

    state: "normal"
    states: [
        State {
            name: "annotating"
            when: contextWindow.annotating
                && !SpectacleCore.videoMode
                && contentLoader.item !== null
            AnchorChanges {
                target: annotationsToolBar
                anchors.left: parent.left
                anchors.right: undefined
            }
            AnchorChanges {
                target: footerLoader
                anchors.bottom: parent.bottom
                anchors.top: undefined
            }
        },
        State {
            name: "normal"
            when: !contextWindow.annotating
                || SpectacleCore.videoMode
                || contentLoader.item === null
            AnchorChanges {
                target: annotationsToolBar
                anchors.left: undefined
                anchors.right: parent.left
            }
            AnchorChanges {
                target: footerLoader
                anchors.bottom: undefined
                anchors.top: parent.bottom
            }
        }
    ]
    transitions: [
        Transition {
            to: "annotating"
            SequentialAnimation {
                PropertyAction {
                    targets: [annotationsToolBar, separator, footerLoader]
                    property: "visible"
                    value: true
                }
                AnchorAnimation {
                    duration: Settings.reduceUiMotion ? 0 : Kirigami.Units.longDuration
                    easing.type: Easing.OutCubic
                }
            }
        },
        Transition {
            to: "normal"
            SequentialAnimation {
                AnchorAnimation {
                    duration: Settings.reduceUiMotion ? 0 : Kirigami.Units.longDuration
                    easing.type: Easing.OutCubic
                }
                PropertyAction {
                    targets: [annotationsToolBar, separator, footerLoader]
                    property: "visible"
                    value: false
                }
            }
        }
    ]
}
