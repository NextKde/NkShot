# Editable static video annotations (099)

Open a recording's **Export a clip → Details → Add annotations…**. The shared
image-copy editor opens on the current decoded frame. Draw arrows, text, freehand
marks, highlights, lines, shapes or numbers; use the same style controls, zoom and
Undo/Redo as image editing. **Done**, closing the editor or Escape keeps the marks
for this export and returns to the clip preview. Reopening **Edit annotations…**
returns to the same document and editable history. **Clear** removes that export's
annotation session. The original video and main image document stay unchanged.

The reference frame is fixed when this session starts. Marks remain at fixed source
coordinates for the **entire selected clip**, not just the displayed frame. Preview
other times in the clip after Done. This is not live recording drawing, motion
tracking, subtitles, timed keyframes or click/camera effects. Cancelling/closing the
clip panel or beginning a fresh export ends the draft annotation session; an
exported file is flattened and is not an editable video project.

Only the transparent annotation layer is encoded, never the reference screenshot.
The editor preserves its existing image mode and complete Spectacle image toolset.
Video mode deliberately excludes crop, blur and pixelation: image-dependent effects
would freeze old frame pixels rather than safely process the moving video. Use the
separate fixed opaque **privacy cover** for redaction, and review the entire output.
It does not remove private speech; audio inclusion is a separate choice.

## Export and reuse contract

- Validated square-pixel MP4/WebM sources; explicit local FFmpeg overlay support.
- Static alpha composition works for MP4 and both GIF palette/encoding passes.
- Order: time selection → annotations → opaque privacy cover → crop/scale.
  Annotations cannot paint reference pixels back over an enabled privacy cover.
- Same-size premultiplied image copied into the job; no caller mutation after Done.
- Private temporary PNG, passed as a separate media input, not interpolated into
  a filter path. It is removed after completion, failure or cancellation.
- Save snapshots the annotation revision along with source, format, audio and crop;
  changed settings invalidate an already open save dialog.
- `ImageCopyEditor` remains independent of the global screenshot document.
  `ImageOverlayPreview` is a reusable injected image/rectangle renderer without
  media ownership, usable by future image tools.

## Verification and limits

Generated MP4/WebM exports check every frame's solid/transparent/half-alpha pixels,
privacy precedence, crop combinations, original hashes, state gates and temporary
file cleanup. Actual Qt interaction checks editor drawing, close/Done/reopen,
editable history, empty Undo, invalid image-transform refusal, Clear, stale Save
and decoded output. Ordinary image-copy editor regression also passed in the dark combined workflow.

Final build passed; CTest **27/27** (143.37 seconds), static checks **24/24**.
Actual software-rendered Qt workflows passed in light, dark and 420×520 windows.
Dark combines annotations with moved portrait crop, privacy cover and silent MP4;
WebM workflows also export GIF. These screenshots use synthetic test media:

- [Light shared editor](screenshots/099-annotations-editor-light.png)
- [Dark combined export](screenshots/099-annotations-export-dark.png)
- [Compact shared editor](screenshots/099-annotations-editor-compact.png)
- [Compact export](screenshots/099-annotations-export-compact.png)

Compact checks scroll controls fully into view and use the visible scrollbar when
the software test backend does not deliver mouse-wheel scrolling. Expected absent
GPU/audio-device warnings and asynchronous QML teardown warnings remain; no
ReferenceError/TypeError appeared in the final dark/compact workflows.

Native KDE focus, GPU playback, real screen/device A/V and distribution installation
remain separate acceptance gates. Source-level software windows are not native
capture certification. The current downloadable preview097 predates this feature;
099 is initially a source update, not a claim of a newly built binary package.
