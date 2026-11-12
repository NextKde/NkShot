/* SPDX-FileCopyrightText: 2026 NextKde
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Templates as T
import Kos.Icons as Symbols
import "SymbolMap.js" as SymbolMap

// Layout counterpart of a style's IconLabel, except the icon comes from the
// pinned Kos.Icons font instead of the system icon theme. A control that needs
// it declares `symbol` and uses this as its contentItem, so changing (or
// lacking) a desktop icon theme no longer blanks out the buttons.
Item {
    id: root

    // When set, glyph, label, font and layout mode come from the control, so a
    // call site only needs `contentItem: SymbolLabel { control: parent }` rather
    // than repeating the same five bindings for every button. Any property set
    // explicitly at the call site still wins.
    property T.AbstractButton control: null

    property string symbol: control ? control.icon.name : ""
    property string text: control ? control.text : ""
    property int display: control ? control.display : T.AbstractButton.TextBesideIcon
    property real spacing: 4
    property color color: control
        ? (control.icon.color.a > 0 ? control.icon.color : control.palette.buttonText)
        : palette.windowText
    property real iconSize: control && control.icon.height > 0 ? control.icon.height : 16
    property bool mirrored: control ? control.mirrored : false
    property font font: control ? control.font : Qt.application.font

    readonly property bool hasIcon: symbol.length > 0 && display !== T.AbstractButton.TextOnly
    readonly property bool hasLabel: text.length > 0 && display !== T.AbstractButton.IconOnly

    implicitWidth: {
        if (!hasLabel) {
            return hasIcon ? iconSize : 0;
        }
        if (!hasIcon) {
            return label.implicitWidth;
        }
        return iconSize + spacing + label.implicitWidth;
    }
    implicitHeight: Math.max(hasIcon ? iconSize : 0, hasLabel ? label.implicitHeight : 0)

    Symbols.Icon {
        id: icon
        visible: root.hasIcon
        name: SymbolMap.glyph(root.symbol)
        size: root.iconSize
        color: root.color
        anchors.verticalCenter: parent.verticalCenter
        x: root.mirrored ? root.width - root.iconSize : 0
    }

    QQC.Label {
        id: label
        visible: root.hasLabel
        text: root.text
        font: root.font
        color: root.color
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
        anchors.verticalCenter: parent.verticalCenter
        x: !root.hasIcon ? 0 : (root.mirrored ? 0 : root.iconSize + root.spacing)
        width: {
            if (!root.hasLabel) {
                return 0;
            }
            const available = root.hasIcon ? root.width - root.iconSize - root.spacing : root.width;
            return Math.max(0, available);
        }
    }
}
