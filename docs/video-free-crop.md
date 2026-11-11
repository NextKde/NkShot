# Freeform video-copy cropping (112)

Open **Export a clip → Details → Frame → Free** to keep an arbitrary rectangular
part of a recording. Drag a corner to resize, or drag inside the white frame to
move it. Collapsing Details gives the image more room without ending the edit.
The preview and MP4/GIF export resolve the same source-pixel rectangle. The source
recording is never changed.

Free starts from the current preset frame, avoiding a jump in composition.
**Center** keeps the selected width and height and moves that frame to the center;
choose **Original frame** to restore the complete image. Existing ratio presets
still provide constrained framing. MP4/GIF switching retains the current crop.

With the frame focused, arrows move two source pixels and Shift+arrows move ten.
Ctrl+arrows resize the bottom-right edge; Ctrl+Shift+arrows resize by ten pixels.
Home centers the current size. These controls match the existing privacy-region
keyboard pattern and retain the preset movement shortcuts.

## Geometry and controls

The frame is bounded to the original square-pixel source, with even positions
and even dimensions (minimum 2 pixels) for YUV420 alignment. A corner drag keeps
the opposite corner fixed and cannot cross it. Resizing uses source coordinates,
not the scaled preview or selected output resolution. Tiny frames which cannot
produce a valid selected output size remain unavailable rather than stretching.

Frame and privacy-region editing are mutually exclusive. Processing and output
selection lock interaction; an out-of-band changed crop invalidates the pending
save through the existing full-rectangle snapshot check. The static privacy cover
and annotation overlay are applied before crop and scaling, matching the existing
pipeline. Cropping does not remove audio; that is a separate explicit option.

The overlay remains an injected-geometry component emitting position, rectangle
and center requests. It does not own the media player, file or document. Preset
behavior, screenshot tools and capture backend remain separate.

This is a fixed export rectangle, not motion tracking, animated reframing or a
live recording effect. Real KDE/KWin/GPU/device capture remains unaccepted.

## Verification and actual UI review

All 36 CTest targets passed (218.32 seconds), with 37 identity/static checks.
New focused tests passed with no skips: real MP4/WebM sources exported to MP4
and GIF, decoded source-coordinate pixels, privacy-over-annotation order, MP4
audio samples, unchanged source hashes and geometry/no-op/busy/reset boundaries.

Actual window interactions passed in light, dark, 420×520 and DPR 2 layouts: Free
selection, all four fixed-opposite-corner drags, interior movement, precise/fast
keyboard resize and movement, center, privacy exclusion, output-dialog/encoding
lock and rejection of a changed rectangle during Save. The old fixed-aspect
reframing flow passed separately. A WebM run also passed real MP4/GIF output with
privacy and annotation editing, decoded output and unchanged source checks.

Reviewed captures show a single contextual frame over the preview, not another
persistent tool panel. Free is an option inside existing Details; collapsing it
keeps one Export action and a short editing hint. The compact layout wraps that
hint while retaining preview, time controls and the primary action. These are
actual software-rendered synthetic fixtures, not native KDE/GPU acceptance.

- [Light](screenshots/112-free-crop-light.png)
- [Dark](screenshots/112-free-crop-dark.png)
- [Compact](screenshots/112-free-crop-compact.png)
