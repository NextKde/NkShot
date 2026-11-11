# Find and cover text in a sharing copy (111)

In the independent image editor, choose **More → Find and cover text…**. NkShot
reads the current edited image locally using the configured OCR languages. Enter
literal text, choose **Find**, review the black preview regions and select the
matches to cover. **Create copy** produces a separate pinned image. Nothing is
saved, copied to the clipboard or uploaded automatically.

**OCR can miss or misread text. Review the whole copy before sharing.** This is
not automatic detection of all personal information, a claim that the screenshot
is safe, or a replacement for manually covering other private details. The search
can find only recognized words with valid aligned bounds. An unavailable language,
timeout, incomplete word metadata or missing match leaves Create unavailable.
The original editor, undo history and unsaved-change state remain unchanged.

## What is covered

Search is literal, Unicode case-folded, with whitespace collapsed; it is not a
regular expression, fuzzy correction or a personal-data classifier. A substring
match covers its entire OCR word, not individual characters. A phrase may cover
several word boxes, including words on different lines. Every selected word box
has a two-physical-pixel margin clipped to the image bounds. Matches in the same
word share a cover. OCR bounds and this margin are not a guarantee that every
visible glyph has been recognized correctly; inspect the resulting copy.

Queries are at most 256 UTF-16 units. Metadata is limited to 2000 words/128 Ki
UTF-16 units and results to 100 matches. An overflow clears the result and asks
for a narrower search rather than silently omitting further matches. Editing the
query clears previous matches immediately, so Create cannot use an old search.
Individual matches may be deselected; Cancel/Escape discards the session.

## Independent opaque output

`Imaging/TextCover` is a UI-independent matching and rendering helper. The source
is captured once after pending editor options are committed. Inputs are bounded
to 20 megapixels, 8192 wide and 32000 tall. Coordinates use physical image pixels,
not window pixels or logical DPR-scaled coordinates.

The renderer allocates a fresh opaque RGB32 image, converts the source to sRGB,
composites it on white, and fills selected regions with fully opaque black using
source composition. Thus the output has no transparent channel hiding the covered
pixels. Source text metadata is not copied; the output intentionally retains only
sRGB display information and the image DPR. It is a flattened sharing copy, not an
editable document containing the original pixels beneath removable cover objects.
The original source and its editing history remain in the original session and
must not be shared in place of the new copy.

The preview uses a bounded white-background source image and physical-coordinate
black overlays. Final output uses the original full-resolution snapshot and is
created off the GUI thread. Only the normal explicit Copy/Save actions on that
new pin transmit pixels to the clipboard or a selected file. OCR text/query and
matches are session-only; there is no query history or external OCR provider.
This is not a secure erasure guarantee for process memory or the original file.

## Cancellation and environment

OCR uses the existing local Tesseract worker budget and a 15-second recognition
deadline, not a hard wall-clock bound on engine initialization.
Cancellation is cooperative: closing invalidates the generation and drops late
results, while a still-finishing worker prevents another overlapping session.
No unsafe thread termination or automatic language installation is performed.
Copy/Save/Done/Compare and underlying editing are blocked while the review tool
or its confirmed output job owns the captured revision.

Native KDE portals, GPU/HDR/monitor color, device input and live recording remain
separate acceptance work. This feature does not change paused translation work.

## Prevent sharing the original by mistake

After a covered copy is created, the original editor displays **Original paused**.
Its Copy/Save shortcuts and direct export actions stay disabled even if the window
manager keeps focus on it. **Show covered copy** returns to the separate result;
closing the result does not silently unlock the original. **Return to original**
is the explicit way to resume editing the still-uncovered source. The result pin
keeps its normal Copy/Save workflow without an extra confirmation.

## Verification and visual review

All 36 CTest targets passed (220.27 s) and all 36 identity/static checks passed.
Actual software-window flows passed in light, dark and 420×520 compact layouts,
including a 20-match compact forward/reverse keyboard traversal. Pixel comparison
checks opaque covered output, normal result Copy and unchanged source/history.
Original-editor reactivation cannot copy/save until an explicit return; closing
the result does not remove the protection. The pre-existing image-edit/export,
comparison/navigation/lifetime and light/color workflows are separately regressed.

The UI was reviewed from actual rendered captures, not a design mockup: the
wide layout places the image beside search and individual matches, with one
primary Create action. Compact view scrolls its body while keeping the warning
and footer visible; the result list scrolls separately and keyboard focus is
kept on-screen. This uses existing NextKde surfaces, rounded controls and accent
hierarchy; software rendering is not acceptance of native GPU glass effects.

- [Light](screenshots/111-text-cover-light.png)
- [Dark](screenshots/111-text-cover-dark.png)
- [Compact](screenshots/111-text-cover-compact.png)
- [Original protection](screenshots/111-original-paused.png)
- [Covered result](screenshots/111-covered-copy.png)
