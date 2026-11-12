// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import Kos.Ui as Kos

// Presentation only: models and settings are injected by the host.
QQC.Pane {
    id: root
    objectName: "nkshotSetupPane"
    font: Qt.application.font
    required property var settings
    required property var captureModes
    required property var recordingModes
    property bool recordingAvailable: false
    property bool transientSupported: false
    property bool clickSupported: false
    property bool onClickOnly: false
    property bool audioSupported: false
    property real captureTimeRemaining: 0
    property real captureProgress: 0
    property int modeIndex: 0
    readonly property int autoStopSeconds: [60, 300, 600].indexOf(root.settings.recordingAutoStopSeconds) >= 0
        ? root.settings.recordingAutoStopSeconds : 0
    // Fixed chrome plus a usable scroll viewport; independent of expanded options.
    readonly property real chromeMinimumHeight: tabs.implicitHeight
        + Math.max(primaryAction.implicitHeight, cancelAction.implicitHeight) + 10 + 32 + 80
    signal captureRequested(int captureMode)
    signal recordRequested(int recordingMode)
    signal cancelRequested()
    padding: 0
    background: null
    palette.windowText: Kos.AppTheme.paletteText
    palette.text: Kos.AppTheme.paletteText
    palette.buttonText: Kos.AppTheme.paletteText
    palette.button: Kos.AppTheme.controlBackground
    palette.base: Kos.AppTheme.contentBackground
    palette.window: Kos.AppTheme.contentBackground
    palette.highlight: Kos.AppTheme.accent
    palette.highlightedText: Kos.AppTheme.accentText
    component Option : QQC.CheckBox {
        Layout.fillWidth: true
        font: root.font
        padding: 2
        spacing: 10
        indicator: Rectangle {
            implicitWidth: 18; implicitHeight: 18
            x: parent.mirrored ? parent.width - width - parent.rightPadding : parent.leftPadding
            y: (parent.height - height) / 2
            radius: 5
            color: parent.checked ? Kos.AppTheme.accent : Kos.AppTheme.controlBackground
            border.width: parent.activeFocus ? 2 : 1
            border.color: parent.activeFocus ? Kos.AppTheme.accent : Kos.AppTheme.mutedText
            Text { anchors.centerIn: parent; text: "✓"; visible: parent.parent.checked; color: Kos.AppTheme.accentText; font: root.font }
        }
        contentItem: Text {
            text: parent.text
            font: parent.font
            color: parent.enabled ? Kos.AppTheme.text : Kos.AppTheme.mutedText
            leftPadding: parent.mirrored ? 0 : parent.indicator.width + parent.spacing
            rightPadding: parent.mirrored ? parent.indicator.width + parent.spacing : 0
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.WordWrap
        }
    }
    Connections {
        target: root.captureModes && typeof root.captureModes.modelReset === "function" ? root.captureModes : null
        function onModelReset() { Qt.callLater(captureTarget.restoreSelection) }
    }
    Connections {
        target: root.recordingModes && typeof root.recordingModes.modelReset === "function" ? root.recordingModes : null
        function onModelReset() { Qt.callLater(recordingTarget.restoreSelection) }
    }
    Connections {
        target: root.settings
        function onCaptureModeChanged() { Qt.callLater(captureTarget.restoreSelection) }
    }
    component TargetSelect : QQC.ComboBox {
        implicitHeight: Math.max(36, contentItem.implicitHeight + 14)
        leftPadding: mirrored ? 30 : 12
        rightPadding: mirrored ? 12 : 30
        background: Rectangle {
            radius: 8
            color: Kos.AppTheme.controlBackground
            border.width: parent.activeFocus ? 2 : 0
            border.color: Kos.AppTheme.accent
        }
        contentItem: Text {
            text: parent.displayText
            font: parent.font
            horizontalAlignment: parent.mirrored ? Text.AlignRight : Text.AlignLeft
            color: parent.enabled ? Kos.AppTheme.paletteText : Kos.AppTheme.mutedText
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }
    contentItem: ColumnLayout {
    spacing: 16
    QQC.TabBar {
        id: tabs
        background: null
        objectName: "nkshotSetupTabs"
        Layout.fillWidth: true
        currentIndex: root.modeIndex
        onCurrentIndexChanged: root.modeIndex = currentIndex
        QQC.TabButton {
            implicitHeight: Math.max(32, contentItem.implicitHeight + 12)
            font: root.font
            objectName: "nkshotSetupScreenshotTab"; text: i18n("Screenshot")
            background: Rectangle { radius: 8; color: parent.checked ? Kos.AppTheme.controlBackground : "transparent"; border.width: parent.activeFocus ? 2 : 0; border.color: Kos.AppTheme.accent }
        }
        QQC.TabButton {
            implicitHeight: Math.max(32, contentItem.implicitHeight + 12)
            font: root.font
            objectName: "nkshotSetupRecordingTab"; text: i18n("Recording")
            background: Rectangle { radius: 8; color: parent.checked ? Kos.AppTheme.controlBackground : "transparent"; border.width: parent.activeFocus ? 2 : 0; border.color: Kos.AppTheme.accent }
        }
    }
    QQC.ScrollView {
        id: scroll
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        contentWidth: availableWidth
        QQC.ScrollBar.horizontal.policy: QQC.ScrollBar.AlwaysOff
        ColumnLayout {
            width: scroll.availableWidth
            spacing: 10
            Text {
                Accessible.role: Accessible.StaticText
                Accessible.name: text
                objectName: "nkshotSetupAreaHeading"
                text: root.modeIndex === 0 ? i18n("Capture area") : i18n("Record area")
                font.family: root.font.family
                font.pointSize: root.font.pointSize * 1.15
                font.bold: true
                color: Kos.AppTheme.text
            }
            TargetSelect {
                id: captureTarget
                objectName: "nkshotCaptureTarget"
                Layout.fillWidth: true
                visible: root.modeIndex === 0
                model: root.captureModes
                textRole: "display"
                valueRole: "captureMode"
                delegate: QQC.ItemDelegate {
                    required property var model
                    required property int index
                    width: captureTarget.width
                    text: model.display
                    highlighted: captureTarget.highlightedIndex === index
                    readonly property string shortcutText: model.shortcuts || ""
                    Accessible.description: shortcutText
                    QQC.ToolTip.text: shortcutText
                    QQC.ToolTip.visible: hovered && shortcutText.length > 0
                    QQC.ToolTip.delay: 700
                }
                function restoreSelection() {
                    const saved = indexOfValue(root.settings.captureMode)
                    const next = saved >= 0 ? saved : (count > 0 ? 0 : -1)
                    // A reset may replace rows without changing the selected index.
                    // Force the role values to be resolved against the new model.
                    currentIndex = -1
                    currentIndex = next
                }
                Component.onCompleted: restoreSelection()
                onCountChanged: Qt.callLater(restoreSelection)
                onModelChanged: Qt.callLater(restoreSelection)
                onActivated: root.settings.captureMode = currentValue
            }
            TargetSelect {
                id: recordingTarget
                objectName: "nkshotRecordingTarget"
                Layout.fillWidth: true
                visible: root.modeIndex === 1 && count > 0
                enabled: root.recordingAvailable
                model: root.recordingModes
                textRole: "display"
                valueRole: "recordingMode"
                function restoreSelection() {
                    const next = currentIndex >= 0 && currentIndex < count ? currentIndex : (count > 0 ? 0 : -1)
                    currentIndex = -1
                    currentIndex = next
                }
                onCountChanged: Qt.callLater(restoreSelection)
                onModelChanged: Qt.callLater(restoreSelection)
            }
            QQC.Label {
                Layout.fillWidth: true
                visible: root.modeIndex === 1 && !root.recordingAvailable
                text: i18n("Screen recording is not available on this platform.")
                color: Kos.AppTheme.mutedText
                wrapMode: Text.WordWrap
            }
            ColumnLayout {
                Layout.fillWidth: true
                visible: root.modeIndex === 0
                spacing: 4
                Option {
                    objectName: "nkshotSetupPointer"
                    text: i18n("Include mouse pointer")
                    checked: root.settings.includePointer
                    onToggled: root.settings.includePointer = checked
                }
                RowLayout {
                    Layout.fillWidth: true
                    QQC.Label { text: i18n("Delay"); Layout.fillWidth: true }
                    QQC.SpinBox {
                        id: delay
                        objectName: "nkshotSetupDelay"
                        from: 0; to: 9900; stepSize: 100
                        editable: true
                        background: Rectangle { radius: 8; color: Kos.AppTheme.controlBackground; border.width: delay.activeFocus ? 2 : 0; border.color: Kos.AppTheme.accent }
                        enabled: !root.onClickOnly && !(root.clickSupported && root.settings.captureOnClick)
                        value: Math.round(root.settings.captureDelay * 100)
                        onValueModified: root.settings.captureDelay = value / 100
                        textFromValue: (value, locale) => Number(value / 100).toLocaleString(locale, "f", value % 100 ? 2 : 0)
                        valueFromText: (text, locale) => Math.round(Number.fromLocaleString(locale, text) * 100)
                        validator: DoubleValidator { bottom: 0; top: 99; decimals: 2; locale: delay.locale.name }
                    }
                    QQC.Label { text: i18n("s"); color: Kos.AppTheme.mutedText }
                }
                Option {
                    objectName: "nkshotSetupCaptureOnClick"
                    visible: root.clickSupported || root.onClickOnly
                    enabled: !root.onClickOnly
                    text: i18n("Capture on click")
                    checked: root.onClickOnly || (root.clickSupported && root.settings.captureOnClick)
                    onToggled: root.settings.captureOnClick = checked
                }
                QQC.ToolButton {
                    id: advanced
                    icon.color: Kos.AppTheme.paletteText
                    implicitHeight: Math.max(32, contentItem.implicitHeight + 12)
                    background: Rectangle {
                        radius: 8
                        color: advanced.hovered || advanced.down || advanced.activeFocus ? Kos.AppTheme.controlBackground : "transparent"
                        border.width: advanced.activeFocus ? 2 : 0
                        border.color: Kos.AppTheme.accent
                    }
                    objectName: "nkshotSetupAdvanced"
                    Layout.fillWidth: true
                    text: checked ? i18n("Fewer options") : i18n("More options")
                    checkable: true
                    icon.name: checked ? "arrow-up" : "arrow-down"
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: advanced.checked
                    spacing: 4
                    Option {
                        objectName: "nkshotSetupDecorations"
                        text: i18n("Include window titlebar and borders")
                        checked: root.settings.includeDecorations
                        onToggled: root.settings.includeDecorations = checked
                    }
                    Option {
                        objectName: "nkshotSetupShadow"
                        text: i18n("Include window shadow")
                        enabled: root.settings.includeDecorations
                        checked: root.settings.includeShadow
                        onToggled: root.settings.includeShadow = checked
                    }
                    Option {
                        objectName: "nkshotSetupTransient"
                        visible: root.transientSupported
                        text: i18n("Capture the current pop-up only")
                        checked: root.settings.transientOnly
                        onToggled: root.settings.transientOnly = checked
                    }
                    Option {
                        objectName: "nkshotSetupQuit"
                        text: i18n("Quit after manual Save or Copy")
                        checked: root.settings.quitAfterSaveCopyExport
                        onToggled: root.settings.quitAfterSaveCopyExport = checked
                    }
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                visible: root.modeIndex === 1
                spacing: 4
                RowLayout {
                    Layout.fillWidth: true
                    QQC.Label { text: i18n("Start delay"); Layout.fillWidth: true }
                    TargetSelect {
                        objectName: "nkshotRecordingStartDelay"
                        implicitWidth: 130
                        model: [
                            { display: i18n("No delay"), seconds: 0 },
                            { display: i18n("3 seconds"), seconds: 3 },
                            { display: i18n("5 seconds"), seconds: 5 },
                            { display: i18n("10 seconds"), seconds: 10 }
                        ]
                        textRole: "display"
                        valueRole: "seconds"
                        currentIndex: Math.max(0, [0, 3, 5, 10].indexOf(root.settings.recordingStartDelay))
                        onActivated: root.settings.recordingStartDelay = currentValue
                    }
                }
                Option {
                    objectName: "nkshotSetupVideoPointer"
                    text: i18n("Include mouse pointer")
                    checked: root.settings.videoIncludePointer
                    onToggled: root.settings.videoIncludePointer = checked
                }
                Option {
                    objectName: "nkshotSetupSystemAudio"
                    text: i18n("Record system audio")
                    enabled: root.audioSupported
                    checked: root.settings.videoRecordSystemAudio
                    onToggled: root.settings.videoRecordSystemAudio = checked
                }
                Option {
                    objectName: "nkshotSetupMicrophone"
                    text: i18n("Record microphone")
                    enabled: root.audioSupported
                    checked: root.settings.videoRecordMicrophone
                    onToggled: root.settings.videoRecordMicrophone = checked
                }
                QQC.ToolButton {
                    id: recordingAdvanced
                    objectName: "nkshotRecordingAdvanced"
                    Layout.fillWidth: true
                    implicitHeight: Math.max(32, contentItem.implicitHeight + 12)
                    checkable: true
                    text: (checked ? i18n("Fewer options") : i18n("More options"))
                        + (!checked && root.autoStopSeconds > 0 ? i18n(" · Auto-stop %1 min", root.autoStopSeconds / 60) : "")
                    icon.name: checked ? "arrow-up" : "arrow-down"
                    icon.color: Kos.AppTheme.paletteText
                    background: Rectangle {
                        radius: 8
                        color: recordingAdvanced.hovered || recordingAdvanced.down || recordingAdvanced.activeFocus
                            ? Kos.AppTheme.controlBackground : "transparent"
                        border.width: recordingAdvanced.activeFocus ? 2 : 0
                        border.color: Kos.AppTheme.accent
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: recordingAdvanced.checked
                    spacing: 4
                    RowLayout {
                        Layout.fillWidth: true
                        QQC.Label {text: i18n("Auto-stop"); Layout.fillWidth: true}
                        TargetSelect {
                            objectName: "nkshotRecordingAutoStop"
                            implicitWidth: 130
                            model: [
                                {display: i18n("Off"), seconds: 0},
                                {display: i18n("1 minute"), seconds: 60},
                                {display: i18n("5 minutes"), seconds: 300},
                                {display: i18n("10 minutes"), seconds: 600}
                            ]
                            textRole: "display"
                            valueRole: "seconds"
                            currentIndex: Math.max(0, [0, 60, 300, 600].indexOf(root.settings.recordingAutoStopSeconds))
                            onActivated: root.settings.recordingAutoStopSeconds = currentValue
                            Accessible.name: i18n("Stop recording automatically after")
                        }
                    }
                    QQC.Label {
                        Layout.fillWidth: true
                        text: i18n("Counts recorded time only. Pauses do not count.")
                        font.pointSize: root.font.pointSize * 0.92
                        color: Kos.AppTheme.mutedText
                        wrapMode: Text.WordWrap
                    }
                }
                QQC.Label {
                    Layout.fillWidth: true
                    visible: !root.audioSupported
                    text: i18n("The selected video format does not support audio.")
                    font.pointSize: root.font.pointSize * 0.92
                    color: Kos.AppTheme.mutedText
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
    ColumnLayout {
        Layout.fillWidth: true
        spacing: 6
        QQC.ProgressBar {
            id: countdownProgress
            Layout.fillWidth: true
            implicitHeight: 4
            visible: root.captureTimeRemaining > 0
            value: root.captureProgress
            background: Rectangle { radius: 2; color: Kos.AppTheme.controlBackground }
            contentItem: Item {
                Rectangle {
                    width: parent.width * countdownProgress.visualPosition
                    height: parent.height
                    radius: 2
                    color: Kos.AppTheme.accent
                }
            }
        }
        QQC.Button {
            id: cancelAction
            objectName: "nkshotSetupCancel"
            Layout.fillWidth: true
            implicitHeight: Math.max(38, contentItem.implicitHeight + 16)
            font: root.font
            background: Rectangle {
                radius: 9
                color: Kos.AppTheme.controlBackground
                border.width: parent.activeFocus ? 2 : 0
                border.color: Kos.AppTheme.accent
            }
            contentItem: Text {
                text: parent.text
                font: parent.font
                color: Kos.AppTheme.paletteText
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.WordWrap
            }
            visible: root.captureTimeRemaining > 0
            text: i18n("Cancel capture (%1 s)", Math.ceil(root.captureTimeRemaining / 1000))
            onClicked: root.cancelRequested()
        }
        QQC.Button {
            id: primaryAction
            objectName: "nkshotSetupPrimary"
            visible: root.captureTimeRemaining <= 0
            Layout.fillWidth: true
            implicitHeight: Math.max(38, contentItem.implicitHeight + 16)
            palette.buttonText: enabled ? Kos.AppTheme.accentText : Kos.AppTheme.mutedText
            background: Rectangle {
                radius: 9
                color: parent.enabled ? Kos.AppTheme.accent : Kos.AppTheme.controlBackground
                opacity: parent.down ? 0.8 : 1
                border.width: parent.activeFocus ? 2 : 0
                border.color: Kos.AppTheme.paletteText
            }
            contentItem: Text {
                text: parent.text
                font: parent.font
                color: parent.enabled ? Kos.AppTheme.accentText : Kos.AppTheme.mutedText
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.WordWrap
            }
            highlighted: true
            text: root.modeIndex === 0 ? i18n("Take screenshot") : i18n("Start recording")
            enabled: root.captureTimeRemaining <= 0 && (root.modeIndex === 0
                ? captureTarget.currentIndex >= 0
                : root.recordingAvailable && recordingTarget.currentIndex >= 0)
            onClicked: {
                if (root.modeIndex === 0) {
                    root.settings.captureMode = captureTarget.currentValue
                    root.captureRequested(captureTarget.currentValue)
                } else root.recordRequested(recordingTarget.currentValue)
            }
        }
    }
}
}
