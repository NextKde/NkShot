/* SPDX-FileCopyrightText: 2023 Noah Davis <noahadvs@gmail.com>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls as QQC
import org.kde.spectacle.private

// Use Qt's context-menu request rather than a nonexistent TextInput pressed
// signal or a pointer handler competing with the control's built-in menu.
Item {
    id: root
    required property Item target
    Binding {
        target: root.target.QQC.ContextMenu
        property: "menu"
        value: null
    }
    Connections {
        target: root.target.QQC.ContextMenu
        function onRequested(position) {
            TextContextMenu.popup(root.target)
        }
    }
}
