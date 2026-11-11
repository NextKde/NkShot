// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import Kos.Ui as Kos
QQC.Pane {
    id: root
    readonly property string draftHint: i18nc("@info","Draft stays only while NkShot is open. Copy or pin the image to keep it.")
    padding: 20
    palette.text: Kos.AppTheme.paletteText
    palette.windowText: Kos.AppTheme.paletteText
    palette.buttonText: Kos.AppTheme.paletteText
    background: Kos.KosSurface {fillColor: Kos.AppTheme.contentSecondaryBackground; radius: 16; elevation: 0}
    function refresh() {previewTimer.stop();noteWindow.updatePreview(editor.text,styleChoice.currentIndex)}
    Timer {id: previewTimer; interval: 120; onTriggered: root.refresh()}
    component NoteButton: QQC.ToolButton {
        implicitHeight: 36
        icon.color: Kos.AppTheme.paletteText
        QQC.ToolTip.visible: hovered
        QQC.ToolTip.text: text
        background: Kos.KosSurface {fillColor: parent.hovered ? Kos.AppTheme.controlBackground : "transparent"; radius: 9; elevation: 0; strokeWidth: 0}
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 16
        RowLayout {
            Layout.fillWidth: true
            QQC.Label {text: i18nc("@title","Quick Note");font.pixelSize: 20;font.bold: true;color: Kos.AppTheme.paletteText;Layout.fillWidth: true}
            NoteButton {
                objectName: "nkshotNoteCopy"
                text: i18nc("@action","Copy image")
                icon.name: "edit-copy"
                display: root.width<520?QQC.AbstractButton.IconOnly:QQC.AbstractButton.TextBesideIcon
                enabled: noteWindow.canExport && !previewTimer.running
                onClicked: {root.refresh();noteWindow.copyImage()}
            }
            NoteButton {
                id: pinButton
                objectName: "nkshotNotePin"
                text: i18nc("@action","Pin note")
                enabled: noteWindow.canExport && !previewTimer.running
                palette.buttonText: Kos.AppTheme.accentText
                background: Kos.KosSurface {fillColor: Kos.AppTheme.accent; radius: 9; elevation: 0; strokeWidth: 0; opacity: pinButton.enabled?1:0.45}
                onClicked: {root.refresh();noteWindow.pinImage()}
            }
            NoteButton {objectName: "nkshotNoteClose";text: i18nc("@action","Close");icon.name:"window-close";display:QQC.AbstractButton.IconOnly;onClicked:noteWindow.close()}
        }
        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: root.width>=700?2:1
            columnSpacing: 20
            rowSpacing: 12
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 320
                Layout.preferredHeight: root.width>=700?-1:180
                RowLayout {
                    Layout.fillWidth: true
                    QQC.Label {text: i18nc("@label","Note text");color:Kos.AppTheme.mutedText;Layout.fillWidth:true}
                    QQC.Label {text:editor.text.length+" / 2000";color:Kos.AppTheme.mutedText}
                }
                QQC.ScrollView {
                    implicitWidth: 280
                    implicitHeight: 180
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    contentWidth: availableWidth
                    background: Kos.KosSurface {fillColor:Kos.AppTheme.contentBackground;radius:12;elevation:0}
                    QQC.TextArea {
                        id: editor
                        focus: true
                        Component.onCompleted: forceActiveFocus()
                        objectName: "nkshotNoteEditor"
                        placeholderText: i18nc("@info:placeholder","Write a reminder, steps, or a quote…")
                        wrapMode: TextEdit.Wrap
                        textFormat: TextEdit.PlainText
                        font.pixelSize: 16
                        padding: 16
                        selectByMouse: true
                        background: null
                        onTextChanged: {feedback.text=root.draftHint;previewTimer.restart()}
                    }
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 460
                RowLayout {
                    Layout.fillWidth: true
                    QQC.Label {text:i18nc("@label","Image preview");color:Kos.AppTheme.mutedText;Layout.fillWidth:true}
                    QQC.ComboBox {
                        id: styleChoice
                        objectName:"nkshotNoteStyle"
                        model:[i18nc("@item","Paper"),i18nc("@item","Blue"),i18nc("@item","Night")]
                        implicitHeight:32
                        leftPadding:12;rightPadding:28
                        background:Kos.KosSurface {fillColor:Kos.AppTheme.controlBackground;radius:8;elevation:0}
                        contentItem:QQC.Label {text:styleChoice.displayText;color:Kos.AppTheme.paletteText;verticalAlignment:Text.AlignVCenter}
                        indicator:QQC.Label {text:"⌄";color:Kos.AppTheme.mutedText;x:styleChoice.width-width-10;anchors.verticalCenter:parent.verticalCenter}
                        onCurrentIndexChanged:previewTimer.restart()
                    }
                }
                Item {
                    Layout.fillWidth:true
                    Layout.fillHeight:true
                    Layout.minimumHeight:120
                    Image {objectName:"nkshotNotePreview";anchors.fill:parent;source:noteWindow.imageSource;fillMode:Image.PreserveAspectFit;cache:false;smooth:true}
                    QQC.Label {anchors.centerIn:parent;width:Math.min(260,parent.width);visible:!noteWindow.canExport;text:i18nc("@info","Your note becomes a shareable image here.");wrapMode:Text.Wrap;horizontalAlignment:Text.AlignHCenter;color:Kos.AppTheme.mutedText}
                }
            }
        }
        QQC.Label {Layout.fillWidth:true;visible:noteWindow.error.length>0;text:noteWindow.error;color:Kos.AppTheme.paletteText;wrapMode:Text.Wrap}
        QQC.Label {
            id:feedback
            objectName:"nkshotNoteFeedback"
            Layout.fillWidth:true
            wrapMode:Text.Wrap
            color:Kos.AppTheme.mutedText
            text:root.draftHint
            Connections {target:noteWindow;function onStatusMessage(message){feedback.text=message}}
        }
    }
}
