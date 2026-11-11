// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "VerticalStitcher.h"
namespace NkShot::Imaging {
struct ScrollAlignment {
    StitchError error = StitchError::NoOverlap;
    int overlap = 0;
    double confidence = 0;
    double runnerUpGap = 0;
};
// Conservative scrolling-only registration. Never modifies or blends either image.
// Limits: 8192 pixels per side, 16M pixels/frame. Metadata and size must match.
// Requires >=94% matching pixels AND textured anchors (per-channel tolerance 8),
// Texture means a >=12-level neighboring RGB edge. Searches at most 16,337
// signed shifts with at most 48x24 samples each, then one full-overlap check.
// Near-tied sampled candidates are fully checked only when <=8 candidates and
// <=32M combined overlap pixels; otherwise ambiguity is retained. Thresholds
// are identical for sampled and full evidence (no confidence relaxation).
// Requires six of nine cells over the occupied texture footprint, all three
// footprint rows/columns covered (>=3 horizontal and >=6 vertical search bins),
// and >=8 percentage points separation from the second-best signed displacement.
// A stationary textured header with nontrivial horizontal extent is rejected.
// Confidence is an internal agreement score, never a statistical probability.
// A localized changed island can fit within the 6% budget; it is not reconstructed.
// Only a unique positive downward-content displacement is accepted. The session
// must retain its accepted composite and append only next[overlap..height).
ScrollAlignment alignScrollFrames(const QImage &previous, const QImage &next, int minimumOverlap = 24);
}
