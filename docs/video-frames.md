# Extract a video frame (088)

In video playback, open **Playback and clips → Extract current frame…**.
Playback pauses and the decoded frame opens as a separate, immutable image pin.
It uses the original decoded resolution, not a screenshot of the player with
letterboxing or controls. Copy, PNG Save As, zoom and local OCR reuse the existing
pinned-image workflow. No video bytes, image document or undo history are replaced.
Seek first to choose another moment; extracting again creates another snapshot.
This is not frame-accurate timeline stepping or a claim of HDR/color-managed
professional frame export. Animated GIF/WebP does not expose this action.

The action is unavailable during active capture/finalization and other media
transactions. The backend validates video mode, current source identity, a valid
sink/frame and a 40-megapixel bound. Image conversion failures show a recoverable
error. A pin remains unchanged when playback seeks or its source later changes.

## Verification scope

Actual Qt decoder and UI actions on generated MP4/WebM files exercise pause,
extraction, pixel-identical clipboard and PNG save, rejected missing/stale sources,
seek without mutating the snapshot, and continued clip export. UI captures are
software-rendered real windows containing synthetic media, not native captures.
Existing pin OCR validation applies to the shared image workflow, not certified
recognition accuracy for every video frame. GPU-to-CPU conversion, rotated/HDR
sources, native KDE focus, device recording and multi-monitor behavior remain
unverified. Offline translation is not part of this feature.

## Reviewed software-window evidence

- [Frame preview, light](screenshots/088-frame-light.png)
- [Frame preview, dark](screenshots/088-frame-dark.png)
- [Playback menu](screenshots/088-frame-menu.png)
- [Playback menu, 420-pixel viewer](screenshots/088-frame-menu-compact.png)

The pin chooses its own size independently of the main viewer. The compact
image above refers to the playback menu, not a compact pinned window.
