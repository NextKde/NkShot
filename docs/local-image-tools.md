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

## Quick-note cards (066)

More → Pins and notes → Quick note opens an independent editor. Plain text is
rendered locally into a 720-pixel-wide opaque image, with Paper, Blue or Night
colors. Chinese, wrapped text and literal markup are supported without treating
input as HTML. Copy image writes only after an explicit click. Pin note creates an
immutable snapshot; Save As in its toolbar exports the PNG through the existing
pin workflow. Editing the note never changes an existing pin or screenshot.

The draft survives closing/reopening the note window within this process, but is
not saved across application restarts. The window says this explicitly. There is
no cloud sync, notes database or automatic clipboard/disk write. Empty notes and
notes beyond 2,000 UTF-16 units or 2,400 rendered pixels in height cannot export.
`Imaging/NoteCard` is a Qt Gui-only renderer reusable outside this UI.

Starting a screenshot or recording hides the draft window; reopen it explicitly
from Pins and notes afterward. Scrolling selection restores its prior visibility
through the existing selection cleanup. This prevents intentional draft display
during setup; actual compositor exclusion timing still needs native KDE testing.

## Read QR and barcodes (067)

More → Read from image → Read QR or barcode scans an immutable snapshot of
the current edited image locally. It displays one decoded textual result in the
shared read-only result window. Copy and UTF-8 Save As are explicit actions; URLs,
HTML and commands stay plain text and are never opened or executed by this flow.
The existing automatic scan preference remains a separate upstream path.

Empty, undecodable and binary-only codes get useful guidance without modifying
the clipboard. The helper rejects images over 20 megapixels and composites alpha
onto white for recognition. A repeated request is ignored while decoding; a hidden
viewer, changed image or switch to video discards the obsolete result. Multiple
codes are not enumerated, and damaged-code accuracy is not guaranteed.


## Sample image colors (068)

More → Pick a color → Screenshot opens a focused sampler for an immutable
snapshot of the edited image. Click or drag to select an image pixel; arrow keys
move one physical image pixel. Tab returns to the image for keyboard navigation.
The 11×11 neighborhood is enlarged without interpolation, with the selected pixel
at its center. The image and its undo history never change.

Values are 8-bit, straight-alpha sRGB. Tagged images are converted to sRGB;
untagged images are assumed to already contain sRGB values. Alpha is preserved:
HEX uses #RRGGBB or CSS #RRGGBBAA (alpha last), while RGB uses rgb() or rgba().
The checkerboard is only a transparency preview, never part of the sampled color.
Coordinates are zero-based physical image pixels, irrespective of source DPR.
Images above 20 megapixels are rejected with a crop/resize suggestion.

Copy HEX (or Ctrl+C) and Copy RGB are explicit clipboard actions; selecting or
closing does not copy anything. A repeat open raises the existing sampler.
Closing destroys its snapshot; the next open reads the current edited image.
Hiding the viewer closes the sampler before capture setup. This is local image
sampling, not a global desktop eyedropper, monitor-profile accuracy, HDR sampling
or native high-DPI/GPU acceptance.

## Read a selected area in a pin (069)

The existing Extract text icon now offers **Read whole image** and **Select an
area…**. Drag across the image, then choose **Read area**. The selection reports
physical image-pixel dimensions. Recognition starts only on that explicit action;
selecting, dragging and Cancel never recognize or copy. Escape cancels area mode
before it can close the pin. Zoom shortcuts stay available, and the selected bounds
remain attached to source pixels when zoom changes. The stable contextual footer replaces zoom buttons while selecting. Area drawing
is mouse-only; keyboard rectangle creation is not implemented. Panning is suspended only
while selecting; cancel restores normal reading. Hiding the pin clears selection.

In image and area modes, Copy, Ctrl+C and PNG Save As use the complete immutable
pin, including while drawing a rectangle. The editor and undo history are untouched. Regions are rounded
outward to physical pixels, clamped to the source bounds and rejected below two
pixels in either dimension. Source DPR does not alter crop coordinates. Area OCR
uses the same bounded local Tesseract helper as whole-image recognition. Hidden whole-image
results are matched to both language and exact crop before reuse. Hidden area
results are discarded with guidance to select again, rather than showing a popup
or later substituting area text for a whole-image request. Closing cancels
work and suppresses late result windows. No screenshot data is transmitted.

This is rectangle-only OCR, not inline recognized-word highlighting, word-level
selection, a destructive crop or a native KDE/GPU/high-DPI acceptance claim.


## Select recognized words on a pin (070)

Extract text → Select text on image recognizes locally and underlines recognized
words in the original pin. Click a word, drag between words, or Shift-click to
extend a contiguous range in OCR reading order. The compact footer previews the
selected plain text and provides Copy text and Done. It replaces the zoom-button
footer rather than adding another permanent toolbar. Merely recognizing or
selecting never changes the clipboard, pixels or editing history.

Focus the image (click or Tab) to navigate: Left/Up select the previous logical
word, Right/Down the next, Home/End the first/last, and Shift extends the range.
Ctrl+A selects all recognized words. In this mode only, Ctrl+C copies selected
text; with no selection it does nothing. The top Copy image button and PNG Save
As still use the entire original image. Done or Escape leaves word mode and
restores Ctrl+C image copy. Another Escape closes the pin.

Zoom shortcuts and Ctrl+wheel remain available. Ordinary mouse-wheel or trackpad
scrolling moves the image while word dragging selects text. Keyboard navigation
reveals the active word; oversized boxes are centered in the viewport. Word boxes
stay attached to physical source pixels across zoom, pan and image DPR. Since 071,
keep the primary mouse button held near a viewport edge to scroll and extend the
same anchored range across the image. The edge band shrinks in small windows;
scrolling accelerates with proximity to the edge, capped at 480 logical pixels
per second on each axis. Returning to the middle, releasing (also outside the
viewport), leaving the mode, hiding/minimizing or closing stops scrolling.
A drag endpoint in whitespace chooses the vertically nearest OCR line and then
the nearest word horizontally; an initial blank click does not start a range.
Character-level selection and geometric up/down-line navigation are not implemented. Chinese selection follows Tesseract token boundaries, not an
assumption that every character is a separate word.

Recognition retains Tesseract's logical word order and its exact line spacing.
Complete metadata must reconstruct the existing full-text result, so ambiguous
alignment is rejected rather than shown as partial success. Selectable metadata
is limited to 2,000 words, 1,024 UTF-16 units per word and 128 Ki UTF-16 units in
total. Existing image/OCR bounds also apply. If word alignment is unavailable,
use Read whole image or Select an area; the ordinary text reader remains usable.
Recognized markup, commands and URLs are literal text, never interpreted.

Cancel, hide, minimize and close invalidate asynchronous word requests. A late
completion cannot reopen the mode after hiding and showing the pin. This is local
word-range selection, not full system Live Text parity, OCR accuracy certification,
native screen capture, touch/trackpad acceptance or GPU/monitor-DPI validation.

The separate **Screen** source (074) requests the desktop portal rather than
sampling this image. See [screen color picking and native acceptance limits](screen-color-picker.md).
