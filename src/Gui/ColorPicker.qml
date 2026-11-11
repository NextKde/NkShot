// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import Kos.Ui as Kos

QQC.Pane {
    id: root
    padding: 16
    palette.text: Kos.AppTheme.paletteText
    palette.windowText: Kos.AppTheme.paletteText
    palette.buttonText: Kos.AppTheme.paletteText
    background: Kos.KosSurface { fillColor: Kos.AppTheme.contentSecondaryBackground; radius: 16; elevation: 0 }

    component PickerButton: QQC.ToolButton {
        implicitHeight: 36
        opacity: enabled ? 1 : 0.38
        icon.color: Kos.AppTheme.paletteText
        QQC.ToolTip.visible: hovered
        QQC.ToolTip.text: text
        background: Kos.KosSurface {
            fillColor: parent.down || parent.hovered ? Kos.AppTheme.controlBackground : "transparent"
            radius: 9
            elevation: 0
            strokeWidth: parent.activeFocus ? 2 : 0
            strokeColor: Kos.AppTheme.accent
        }
    }
    component Checker: Canvas {
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const painter = getContext("2d")
            painter.fillStyle = "#ffffff"
            painter.fillRect(0, 0, width, height)
            painter.fillStyle = "#dedee1"
            for (let y = 0; y < height; y += 10)
                for (let x = 0; x < width; x += 10)
                    if ((Math.floor(x / 10) + Math.floor(y / 10)) % 2)
                        painter.fillRect(x, y, 10, 10)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            Column {
                anchors.left: parent.left
                anchors.right: closeButton.left
                anchors.rightMargin: 12
                spacing: 4
                QQC.Label { text: i18nc("@title", "Pick a color"); font.pixelSize: 20; font.bold: true; color: Kos.AppTheme.paletteText }
                QQC.Label { width: parent.width; text: colorWindow.screenMode ? i18nc("@info", "Choose with your desktop’s color picker") : i18nc("@info", "From a snapshot of your edited image"); color: Kos.AppTheme.mutedText; font.pixelSize: 12; elide: Text.ElideRight }
            }
            PickerButton {
                id: closeButton
                anchors.right: parent.right
                objectName: "nkshotColorClose"
                text: i18nc("@action", "Close")
                icon.name: "window-close"
                display: QQC.AbstractButton.IconOnly
                onClicked: colorWindow.close()
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            QQC.Label {text: i18nc("@label", "Source"); color: Kos.AppTheme.mutedText; Layout.fillWidth: true}
            QQC.ComboBox {
                id: sourceChoice
                objectName: "nkshotColorSource"
                implicitWidth: 160
                implicitHeight: 32
                model: [i18nc("@item", "Screenshot"), i18nc("@item", "Screen")]
                currentIndex: colorWindow.screenMode ? 1 : 0
                palette.text: Kos.AppTheme.paletteText
                palette.buttonText: Kos.AppTheme.paletteText
                background: Kos.KosSurface {fillColor: Kos.AppTheme.controlBackground; radius: 8; elevation: 0}
                Accessible.name: i18nc("@label", "Color source")
                delegate: QQC.ItemDelegate {
                    required property int index
                    required property string modelData
                    width: sourceChoice.width
                    text: modelData
                    enabled: index === 1 || colorWindow.hasImage
                    highlighted: sourceChoice.highlightedIndex === index
                }
                onActivated: index => {
                    colorWindow.screenMode = index === 1
                    // Keyboard navigation may activate a disabled delegate.
                    // Restore the authoritative source after the C++ guard.
                    currentIndex = Qt.binding(function() { return colorWindow.screenMode ? 1 : 0 })
                }
            }
        }
        Rectangle {
            visible: !colorWindow.screenMode
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 100
            color: Kos.AppTheme.contentBackground
            radius: 12
            border.color: viewport.activeFocus ? Kos.AppTheme.accent : Kos.AppTheme.border
            Item {
                id: viewport
                objectName: "nkshotColorViewport"
                anchors.fill: parent
                anchors.margins: 12
                focus: !colorWindow.screenMode
                activeFocusOnTab: true
                Component.onCompleted: if (!colorWindow.screenMode) forceActiveFocus()
                Connections {
                    target: colorWindow
                    function onSourceModeChanged() { if (!colorWindow.screenMode) Qt.callLater(viewport.forceActiveFocus) }
                }
                readonly property real imageScale: Math.min(width / Math.max(1, colorWindow.imageSize.width), height / Math.max(1, colorWindow.imageSize.height))
                readonly property real imageWidth: colorWindow.imageSize.width * imageScale
                readonly property real imageHeight: colorWindow.imageSize.height * imageScale
                Checker {
                    width: viewport.imageWidth; height: viewport.imageHeight
                    anchors.centerIn: parent
                }
                Image {
                    anchors.fill: parent
                    source: colorWindow.hasImage ? colorWindow.imageSource : ""
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                }
                Rectangle {
                    visible: colorWindow.hasSample
                    x: (viewport.width - viewport.imageWidth) / 2 + (colorWindow.pixel.x + 0.5) * viewport.imageScale - width / 2
                    y: (viewport.height - viewport.imageHeight) / 2 + (colorWindow.pixel.y + 0.5) * viewport.imageScale - height / 2
                    width: 13; height: 13; radius: 6.5
                    color: "transparent"; border.color: "white"; border.width: 2
                    Rectangle { anchors.fill: parent; anchors.margins: -1; radius: 7.5; color: "transparent"; border.color: "#202023" }
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.CrossCursor
                    onPressed: mouse => { viewport.forceActiveFocus(); colorWindow.sampleAt(mouse.x, mouse.y, width, height) }
                    onPositionChanged: mouse => { if (pressed) colorWindow.sampleAt(mouse.x, mouse.y, width, height) }
                }
                Keys.onLeftPressed: colorWindow.moveSample(-1, 0)
                Keys.onRightPressed: colorWindow.moveSample(1, 0)
                Keys.onUpPressed: colorWindow.moveSample(0, -1)
                Keys.onDownPressed: colorWindow.moveSample(0, 1)
                Accessible.focusable: true
                Accessible.description: colorWindow.hasSample ? i18nc("@info", "Pixel %1, %2. %3. %4.", colorWindow.pixel.x, colorWindow.pixel.y, colorWindow.hexValue, colorWindow.rgbValue) : i18nc("@info", "No pixel selected")
                Accessible.role: Accessible.Canvas
                Accessible.name: i18nc("@info", "Image color sampler. Click to choose a pixel, or use the arrow keys.")
            }
        }
        Rectangle {
            visible: colorWindow.screenMode
            Layout.fillWidth: true
            Layout.maximumWidth: 420
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredHeight: screenContent.implicitHeight + 40
            color: Kos.AppTheme.contentBackground
            border.color: Kos.AppTheme.border
            radius: 12
            ColumnLayout {
                id: screenContent
                anchors.centerIn: parent
                width: parent.width - 32
                spacing: 12
                Rectangle {
                    objectName: "nkshotScreenSwatch"
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 64
                    Layout.preferredHeight: 64
                    color: colorWindow.hasSample ? colorWindow.sampleColor : Kos.AppTheme.controlBackground
                    border.color: Kos.AppTheme.border
                    radius: 14
                    QQC.Label {anchors.centerIn: parent; visible: !colorWindow.hasSample; text: "○"; font.pixelSize: 28; color: Kos.AppTheme.mutedText}
                }
                QQC.Label {
                    objectName: "nkshotScreenHex"
                    Layout.fillWidth: true
                    text: colorWindow.hasSample ? colorWindow.hexValue : i18nc("@info", "Choose a screen color")
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    color: Kos.AppTheme.paletteText
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                }
                QQC.Label {
                    Layout.fillWidth: true
                    text: colorWindow.hasSample ? i18nc("@info", "%1 · sRGB, 8-bit", colorWindow.rgbValue) : i18nc("@info", "No image is captured by this window")
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    color: Kos.AppTheme.mutedText
                    font.pixelSize: 12
                }
                QQC.Label {
                    objectName: "nkshotScreenStatus"
                    Layout.fillWidth: true
                    text: colorWindow.screenStatus
                    textFormat: Text.PlainText
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    color: Kos.AppTheme.mutedText
                    font.pixelSize: 12
                }
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 8
                    PickerButton {
                        objectName: "nkshotScreenPick"
                        text: colorWindow.hasSample ? i18nc("@action", "Pick again") : i18nc("@action", "Pick a color…")
                        enabled: !colorWindow.screenPending
                        onClicked: colorWindow.pickScreenColor()
                    }
                    PickerButton {
                        objectName: "nkshotScreenCancel"
                        visible: colorWindow.screenPending
                        text: i18nc("@action", "Stop waiting")
                        onClicked: colorWindow.cancelScreenPick()
                    }
                }
            }
        }
        Item { visible: colorWindow.screenMode; Layout.fillHeight: true }
        RowLayout {
            visible: !colorWindow.screenMode
            Layout.fillWidth: true
            spacing: 14
            Item {
                Layout.preferredWidth: 66
                Layout.preferredHeight: 66
                Checker { anchors.fill: parent }
                Image { anchors.fill: parent; source: colorWindow.neighborhoodSource; cache: false; smooth: false }
                Rectangle { anchors.centerIn: parent; width: 8; height: 8; color: "transparent"; border.width: 1; border.color: "white"; visible: colorWindow.hasSample
                    Rectangle { anchors.fill: parent; anchors.margins: -1; color: "transparent"; border.color: "#202023" }
                }
                Rectangle { anchors.fill: parent; color: "transparent"; border.color: Kos.AppTheme.border }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 5
                RowLayout {
                    spacing: 8
                    Rectangle {
                        width: 18; height: 18; radius: 5
                        color: colorWindow.hasSample ? colorWindow.sampleColor : "transparent"
                        border.color: Kos.AppTheme.border
                    }
                    QQC.Label {
                        objectName: "nkshotColorHex"
                        text: colorWindow.hasSample ? colorWindow.hexValue : i18nc("@info", "Choose a pixel")
                        color: Kos.AppTheme.paletteText
                        font.pixelSize: 18
                        font.weight: Font.DemiBold
                    }
                }
                QQC.Label {
                    Layout.fillWidth: true
                    text: colorWindow.hasSample ? colorWindow.rgbValue : i18nc("@info", "Click or drag across the image")
                    color: Kos.AppTheme.mutedText
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }
                QQC.Label {
                    text: colorWindow.hasSample ? i18nc("@info", "Pixel %1, %2 · sRGB", colorWindow.pixel.x, colorWindow.pixel.y) : i18nc("@info", "Arrow keys move one image pixel")
                    color: Kos.AppTheme.mutedText
                    font.pixelSize: 11
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            QQC.Label {
                id: feedback
                objectName: "nkshotColorFeedback"
                Layout.fillWidth: true
                text: colorWindow.screenMode ? i18nc("@info", "Copy only when ready") : i18nc("@info", "No changes to your image")
                color: Kos.AppTheme.mutedText
                font.pixelSize: 11
                wrapMode: Text.Wrap
                Connections {
                    target: colorWindow
                    function onValueCopied(value) { feedback.text = i18nc("@info", "Copied %1", value) }
                    function onSampleChanged() { feedback.text = colorWindow.screenMode ? i18nc("@info", "Copy only when ready") : i18nc("@info", "No changes to your image") }
                }
            }
            PickerButton {
                objectName: "nkshotColorCopyRgb"
                text: i18nc("@action", "Copy RGB")
                enabled: colorWindow.hasSample
                onClicked: colorWindow.copyRgb()
            }
            PickerButton {
                id: copyHex
                objectName: "nkshotColorCopyHex"
                text: i18nc("@action", "Copy HEX")
                enabled: colorWindow.hasSample
                palette.buttonText: Kos.AppTheme.accentText
                background: Kos.KosSurface {
                    fillColor: Kos.AppTheme.accent; radius: 9; elevation: 0
                    strokeWidth: copyHex.activeFocus ? 2 : 0
                    strokeColor: Kos.AppTheme.paletteText
                    opacity: copyHex.enabled ? 1 : 0.4
                }
                onClicked: colorWindow.copyHex()
            }
        }
    }
}
