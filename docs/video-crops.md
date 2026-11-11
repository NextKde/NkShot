# Center-cropped recording copies (094)

Playback → Export a clip → Details now offers **Center crop**: Original frame,
1:1, 9:16 or 16:9. The default is the entire frame. A dimmed preview outside a
white boundary shows what will be removed, and the collapsed export summary
retains the crop's pixel dimensions. Crop controls are secondary to the time
range, playback preview and primary Export action rather than a new toolbar.

The selection applies to both MP4 and GIF. Cropping happens after time-range
processing and before GIF downscaling/palette generation. There is no stretching
or upscaling to force an aspect ratio. Dimensions/offsets are rounded inward to
even pixels for chroma alignment, with up to one pixel of center bias; very small
sources cannot represent every exact ratio. GIF keeps its existing maximum
640-pixel side, 10 fps, silent looping and size/duration bounds.

The preview guide is display-only and takes source dimensions, crop coordinates
and the rendered image rectangle as inputs. It does not change the player,
source file or original image editor. Software display uses its fitted image
rectangle; GPU display uses VideoOutput.contentRect. Native GPU/Wayland behavior
still needs acceptance. This is center-preset cropping, **not** arbitrary freeform
crop selection, auto-follow, camera overlay or a live capture mode.

## Source and export safety

The existing local-source, supported-format, time-range, output ownership,
new-file-only publication, decoding and audio validation gates remain. The saved
file's dimensions must match the chosen crop. Closing/reopening the clip panel
starts with Original frame; changing MP4/GIF within a session keeps the selection.
Busy export rejects crop changes. A save dialog snapshots the crop setting and
will not export after that setting changes behind it. Nothing replaces the source.
Cropping does not remove audio; use the separate audio option when needed.

Aspect presets require square-pixel video. An explicit non-square sample aspect
ratio disables crop but does not remove the existing Original frame export;
its sample aspect ratio is retained. HDR/rotation/unusual source restrictions
remain those of the existing local media pipeline.

## Evidence

Real generated MP4/WebM sources cover all four presets, MP4/GIF dimensions,
spatial gradient sampling to distinguish cropping from squeezing, audio tracks,
source hashes, invalid/busy choices, format switching and new-session reset.
A non-square-pixel fixture checks the crop gate and original-frame SAR retention.
Actual Qt UI covers the preset selector, visible crop geometry, normal/narrow
layout, real save/export dimensions, stale-save rejection and reopening.

Software-window screenshots use non-personal generated media and are not native
KDE recording acceptance. Real desktop A/V, portals, GPU and mixed-DPI remain
unverified. Offline translation is not part of this change.

Reviewed screenshots: [light preview](screenshots/094-crop-light.png),
[dark preview](screenshots/094-crop-dark.png),
[narrow preview](screenshots/094-crop-compact.png), and
[crop options](screenshots/094-crop-options.png). Expanded options preserve the
preview and Cancel action at 420 × 520; collapsing them gives the video more room.

## Adjustable framing (095)

The presets now also support [dragging and keyboard positioning](video-reframing.md).
Center remains the default and can be restored; arbitrary crop resizing is not
implemented. The Details label is now Frame because positioning is no longer
restricted to the center.
