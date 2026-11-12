/* SPDX-FileCopyrightText: 2026 NextKde
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

import QtQuick
import Kos.Ui as Kos
import org.kde.kirigami as Kirigami

SymbolMenuItem {
    id: root
    implicitHeight: 30
    leftPadding: 8
    rightPadding: 8
    font.pixelSize: 13
    font.weight: Font.Normal
    Kirigami.Theme.inherit: false
    Kirigami.Theme.textColor: Kos.AppTheme.paletteText
    Kirigami.Theme.highlightedTextColor: Kos.AppTheme.paletteText
    // Checked rows use the neutral selection fill, like LiquidPopoverItem.
    indicator: null
    icon.color: enabled ? Kos.AppTheme.paletteText : Kos.AppTheme.disabledText
    palette.windowText: enabled ? Kos.AppTheme.paletteText : Kos.AppTheme.disabledText
    palette.text: enabled ? Kos.AppTheme.paletteText : Kos.AppTheme.disabledText
    background: Rectangle {
        radius: 6
        color: root.down || root.checked ? Qt.alpha(Kos.AppTheme.paletteText, 0.13)
            : root.highlighted || root.hovered ? Qt.alpha(Kos.AppTheme.paletteText, 0.07)
            : "transparent"
    }
}
