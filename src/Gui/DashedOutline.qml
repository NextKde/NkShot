/* SPDX-FileCopyrightText: 2022 Noah Davis <noahadvs@gmail.com>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

import QtQuick
import QtQuick.Shapes
import org.kde.kirigami as Kirigami
import org.kde.spectacle.private

Outline {
    id: root
    property alias dashColor: dashPath.strokeColor
    property alias dashCapStyle: dashPath.capStyle
    property alias dashJoinStyle: dashPath.joinStyle
    // dashPattern is a list of alternating dash and space lengths.
    // Length in logical pixels is length * strokeWidth,
    // so divide by strokeWidth if you want to set length in logical pixels.
    property alias dashPattern: dashPath.dashPattern
    property alias dashOffset: dashPath.dashOffset
    property alias dashSvgPath: dashPathSvg.path
    property alias dashPathScale: dashPath.scale
    property alias dashPathHints: dashPath.pathHints

    // A regular alternative pattern with a spacing in logical pixels
    function regularDashPattern(spacing, strokeWidth = root.strokeWidth) {
        return [spacing / strokeWidth, spacing / strokeWidth]
    }

    ShapePath {
        id: dashPath
        fillColor: "transparent"
        strokeWidth: root.strokeWidth
        strokeColor: palette.base
        strokeStyle: ShapePath.DashLine
        // A near-zero dash becomes a dot with RoundCap. Reserve two stroke
        // widths in the space for the caps to keep the visible dots separate.
        dashPattern: [0.01, Kirigami.Units.smallSpacing / root.strokeWidth + 2]
        dashOffset: 0
        capStyle: ShapePath.RoundCap
        joinStyle: root.joinStyle
        scale: root.pathScale
        pathHints: root.pathHints
        PathSvg {
            id: dashPathSvg
            path: root.svgPath
        }
    }
}
