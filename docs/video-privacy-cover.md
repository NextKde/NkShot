# Fixed video privacy covers (097, 117)

In a recording's **Export a clip → Details**, enable **Hide private regions**.
The details collapse to leave room for the preview. Drag the opaque black region
to move it and drag its four corners to resize. Arrow keys move by two source
pixels; Control+arrows resize from the bottom/right; Shift increases the step to
ten pixels. Details → Adjust covers switches back from framing to cover editing.

The cover is baked into the new MP4/GIF, before optional crop and scaling. It is
fixed in source coordinates for the **entire selected time range**. It does not
track moving people, text or windows. Review the entire exported video before
sharing, and cover a margin around sensitive content. It does not remove audio;
use the separate Include audio control if speech is private. The original file
remains unchanged and may still contain all sensitive information.

Up to eight opaque rectangular covers are supported, with square-pixel sources
and the same validated MP4/WebM scope as clip export. No blur, tracking, AI erase,
timed keyframes are implied. A fresh export session starts
with cover disabled; toggling within one session retains the regions. Format
switching keeps the chosen cover. The save dialog rejects changes made to the
coverage after it was opened, rather than exporting unexpected settings.

The injected `PrivacyRegionOverlay` controls geometry without depending on the
media job or global application document. It can be reused by future image tools.
Capture-selection and Spectacle annotation routes are unchanged.

## Verification and limits

Tests cover actual encoded pixels, output/crop combinations, bounds, cancellation
and source isolation. Actual Qt UI exercises toggle, drag, corner resize, keyboard,
stale Save settings and new-session reset. Native KDE capture, GPU playback and
real device audio/video remain unaccepted. This is local post-processing, not live
recording redaction or a guarantee that the user selected every private detail.

Actual software-rendered Qt evidence using synthetic media:
- [Light cover editing](screenshots/097-privacy-light.png)
- [Dark cover with crop](screenshots/097-privacy-dark.png)
- [420px compact editing](screenshots/097-privacy-compact.png)
- [Compact secondary settings](screenshots/097-privacy-details.png)

Secondary settings scroll when space is tight; the range and Export/Cancel stay
outside that scroll area, with a visible video preview. Moving to a source edge
preserves cover size instead of unexpectedly shrinking the protected region.

## Multiple private regions (117)

The existing Details entry expands into a compact contextual row only while
adjusting privacy. Choose a numbered cover, add another or remove the selection.
All covers remain opaque in the preview; only the selected cover shows editing
handles. For overlapping covers, use the selector to reach a covered region.
Adding or changing one cover never replaces earlier selected regions.

The last removal turns privacy off. Disabling privacy preserves the session's
rectangles, while starting a new source/export session clears them. Each region
is fixed for the entire selected clip, not motion-tracked or time-keyframed.
Audio is unaffected. Review all frames and use Include audio separately.

The backend validates at most eight bounded even-pixel rectangles and applies
all opaque covers after static annotations, before cropping and scaling. Its
legacy single-region accessor continues to address the first region; selection
is view state, not an export parameter. The save dialog records a coverage
revision, so changing any region after opening it cannot export stale settings.
The original recording and screenshot document remain unchanged.

Small covers keep a real centre drag area: corner hit targets shrink with the
on-screen rectangle instead of overlapping across it. Changing the selected
cover, hiding the overlay or changing its display geometry cancels an in-flight
gesture, rather than continuing to write to a different region. UI regression
checks include a 32 × 32 source-pixel cover and require movement without resizing.

### 117 review evidence

Actual Qt windows using a labelled synthetic local video (not desktop capture):
- [Light multiple-cover editor](screenshots/117-multiple-covers-light.png)
- [Dark editor with crop](screenshots/117-multiple-covers-dark.png)
- [420 px compact editor](screenshots/117-multiple-covers-compact.png)

The source-independent backend checks every frame's protected interiors in
MP4/WebM-to-MP4/GIF combinations, annotation precedence and crop interaction; a
short real MP4 export additionally checks all eight regions. Actual UI tests
exercise selection, both small and large centre dragging, adding/removing up to
the limit, a second-region stale-Save rejection and decoded output pixels.
The compact panel keeps Export/Cancel visible; only the selected region has
handles. All preview masks are opaque, not cosmetic translucent suggestions.
Software rendering does not establish native KDE, GPU or device acceptance.
