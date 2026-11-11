# Fixed video privacy cover (097)

In a recording's **Export a clip → Details**, enable **Hide a private region**.
The details collapse to leave room for the preview. Drag the opaque black region
to move it and drag its four corners to resize. Arrow keys move by two source
pixels; Control+arrows resize from the bottom/right; Shift increases the step to
ten pixels. Details → Adjust cover switches back from framing to cover editing.

The cover is baked into the new MP4/GIF, before optional crop and scaling. It is
fixed in source coordinates for the **entire selected time range**. It does not
track moving people, text or windows. Review the entire exported video before
sharing, and cover a margin around sensitive content. It does not remove audio;
use the separate Include audio control if speech is private. The original file
remains unchanged and may still contain all sensitive information.

This first version supports one opaque rectangular cover, square-pixel sources
and the same validated MP4/WebM scope as clip export. No blur, tracking, AI erase,
multiple regions or timed keyframes are implied. A fresh export session starts
with cover disabled; toggling within one session retains the region. Format
switching keeps the chosen cover. The save dialog rejects changes made to the
cover after it was opened, rather than exporting unexpected settings.

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
