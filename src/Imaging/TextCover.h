// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "LocalTextExtraction.h"
namespace NkShot::Imaging {
struct TextCoverMatch { QString text; QList<QRect> regions; };
struct TextCoverMatches {
    QList<TextCoverMatch> matches;
    bool valid = false;
    bool overflow = false;
};
// Literal, Unicode case-folded search with whitespace runs collapsed and trimmed.
// Matches advance by the query length (non-overlapping). A substring covers its
// complete OCR word(s), with two physical pixels of margin clipped to the image.
// The displayed whole-word label is bounded to 512 UTF-16 units. Regions are not
// truncated. More than 100 matches fails closed with overflow=true and no matches.
// Caller must separately require complete OCR metadata and review the whole copy:
// this helper cannot detect text that OCR missed or prove absence of private data.
TextCoverMatches findTextCoverMatches(const QList<TextWord> &words,
    const QSize &imageSize, const QString &query);
// Fresh opaque sRGB8 copy: white-composite source, then black physical-pixel
// rectangles. No text/EXIF-like metadata is copied; only sRGB and source DPR.
// Rejects empty/invalid/out-of-image rectangles, >2000 regions, and source limits.
// Original image, metadata and edit history are never modified.
QImage coveredTextCopy(const QImage &source, const QList<QRect> &regions);
}
