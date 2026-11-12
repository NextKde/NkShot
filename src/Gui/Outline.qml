/* SPDX-FileCopyrightText: 2022 Noah Davis <noahadvs@gmail.com>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

import QtQuick
import QtQuick.Shapes
import org.kde.kirigami as Kirigami
import org.kde.spectacle.private

Shape {
    id: root
    property alias strokeWidth: shapePath.strokeWidth
    // The stroke color beneath the dash
    property alias strokeColor: shapePath.strokeColor
    property alias strokeStyle: shapePath.strokeStyle
    property alias capStyle: shapePath.capStyle
    property alias joinStyle: shapePath.joinStyle
    property alias svgPath: pathSvg.path
    property alias pathScale: shapePath.scale
    property alias pathHints: shapePath.pathHints
    property real cornerRadius: 0

    // Get a rectangular SVG path
    function rectanglePath(x, y, w, h) {
        // absolute start at top-left,
        // relative line to top-right,
        // relative line to bottom-right
        // relative line to bottom-left
        // close path (automatic line to top-left)
        return `M ${x},${y}
                l ${w},0
                l 0,${h}
                l ${-w},0
                z`
    }

    function roundedRectanglePath(x, y, w, h, radius) {
        const r = Math.max(0, Math.min(radius, w / 2, h / 2))
        if (r === 0) {
            return rectanglePath(x, y, w, h)
        }
        return `M ${x + r},${y}
                l ${w - 2 * r},0
                A ${r},${r} 0 0 1 ${x + w},${y + r}
                l 0,${h - 2 * r}
                A ${r},${r} 0 0 1 ${x + w - r},${y + h}
                l ${2 * r - w},0
                A ${r},${r} 0 0 1 ${x},${y + h - r}
                l 0,${2 * r - h}
                A ${r},${r} 0 0 1 ${x + r},${y}
                z`
    }

    // Get a matrix4x4 that moves the stroke outside the bounds of the path
    function outerStrokeScaleValue(originalValue, strokeWidth = root.strokeWidth) {
        return QmlUtils.ratio(originalValue + strokeWidth * 2, originalValue + strokeWidth)
    }

    // Get a matrix4x4 that moves the stroke outside the bounds of the path
    function outerStrokeTranslateValue(originalValue, scale, strokeWidth = root.strokeWidth) {
        return QmlUtils.unTranslateScale(originalValue, scale) - strokeWidth / 2
    }

    preferredRendererType: Shape.CurveRenderer

    ShapePath {
        id: shapePath
        fillColor: "transparent"
        // ensure outline is always thick enough to be visible, but grows with zoom
        strokeWidth: dprRound(1)
        strokeColor: palette.highlight
        // Solid line because it's easier to do the alternating color effect this way.
        strokeStyle: ShapePath.SolidLine
        joinStyle: ShapePath.MiterJoin
        PathSvg {
            id: pathSvg
            path: root.cornerRadius > 0
                ? roundedRectanglePath(strokeWidth / 2, strokeWidth / 2,
                                       width - strokeWidth, height - strokeWidth, root.cornerRadius)
                : rectanglePath(strokeWidth / 2, strokeWidth / 2,
                                width - strokeWidth, height - strokeWidth)
        }
    }
}
