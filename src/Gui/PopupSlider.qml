// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import Kos.Ui as Kos

Kos.LiquidSlider {
    id: root
    implicitHeight: 28
    materialForm: true
    appearanceMode: Kos.AppTheme.dark ? "dark" : "light"
    accentColor: Kos.AppTheme.accent
    trackColor: Kos.AppTheme.controlBackground
    opacity: 1
    activeFocusOnTab: true
    on_PressedChanged: if (_pressed) forceActiveFocus(Qt.MouseFocusReason)
    Accessible.role: Accessible.Slider
    Accessible.focusable: enabled
    Accessible.focused: activeFocus
    function stepByKeyboard(steps) {
        const next = Math.max(0, Math.min(1, value + steps * wheelStep))
        previewChanged(next)
        commitRequested(next)
    }
    Accessible.onIncreaseAction: stepByKeyboard(1)
    Accessible.onDecreaseAction: stepByKeyboard(-1)
    Keys.onPressed: event => {
        if (event.key === Qt.Key_Right || event.key === Qt.Key_Up) stepByKeyboard(1)
        else if (event.key === Qt.Key_Left || event.key === Qt.Key_Down) stepByKeyboard(-1)
        else if (event.key === Qt.Key_Home || event.key === Qt.Key_End) {
            const next = event.key === Qt.Key_Home ? 0 : 1
            previewChanged(next)
            commitRequested(next)
        } else return
        event.accepted = true
    }
}
