/* SPDX-FileCopyrightText: 2026 NextKde
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

import QtQuick
import QtQuick.Window
import QtQuick.Controls as QQC
import Kos.Ui as Kos
import org.kde.spectacle.private

QQC.Menu {
    id: root
    width: 240
    padding: 6
    popupType: QQC.Popup.Item
    palette.windowText: Kos.AppTheme.paletteText
    palette.text: Kos.AppTheme.paletteText
    palette.buttonText: Kos.AppTheme.paletteText
    palette.base: Kos.AppTheme.contentBackground
    palette.button: Kos.AppTheme.controlBackground
    delegate: GlassMenuItem {}
    // Rows own their neutral hover/selection fill; the style's list highlight is blue.
    Binding {
        target: root.contentItem
        property: "highlight"
        value: null
    }
    background: Kos.LiquidPanelSurface {
        cornerRadius: 14
        backdrop: Kos.LiquidStyle.backdropForItem(root.parent, root.parent ? root.parent.Window.window : null)
        reduceTransparency: !Settings.useLiquidMaterial
        tintColor: Kos.AppTheme.contentBackground
    }
}
