/* SPDX-FileCopyrightText: 2026 NextKde
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls as QQC
import Kos.Icons as Symbols
import "SymbolMap.js" as SymbolMap

// Layout counterpart of a style's IconLabel, except the icon comes from the
// pinned Kos.Icons font instead of the system icon theme. A control that needs
// it declares `symbol` and uses this as its contentItem, so changing (or
// lacking) a desktop icon theme no longer blanks out the buttons.
Item {
    id: root

    property string symbol: ""
    property string text: ""
    property int display: QQC.AbstractButton.TextBesideIcon
    property real spacing: 4
    property color color: palette.windowText
    property real iconSize: 16
    property bool mirrored: false
    property font font: Qt.application.font

    readonly property bool hasIcon: symbol.length > 0 && display !== QQC.AbstractButton.TextOnly
    readonly property bool hasLabel: text.length > 0 && display !== QQC.AbstractButton.IconOnly

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
