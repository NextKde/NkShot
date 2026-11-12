/* SPDX-FileCopyrightText: 2026 NextKde
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import QtQuick.Templates as T
import org.kde.kirigami as Kirigami
import Kos.Icons as Symbols
import Kos.Ui as Kos
import "SymbolMap.js" as SymbolMap

// Menu item counterpart of SymbolLabel. Everything that makes a menu item a menu
// item -- the check indicator, the sub-menu arrow, the highlight background,
// mnemonics, keyboard navigation -- stays native; only the content is replaced so
// the icon comes from the pinned Kos.Icons font instead of the desktop icon
// theme.
//
// Derived from QQC.MenuItem rather than reimplementing it, so the style's own
// padding rules, indicator placement and Kirigami theme switching still apply.
QQC.MenuItem {
    id: root

    // Optional explicit glyph; leave empty to use icon.name or the Action's.
    property string symbol: ""

    // Size of the glyph column. Matches what the style uses for label-sized
    // icons, so menus keep their previous metrics.
    readonly property real glyphSize: Kirigami.Units.iconSizes.sizeForLabels

    // Glyph name, freedesktop spelling. Explicit `symbol` wins, then an Action's
    // own symbol, then icon.name, then the sub-menu's icon -- Spectacle declares
    // a sub-menu's icon on the Menu it opens (TaskMenu), not on the item.
    readonly property string glyphName: symbol.length > 0 ? symbol
        : (action && action.symbol !== undefined && action.symbol.length > 0 ? action.symbol
        : (icon.name.length > 0 ? icon.name
        : (subMenu && subMenu.icon && subMenu.icon.name !== undefined ? subMenu.icon.name : "")))

    readonly property bool hasGlyph: display !== T.AbstractButton.TextOnly
        && (glyphName.length > 0 || icon.source.toString().length > 0)

    // The style decides whether to reserve a glyph column by looking at
    // icon.name only, which misses items that were given a `symbol` instead.
    // Check both spellings, so a menu written either way still lines up. The
    // scan has to run again once the item actually joins its menu, hence the
    // explicit rescan rather than a plain binding.
    property bool menuHasGlyph: root.hasGlyph

    function rescanMenu() {
        const owner = root.menu
        if (!owner) {
            root.menuHasGlyph = root.hasGlyph
            return
        }
        for (let i = 0; i < owner.count; ++i) {
            const item = owner.itemAt(i)
            // Visibility is not a reliable filter here: the menu marks its items
            // invisible until it is shown, which is exactly when this scan runs.
            if (!item) {
                continue
            }
            const name = item.glyphName !== undefined ? item.glyphName : ""
            if (name.length > 0 || (item.icon && item.icon.name !== "")) {
                root.menuHasGlyph = true
                return
            }
        }
        root.menuHasGlyph = root.hasGlyph
    }

    // Deferred: at construction time the item has not joined its menu yet, so
    // the scan would see an empty menu. callLater runs once the whole menu has
    // been populated.
    Component.onCompleted: Qt.callLater(rescanMenu)

    Connections {
        target: root.menu
        ignoreUnknownSignals: true
        function onCountChanged() { root.rescanMenu() }
        function onAboutToShow() { root.rescanMenu() }
    }

    contentItem: Item {
        id: content

        // The style's MenuItem reads these three off its contentItem when
        // computing left/right padding. Replacing the item therefore has to keep
        // publishing them, or a checkable item would lose the room reserved for
        // its indicator.
        readonly property bool hasIcon: root.hasGlyph
        readonly property bool hasLabel: label.visible
        readonly property bool textBesideIcon: root.display === T.AbstractButton.TextBesideIcon

        // Room the style's indicator occupies inside the content area.
        readonly property real indicatorSpace: root.indicator && root.indicator.visible
            ? (root.indicator.width || root.indicator.implicitWidth) + root.spacing
            : 0

        // Menus line their labels up: once one item has a glyph, every item keeps
        // a glyph column so the text stays in one place.
        readonly property bool reserveGlyph: root.menuHasGlyph

        // Layout rather than hand-computed widths: binding a label width to the
        // control width closes a loop through implicitWidth, and the layout
        // engine already knows how to shrink the label when the menu is narrower
        // than its contents.
        implicitWidth: Math.ceil(indicatorSpace + layout.implicitWidth)
        implicitHeight: Math.ceil(layout.implicitHeight)

        RowLayout {
            id: layout
            x: content.indicatorSpace
            width: Math.max(0, content.width - content.indicatorSpace)
            height: content.height
            spacing: root.spacing

            Symbols.Icon {
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: root.glyphSize
                Layout.preferredHeight: root.glyphSize
                // Reserve the column even for items without a glyph, so a menu
                // that mixes both still lines its text up.
                visible: content.reserveGlyph
                opacity: content.hasIcon ? 1 : 0
                name: SymbolMap.glyph(root.glyphName)
                size: root.glyphSize
                color: !root.enabled ? Kos.AppTheme.disabledText
                    : root.icon.color.a > 0 ? root.icon.color : root.palette.windowText
            }

            // MnemonicData hands us markup with the underlines already in place,
            // so this has to stay a rich-text label rather than a plain Text.
            Text {
                id: label
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                visible: text.length > 0 && root.display !== T.AbstractButton.IconOnly
                text: root.Kirigami.MnemonicData.richTextLabel
                textFormat: Text.RichText
                font: root.font
                color: root.enabled ? root.palette.windowText : Kos.AppTheme.disabledText
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }

            Text {
                id: shortcut
                Layout.alignment: Qt.AlignVCenter
                visible: Application.styleHints.showShortcutsInContextMenus
                    && hasSequence
                    && root.display !== T.AbstractButton.IconOnly
                readonly property bool hasSequence: root.action
                    && root.action.shortcut !== undefined
                    && root.action.shortcut.toString().length > 0
                // nativeText renders the shortcut in the platform's own notation
                // ("Ctrl+Shift+S" here, "Strg+Umschalt+S" on a German desktop).
                text: visible ? shortcutResolver.nativeText : ""
                font: root.font
                color: root.enabled ? root.palette.windowText : Kos.AppTheme.disabledText
                opacity: root.enabled ? 0.7 : 1
                horizontalAlignment: Text.AlignRight
                verticalAlignment: Text.AlignVCenter

                Shortcut {
                    id: shortcutResolver
                    sequence: root.action && root.action.shortcut !== undefined
                        ? root.action.shortcut : ""
                }
            }
        }
    }
}
