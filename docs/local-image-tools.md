# Local image composition core

`NkShot::Imaging::stitchVertical` is an offline, Qt Gui-only core for a future
scrolling-capture workflow or photo tool. It does not capture a screen, scroll an
application, select files or send data anywhere. It is currently tested source,
not a user-facing automatic scrolling feature.

## Conservative contract

- Provide 2–16 frames in top-to-bottom order, with equal pixel width, device pixel
  ratio and color space. Heights may differ.
- Consecutive frames need a unique, pixel-identical suffix/prefix overlap of at
  least 24 rows by default. Row hashes locate candidates; full pixel comparison
  confirms them, so hash equality alone never authorizes a merge.
- Reject absent or ambiguous overlaps and no-progress duplicate frames. Do not
  guess across repeated whitespace, animation, fixed headers or horizontal shifts.
- The default output budget is 40 million pixels, with maximum width 8192 and
  height 32000. Reject incompatible/oversized input before output allocation.
- Preserve alpha, DPR and color-space metadata; returned image is independent
  of later changes to input images. No blended seams or silent resampling.

## Verified tests

`vertical_stitcher_test` reports 10 passed, no failures/skips. Three overlapping
segments reassemble into the exact original long-image pixels. Tests cover
no-progress, unrelated images, ambiguous whitespace, size/memory limits, changed
overlap pixels, incompatible color spaces, alpha and fractional DPR.

Future UI/capture integration must still establish frame order, explain rejected
sequences and obtain real compositor frames through permitted mechanisms. A
passed composition test is not a passed automatic scrolling screenshot.

## Application entry (041)

More → Combine screenshots loads 2–16 local images, ordered naturally by filename
(e.g. frame2 precedes frame10). Number frames top-to-bottom. Total input is capped
at 40 megapixels before decode. Work runs through QtConcurrent and the original
editor stays unchanged. A successful image opens in a separate pinned preview
with Copy/Save As; no input file is modified and no image is uploaded. Ambiguous,
duplicate, incompatible or insufficiently overlapping inputs are rejected.
This is not automated scrolling or fuzzy/perspective photo stitching.

### Vertical stacking without overlap

Use More → Stack images vertically for 2–16 local pictures that should retain
all their content. Number filenames in top-to-bottom order (for example 2.png,
10.png); numeric filename order, not click order, determines placement. Images
must share width, scale and color profile. No automatic scaling, cropping or
removal of repeated regions occurs. Transparent pixels are preserved.

This is an explicit alternative to Combine screenshots, which requires an exact,
unambiguous unchanged overlap. Neither mode captures or scrolls another app.
Both leave source files and the current editor untouched, opening a separate
preview for Copy or Save As. Limits remain 40 megapixels total input/output,
8192 pixels wide and 32000 pixels tall. Stacking can retain duplicated content;
use overlap matching instead when duplicates should be removed.
