// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QImage>

namespace NkShot::Imaging {
struct AdjustmentOptions {
    int brightness = 0;
    int contrast = 0;
    int saturation = 0;
};
// Invalid inputs return a null image. Identity returns an independent exact copy,
// including profile/format. Otherwise output is straight-alpha sRGB8 RGBA8888.
// Untagged input is assumed sRGB. Per normalized encoded-sRGB channel:
//   c = clamp((c + brightness/100 - 0.5) * (1 + contrast/100) + 0.5)
// then y = .2126*r + .7152*g + .0722*b and
//   c = clamp(y + (c-y) * (1 + saturation/100)).
// Round final channels to nearest byte; retain alpha exactly. Transparent RGB
// may be zeroed. This is not a linear-light or HDR adjustment pipeline.
QImage adjustedCopy(const QImage &source, const AdjustmentOptions &adjustments = {});
}
