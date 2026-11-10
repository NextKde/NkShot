/*
 *  SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import org.kde.kquickimageeditor
import org.kde.spectacle.private

PortableAnnotationViewport {
    id: root

    // Inject the image model so a future photo editor can reuse this canvas.
    required property AnnotationDocument editorDocument
    document: editorDocument
    viewportRect: Qt.rect(0, 0, width, height)

    onPressedChanged: if (pressed) {
        if (textTool.shouldShow) {
            textTool.forceActiveFocus(Qt.MouseFocusReason);
        }
    }

    Item {
        x: -root.viewportRect.x
        y: -root.viewportRect.y
        transformOrigin: Item.TopLeft
        TextTool {
            id: textTool
            viewport: root
        }
        AnnotationSelectionTool {
            id: selectionTool
            viewport: root
        }
        HoverOutline {
            viewport: root
            hidden: selectionTool.hovered || selectionTool.dragging
        }
    }

    Shortcut {
        enabled: root.enabled
        sequences: [StandardKey.Undo]
        onActivated: root.document.undo()
    }
    Shortcut {
        enabled: root.enabled
        sequences: [StandardKey.Redo]
        onActivated: root.document.redo()
    }
}
