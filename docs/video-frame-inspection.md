# Exact local frame inspection (115)

**Playback and clips → Browse exact frames…** pauses playback and opens a separate
inspection window. Previous/Next and the left/right keys move through decoded
frames, not a guessed `1 / average FPS` offset. The initial frame is chosen from
the last indexed presentation timestamp at or before the player's current time;
that initial player-time mapping is approximate. Subsequent navigation uses the
actual presentation-order index.

The large Fit preview, frame number and timestamp describe the same verified
result. A new request immediately hides the old frame and disables Extract copy.
Changing direction while decoding keeps only the newest target. Extract copy
opens an independent immutable image pin, reusing Copy, PNG Save, local OCR and
the shared image-copy editor. The inspector remains open for another frame.
Closing it cancels its work. The original video bytes, playback position, clip
options, main image and edit history are not modified; playback stays paused.

## Bounded local processing

Only regular local H.264 MP4 / VP9 WebM sources are accepted, up to 256 MiB,
ten minutes and 36,000 decoded frames, with one SDR square-pixel video track, at most 4096 pixels per axis and
8.29 million pixels. Rotated/display-matrix sources and negative initial
timestamps are rejected.
This is not support for every file accepted by the separate clip exporter.
Missing tools, unsupported sources, repeated/missing/out-of-order timestamps,
changed source files and exceeded budgets produce an error, not a guessed frame.

The index retains integer presentation timestamps and the source time base.
It rejects duplicate timestamps rather than silently removing frames. Extraction
seeks to the nearest preceding indexed keyframe, preserves original timestamps
and preroll, and selects the unique target timestamp. The decoder's reported
integer timestamp/time base and exact output size must match before the image is
published. No player seek or GUI video-sink buffer serves as the accuracy oracle.

A single cancellable media process is active per session. Index and pixel outputs
are bounded; full-resolution frames are not cached for the entire recording.
File identity, size and modification metadata are checked across work. Those
checks are not an atomic guarantee against a same-user pathname race. The tool
performs no network access or automatic dependency installation.

Long keyframe intervals, large images and slow storage may still take noticeable
time. Timeouts stop work rather than leaving an unbounded decoder running.
Color output is SDR RGB, not a professional HDR or monitor-profile guarantee.

## Verification evidence

The command-contract prototype checks 54 target frames across five generated
fixtures: CFR H.264 with B frames, 29.97 fps, long GOP, nonzero initial timestamps,
and nonuniform VFR VP9. Every selected RGB24 frame matches an independent decode
from the beginning byte for byte, with the expected timestamp and time base.
The 320×180 prototype measured 0.096–0.311 seconds per extraction. Three additional 1080p long-GOP
targets matched exact pixels in 0.33–1.25 seconds; neither result is a
4K performance claim. Disabling accurate-seek trimming was necessary to retain
preroll for nonzero-start sources; that failure and corrected evidence were kept.

Native KDE capture, GPU decoding, device audio/video synchronization and desktop
permission flows are not accepted by these local software tests. Offline
translation remains paused.

### Application verification

Full build passed; CTest **39/39** (223.31 seconds), static checks **39/39**.
The new session tests cover true B-frame/VFR/offset pixels, indexed time,
latest-request wins, cancel/reopen, repeated timestamps, file/URL boundaries,
source mutation including same-index requests, and synchronous observer deletion
or replacement. An output-limit callback that immediately starts a new inspection
has a regression test: the retired completion cannot stop or process the new job.

Actual light/dark/420-wide windows verify menu entry, continuous keyboard and
button navigation, first/last bounds, disabled busy-state extraction, independent
Copy/PNG save, and original timeline/source/edit/clip isolation. Owner hiding and
source retirement close the inspector. Screenshots show actual Qt windows with
explicitly synthetic local media, not native KDE recordings.

- [Light inspector](screenshots/115-frame-inspection-light.png)
- [Dark inspector](screenshots/115-frame-inspection-dark.png)
- [Compact inspector](screenshots/115-frame-inspection-compact.png)
- [Irregular VFR frame verification](screenshots/115-frame-inspection-vfr.png)

The previous current-frame extraction and clip-export workflow also passes in
the 420-pixel viewer after adding the new entry. The dedicated demonstration
screenshots use a 3-second, 36-frame local video visibly labeled Synthetic demo;
its frames go through the same real decoder and pixel-oracle checks.
