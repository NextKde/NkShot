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

### Resize a sharing copy

More → Resize a copy accepts a physical pixel width and calculates proportional
height. Create copy snapshots the current image including visible annotations,
scales it off the UI thread and opens a separate preview for Copy/Save As.
Original files, current editing pixels and undo history are not replaced. The
result is a flattened raster copy, not a second editable annotation document.

The pure ImageTransforms helper is independent of capture/UI. It preserves image
alpha, color-space metadata and device-pixel ratio, rounds height to the nearest
row and rejects output beyond 8192×32000 bounds or 40 megapixels. This is ordinary
smooth scaling, not detail reconstruction or AI upscaling.

### Solid cover preset

Editor More tools → Solid cover preset configures the existing rectangle tool
with an opaque black fill, no stroke and no shadow. It deselects existing marks
first, so choosing the preset does not recolor a selected annotation. This is a
style preset, not a separate destructive erase or automatic sensitive-data finder.

Draw past every edge of the content you want covered, then inspect and export a
flattened copy before sharing. Antialiased boundaries need a safety margin. The
editable document and original files still retain the original pixels/undo history;
this does not erase originals, backups, metadata or copies elsewhere. Changing
the rectangle's fill later can make it translucent again. Pixelation/blur remain
available but should not be assumed to make all sensitive content unrecoverable.

### Background styling and text watermark

More → Share a copy → Background & watermark opens three local presets (Light, Dark, Aurora),
padding in physical pixels, optional single-line watermark and rounded-corner
choice. Create copy produces an independent preview with Copy/Save As. The
current annotated image is flattened into that copy; originals/history are not
replaced. A subtle shadow is included when padding permits. Rounded corners can
hide pixels at the image corners, so disable that option when every corner matters.

The reusable Qt ImageTransforms compositor outputs an opaque sRGB canvas,
converting tagged input to sRGB and treating untagged input as sRGB. Transparent
input is composited onto the chosen background. Output retains source DPR metadata
while geometry is painted in physical pixels. Watermark input is limited to 120
UTF-16 units, whitespace is normalized and text is elided if it cannot fit.
This is basic local styling/text attribution, not an AI edit, logo watermark,
custom uploaded background or a promise of complete 1Capture parity. No input is
sent to an external service. Existing output-size limits still apply.

The dialog shows output dimensions and disables Create when canvas limits would
be exceeded. Reduce padding to recover without losing entered options. More is
grouped by task: sharing, extracting text and combining images; recording mode
only exposes recording-file actions rather than irrelevant image tools.

## Read text from a pinned image (060)

Use **Extract text** in a pinned image's toolbar. Recognition runs on the immutable
pin, using the OCR languages selected in Options. The result opens separately for
selection, Copy all or UTF-8 Save As. Nothing is copied automatically, and the
editor, pin pixels, edit history and language preferences remain unchanged.
Recognition is local; installed Tesseract models are required. Missing models and
empty results leave an explanatory message in the pin instead of opening an empty
window. This is a whole-image text view, not inline word selection or highlighting.

The UI prevents repeat jobs for one pin; the shared helper allows at most two
concurrent jobs. Inputs are limited to 20 million pixels, 8192 pixels wide and
32000 pixels high, with up to four configured language codes. Recognition has a
15-second cooperative deadline; this is not a hard deadline for loading language
models. Closing the pin requests cancellation and suppresses late result windows.
When a pin is hidden/minimized, completed text is held locally until the user
chooses Extract text again. It does not interrupt a capture selector with a popup.
Changing the selected languages discards that pending result and re-recognizes.

The UI-independent `Imaging/LocalTextExtraction` helper owns a separate Tesseract
instance per request; it does not use the editor OCR singleton or its clipboard
behavior. Real English/Chinese fixture recognition and Qt offscreen interactions
are tested. General recognition quality, native window focus and accessibility
still require real desktop acceptance.

## Read large pinned images (061)

Pinned images start in Fit mode. The compact footer provides Fit, Width (fit width), zoom out,
percentage/100% and zoom in; the image can be dragged or scrolled when it is larger
than the viewport. Ctrl+wheel zooms around the pointer, while the buttons zoom
around the current view's center. Ctrl+0 restores Fit, Ctrl+1 selects 100%, and
standard zoom shortcuts are supported. At 100%, one source pixel occupies one Qt
logical unit; this is not a claim of physical 1:1 rendering on every desktop scale.
Width starts at the top and fits a long image horizontally for reading.
Zoom is capped at 400%. Fit and Width track window resizing and never enlarges small images.

This is view-only: OCR, Copy and Save As still use the entire immutable original
snapshot, not the visible or scaled region. The footer is outside the image rather
than an overlay hiding the bottom of a long screenshot. Mouse/keyboard/wheel and
window resizing were checked in software Qt windows; native high-DPI, touch and
trackpad behavior still require real desktop testing. Pinch zoom is not implemented.
