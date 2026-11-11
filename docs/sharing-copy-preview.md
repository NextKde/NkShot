# Preview background and watermark before creating a copy (106)

More → Sharing → Background and watermark now opens a rendered preview of the
current edited screenshot. Choose Light/Dark/Aurora, padding, rounded corners and
optional watermark while seeing the result. Create copy remains explicit; Cancel
and Escape do not create files, pins or history entries.

The wide layout separates the preview canvas from a small controls column. Narrow
windows use a single scrollable column while Cancel/Create copy stay fixed. The
last valid preview remains visible during the short typing debounce rather than
flashing a blank loading panel.

## Reusable, non-destructive contract

`PresentationPreview` owns one immutable image snapshot for the popup session,
independent of SpectacleCore. Color conversion to sRGB happens once. Both preview
and export use the same `ImageTransforms` composition code and original physical
pixel coordinates, including padding, corners, shadow, footer and text elision.
The preview renders directly to at most 1024 pixels on its longest edge; it does
not allocate a new full-resolution output on every keystroke. It is a reduced
resolution visual aid, not a promise of identical antialiasing after scaling.

Create synchronously flushes pending parameter changes, captures that same source
and option set, then runs the existing full-resolution copy worker. Closing the
popup releases the preview; it cannot alter the already accepted output job.
Original image pixels and annotation history are not changed. Reopening captures
the then-current document. No network service or translation is involved.

Full output keeps the existing bounds (8192 pixels wide, 32000 high, 40 million
pixels, padding 0–240, watermark at most 120 characters) and original DPR. The
preview uses DPR 1 for UI display. Invalid options clear the actionable preview;
there is no silently stale export or automatic source replacement.

## Acceptance boundary

Software-window interaction and synthetic image fixtures are not native KDE,
Wayland, real capture, monitor-profile or GPU acceptance. This change does not add
AI background removal, arbitrary photo filters, a new capture engine or a photo
library. Shared renderer/session APIs are reusable, but not a separate installed
photo-editor library.

## Reviewed application windows

Actual Qt software-window captures, using a synthetic screenshot fixture:

- [Light rendered preview](screenshots/106-sharing-preview-light.png)
- [Dark rendered preview](screenshots/106-sharing-preview-dark.png)
- [420px preview](screenshots/106-sharing-preview-compact.png)
- [420px controls after scrolling](screenshots/106-sharing-options-compact.png)

Visual review tightened the wide dialog from 580 to 480 pixels high to remove
excess empty space; the narrow layout retains reachable controls and a fixed
footer. Preview and watermark controls are intentionally separate scroll positions
on the smallest window rather than compressing everything into unreadable controls.

Final verification: C++/QML build passed; full CTest 28/28; identity/static checks
31/31. Real light/dark/420px popup interactions, repeat choices, rendered provider
pixels, cancel/reopen, full-resolution pin, clipboard/PNG export and source/history
isolation passed. The maximum-width invalid-padding recovery and existing
size/orientation sharing workflow also passed. Preview105 is a separately rebuilt
binary checkpoint from source105 and does not include this source106 feature.
