# Reframe a video copy (095)

After choosing a crop aspect in Export a clip → Details, drag the retained
rectangle in the video preview to keep the part that matters. The selected aspect
and output dimensions stay fixed: this moves the crop rather than stretching,
resizing or modifying the original recording.

- Drag inside the white frame to reposition it. Playback pauses while adjusting.
- With the frame focused, arrow keys move two source pixels; Shift+Arrow moves ten.
- Home or the **Center** action restores the initial centered composition.
- Collapsing Details gives the preview more room; the frame stays interactive.

The helper remains reusable: it receives source/display/crop geometry and emits
explicit position/reset requests, with no player or document dependency. The
owning video view applies requests to its export job. The main screenshot editor,
recording capture, audio choices and all original Spectacle tools remain separate.

## Bounds, lifecycle and output

Positions are clamped to source bounds and aligned to even pixels for YUV420.
Dragging against an edge cannot expose pixels outside the source. No-op requests
do not clear an already completed export. During processing or a save dialog the
UI is locked, and the backend rejects changes while busy or not ready. The save
dialog also snapshots the entire crop rectangle, not merely the aspect name, so
an out-of-band position change cannot silently alter the pending export.

Switching MP4/GIF within the same session retains the position. Choosing another
aspect, opening a new clip session or inspecting a new source starts centered.
Both MP4 and GIF use the same resolved crop rectangle. The source file stays
unchanged; existing new-file-only output publication and verification remain.

This is adjustable **fixed-aspect framing**, not freeform resizing, tracking a
moving subject, animated pan/zoom or a live camera/capture overlay. Existing
square-pixel/support-format restrictions apply. Cropping does not remove audio.

## Evidence

Real MP4/WebM fixtures verify left/right/top/bottom crop pixels in MP4 and GIF,
not only output dimensions. Boundary/odd/extreme positions, no-ops, busy-state
rejection, format switching, aspect/session reset and Center are checked.
Actual Qt interactions cover independent repeated drags, arrow/Shift+Arrow/Home,
the Center action, stale-position Save rejection and final output dimensions.
Light/dark/narrow software windows are reviewed; native KDE/Wayland, touch, GPU
and mixed-DPI acceptance remain pending. No translation or external service is used.

Reviewed screenshots: [light](screenshots/095-reframe-light.png),
[dark](screenshots/095-reframe-dark.png),
[narrow](screenshots/095-reframe-compact.png),
[controls](screenshots/095-reframe-options.png). The actual UI tests also drag
again after collapsing Details, checking the resized preview's coordinate mapping.
