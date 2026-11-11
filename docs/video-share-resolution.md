# MP4 sharing dimensions (102)

Open a recording's **Export a clip → Details → Output size**:

- **Original** keeps the cropped pixel dimensions. This remains the default.
- **Max 1920 px** fits the longest edge within 1920 pixels.
- **Max 1280 px** fits the longest edge within 1280 pixels.

These are maximum-edge presets, not a promise that every recording is exactly
1920×1080 or 1280×720. Portrait recordings remain portrait. A smaller image is not
enlarged. Dimensions round inward to even pixels for H.264; normal aspect ratios
may differ by less than two pixels of rounding. The panel shows the exact resulting
width and height. Extremely thin frames that would collapse below two pixels are
rejected rather than silently stretched; choose Original or change the crop.

The original file and playback remain unchanged. Save creates a new MP4; resolution
selection is not playback zoom, bitrate control or a guarantee of file size. Audio
is retained or removed according to the existing audio checkbox; its duration and
pitch are not changed. The option is hidden for GIF, whose existing 640-pixel,
silent 10-fps export remains unchanged. Switching formats preserves the MP4 choice;
a new export inspection returns to Original.

## Composition and lifecycle

Processing remains time selection → transparent annotations → opaque privacy cover
→ crop → size reduction. The privacy cover therefore cannot be painted over by
annotations. Source coordinates used by the editor and crop preview do not change;
only the final file dimensions change. Existing full decode, duration, audio-track,
source identity and new-file checks remain active. Save dialogs snapshot resolution
alongside crop/privacy/annotation state; changing settings invalidates an old dialog.
Changing a preset clears obsolete results, and encoding blocks conflicting changes.

## Scope and verification

Backend tests encode MP4 and WebM sources, both presets, horizontal/portrait crops,
audio/silent output, transparent annotations and privacy overlays. They check decoded
pixels, dimensions, source hashes, format switching, no-upscale, size boundaries,
cancellation and recovery from an impossible thin-frame preset. Actual Qt workflows
check the control, precise output dimensions and stale Save rejection. Pixel limits
and encoding costs remain those of the existing local clip exporter.

This is local post-processing, not a new capture backend. Native KDE focus/dialogs,
real device recording, GPU playback/encoding and installation remain unaccepted.
The existing preview100 binary predates this source feature.

## Recorded checks

Build passed; final full CTest **27/27** (235.52 seconds), static **26/26**,
focused new encoding/boundary checks **7/7**. Actual light UI exports a
1600×900 source as 1280×720. The 420×520 WebM workflow verifies no enlargement,
Save-race protection, session reset and unchanged GIF export. Dark combines the
preset with moved portrait crop, static annotations, opaque cover and silent audio.

Details reserves a separate scrollbar gutter, so it does not overlap Clear or
other actions. Pointer tests wait until kinetic scrolling has stopped before
expecting button activation; this was verified through actual clicked signals.
No direct Clear call bypasses the UI. Expected missing-device/GPU warnings and
QML asynchronous teardown warnings remain; final workflows emitted no TypeError
or ReferenceError. All media shown below is synthetic, nonpersonal test input.

- [Light size options](screenshots/102-share-resolution-light.png)
- [1280×720 export summary](screenshots/102-share-output-light.png)
- [Dark combined workflow](screenshots/102-share-combined-dark.png)
- [420×520 options](screenshots/102-share-resolution-compact.png)
